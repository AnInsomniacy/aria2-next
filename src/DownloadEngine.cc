/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "DownloadEngine.h"
#include "CheckIntegrityMan.h"
#include "DNSCache.h"
#include "FileAllocationMan.h"
#include "TimerA2.h"
#include "a2netcompat.h"
#include "a2time.h"
#include "common.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include <signal.h>

#include <algorithm>
#include <iterator>
#include <typeinfo>

#include "StatCalc.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "DownloadResult.h"
#include "Log.h"
#include "fmt.h"
#include "SocketCore.h"
#include "support/Random.h"
#include "a2functional.h"
#include "prefs.h"
#include "EventPoll.h"
#include "Command.h"
#include "CurlSession.h"
#include "SystemResolver.h"
#include "FileAllocationEntry.h"
#include "CheckIntegrityEntry.h"
#include "DownloadContext.h"
#include "wallclock.h"
#ifdef ENABLE_BITTORRENT
#  include "BtSession.h"
#endif // ENABLE_BITTORRENT
#ifdef ENABLE_WEBSOCKET
#  include "WebSocketSessionMan.h"
#endif // ENABLE_WEBSOCKET
#include "Option.h"
#include "util_security.h"

namespace aria2 {

namespace global {

// 0 ... running
// 1 ... stop signal detected
// 2 ... stop signal processed by DownloadEngine
// 3 ... 2nd stop signal(force shutdown) detected
// 4 ... 2nd stop signal processed by DownloadEngine
// 5 ... main loop exited
volatile sig_atomic_t globalHaltRequested = 0;

} // namespace global

namespace {
constexpr auto DEFAULT_REFRESH_INTERVAL = 1_s;
// RPC, curl and timers share this thread; any step above this budget delays
// every client and transfer, so it is reported with the responsible step.
constexpr auto STALL_THRESHOLD = std::chrono::milliseconds(500);

template <class F> void timed(const char* step, F&& work)
{
  const auto start = std::chrono::steady_clock::now();
  work();
  const auto elapsed = std::chrono::steady_clock::now() - start;
  if (elapsed >= STALL_THRESHOLD) {
    A2_LOG_WARN(fmt("component=engine event=loop_stall step=%s duration_ms=%lld",
                    step,
                    static_cast<long long>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            elapsed)
                            .count())));
  }
}
} // namespace

DownloadEngine::DownloadEngine(std::unique_ptr<EventPoll> eventPoll)
    : eventPoll_(std::move(eventPoll)),
      haltRequested_(0),
      noWait_(true),
      refreshInterval_(DEFAULT_REFRESH_INTERVAL),
      lastRefresh_(Timer::zero()),
      dnsCache_(make_unique<DNSCache>()),
      systemResolver_(make_unique<SystemResolver>()),
      option_(nullptr)
{
  unsigned char sessionId[20];
  util::generateRandomKey(sessionId);
  sessionId_.assign(&sessionId[0], &sessionId[sizeof(sessionId)]);
}

DownloadEngine::~DownloadEngine()
{
  // Native socket callbacks still refer to commands owned by this engine.
  // Unregister them while both commands and request groups remain alive.
  curlSession_.reset();
  commands_.clear();
  routineCommands_.clear();
#ifdef ENABLE_BITTORRENT
  btSession_.reset();
#endif // ENABLE_BITTORRENT
  requestGroupMan_.reset();
}

namespace {
void executeCommand(std::deque<std::unique_ptr<Command>>& commands,
                    Command::STATUS statusFilter)
{
  size_t max = commands.size();
  for (size_t i = 0; i < max; ++i) {
    auto com = std::move(commands.front());
    commands.pop_front();
    if (!com->statusMatch(statusFilter)) {
      com->clearIOEvents();
      commands.push_back(std::move(com));
      continue;
    }
    com->transitStatus();
    bool done = false;
    const auto& command = *com;
    timed(typeid(command).name(), [&] { done = com->execute(); });
    if (done) {
      com.reset();
    }
    else {
      com->clearIOEvents();
      com.release();
    }
  }
}
} // namespace

namespace {
class GlobalHaltRequestedFinalizer {
public:
  GlobalHaltRequestedFinalizer(bool oneshot) : oneshot_(oneshot) {}
  ~GlobalHaltRequestedFinalizer()
  {
    if (!oneshot_) {
      global::globalHaltRequested = 5;
    }
  }

private:
  bool oneshot_;
};
} // namespace

int DownloadEngine::run(bool oneshot)
{
  GlobalHaltRequestedFinalizer ghrf(oneshot);
  while (!commands_.empty() || !routineCommands_.empty()) {
    if (!commands_.empty()) {
      waitData();
    }
    noWait_ = false;
    global::wallclock().reset();
#ifdef ENABLE_BITTORRENT
    if (btSession_) {
      timed("bt_poll", [this] { btSession_->poll(); });
    }
#endif
    rebalanceGlobalDownloadLimit();
    if (curlSession_) {
      timed("curl_poll", [this] { curlSession_->poll(); });
    }
    if (systemResolver_->poll()) {
      setNoWait(true);
      setRefreshInterval(std::chrono::milliseconds(0));
    }
    timed("statistics", [this] { calculateStatistics(); });
    if (lastRefresh_.difference(global::wallclock()) + A2_DELTA_MILLIS >=
        refreshInterval_) {
      refreshInterval_ = DEFAULT_REFRESH_INTERVAL;
      lastRefresh_ = global::wallclock();
      executeCommand(commands_, Command::STATUS_ALL);
    }
    else {
      executeCommand(commands_, Command::STATUS_ACTIVE);
    }
    executeCommand(routineCommands_, Command::STATUS_ALL);
    afterEachIteration();
    if (systemResolver_->hasPending()) {
      refreshInterval_ =
          std::min(refreshInterval_, std::chrono::milliseconds(25));
    }
    if (!noWait_ && oneshot) {
      return 1;
    }
  }
  onEndOfRun();
  return 0;
}

void DownloadEngine::rebalanceGlobalDownloadLimit()
{
  if (!option_) {
    return;
  }
  const auto overall = option_->getAsInt(PREF_MAX_OVERALL_DOWNLOAD_LIMIT);
  auto curlLimit = overall;
  auto btLimit = overall;
#ifdef ENABLE_BITTORRENT
  const auto btActive = btSession_ && btSession_->downloadSpeed() > 0;
#else
  const auto btActive = false;
#endif
  const auto curlActive = curlSession_ && curlSession_->activeCount() > 0;
  if (overall > 0 && btActive && curlActive) {
    curlLimit = std::max(1, overall / 2);
    btLimit = std::max(1, overall - curlLimit);
  }
  if (curlSession_) {
    curlSession_->setGlobalDownloadLimit(curlLimit);
  }
#ifdef ENABLE_BITTORRENT
  if (btSession_) {
    btSession_->setGlobalDownloadLimit(btLimit);
  }
#endif
}

void DownloadEngine::waitData()
{
  struct timeval tv;
  if (noWait_) {
    tv.tv_sec = tv.tv_usec = 0;
  }
  else {
    auto t =
        std::chrono::duration_cast<std::chrono::microseconds>(refreshInterval_);
    tv.tv_sec = t.count() / 1000000;
    tv.tv_usec = t.count() % 1000000;
  }
  eventPoll_->poll(tv);
}

bool DownloadEngine::addSocketForReadCheck(
    const std::shared_ptr<SocketCore>& socket, Command* command)
{
  return eventPoll_->addEvents(socket->getSockfd(), command,
                               EventPoll::EVENT_READ);
}

bool DownloadEngine::deleteSocketForReadCheck(
    const std::shared_ptr<SocketCore>& socket, Command* command)
{
  return eventPoll_->deleteEvents(socket->getSockfd(), command,
                                  EventPoll::EVENT_READ);
}

bool DownloadEngine::addSocketForWriteCheck(
    const std::shared_ptr<SocketCore>& socket, Command* command)
{
  return eventPoll_->addEvents(socket->getSockfd(), command,
                               EventPoll::EVENT_WRITE);
}

bool DownloadEngine::deleteSocketForWriteCheck(
    const std::shared_ptr<SocketCore>& socket, Command* command)
{
  return eventPoll_->deleteEvents(socket->getSockfd(), command,
                                  EventPoll::EVENT_WRITE);
}

bool DownloadEngine::addSocketForReadCheck(sock_t socket, Command* command)
{
  return eventPoll_->addEvents(socket, command, EventPoll::EVENT_READ);
}

bool DownloadEngine::deleteSocketForReadCheck(sock_t socket, Command* command)
{
  return eventPoll_->deleteEvents(socket, command, EventPoll::EVENT_READ);
}

bool DownloadEngine::addSocketForWriteCheck(sock_t socket, Command* command)
{
  return eventPoll_->addEvents(socket, command, EventPoll::EVENT_WRITE);
}

bool DownloadEngine::deleteSocketForWriteCheck(sock_t socket, Command* command)
{
  return eventPoll_->deleteEvents(socket, command, EventPoll::EVENT_WRITE);
}

void DownloadEngine::calculateStatistics()
{
  if (statCalc_) {
    statCalc_->calculateStat(this);
  }
}

void DownloadEngine::onEndOfRun()
{
  requestGroupMan_->removeStoppedGroup(this);
  requestGroupMan_->closeFile();
  requestGroupMan_->checkpointActiveDownloads();
}

void DownloadEngine::afterEachIteration()
{
  if (global::globalHaltRequested == 1) {
    A2_LOG_INFO(_("Shutdown sequence commencing..."
                  " Press Ctrl-C again for emergency shutdown."));
    requestHalt();
    global::globalHaltRequested = 2;
    setNoWait(true);
    setRefreshInterval(std::chrono::milliseconds(0));
    return;
  }

  if (global::globalHaltRequested == 3) {
    A2_LOG_INFO(_("Emergency shutdown sequence commencing..."));
    requestForceHalt();
    global::globalHaltRequested = 4;
    setNoWait(true);
    setRefreshInterval(std::chrono::milliseconds(0));
    return;
  }
}

void DownloadEngine::requestHalt()
{
  haltRequested_ = std::max(haltRequested_, 1);
  requestGroupMan_->halt();
}

void DownloadEngine::requestForceHalt()
{
  haltRequested_ = std::max(haltRequested_, 2);
  requestGroupMan_->forceHalt();
}

void DownloadEngine::setStatCalc(std::unique_ptr<StatCalc> statCalc)
{
  statCalc_ = std::move(statCalc);
}

void DownloadEngine::setOption(Option* option)
{
  option_ = option;
  if (option_ && !curlSession_) {
    curlSession_ = make_unique<CurlSession>(option_);
  }
}

void DownloadEngine::setCurlSession(std::unique_ptr<CurlSession> session)
{
  curlSession_ = std::move(session);
}

void DownloadEngine::setNoWait(bool b) { noWait_ = b; }

void DownloadEngine::addRoutineCommand(std::unique_ptr<Command> command)
{
  routineCommands_.push_back(std::move(command));
}

cuid_t DownloadEngine::newCUID() { return cuidCounter_.newID(); }

const std::string&
DownloadEngine::findCachedIPAddress(const std::string& hostname,
                                    uint16_t port) const
{
  return dnsCache_->find(hostname, port);
}

void DownloadEngine::cacheIPAddress(const std::string& hostname,
                                    const std::string& ipaddr, uint16_t port)
{
  dnsCache_->put(hostname, ipaddr, port);
}

void DownloadEngine::markBadIPAddress(const std::string& hostname,
                                      const std::string& ipaddr, uint16_t port)
{
  dnsCache_->markBad(hostname, ipaddr, port);
}

void DownloadEngine::removeCachedIPAddress(const std::string& hostname,
                                           uint16_t port)
{
  dnsCache_->remove(hostname, port);
}

void DownloadEngine::setRefreshInterval(std::chrono::milliseconds interval)
{
  // Timer producers may shorten this iteration's wait, never postpone a
  // deadline already requested by another transfer or protocol.
  refreshInterval_ = std::min(refreshInterval_, interval);
}

void DownloadEngine::addCommand(std::vector<std::unique_ptr<Command>> commands)
{
  commands_.insert(commands_.end(),
                   std::make_move_iterator(std::begin(commands)),
                   std::make_move_iterator(std::end(commands)));
}

void DownloadEngine::addCommand(std::unique_ptr<Command> command)
{
  commands_.push_back(std::move(command));
}

void DownloadEngine::setRequestGroupMan(std::unique_ptr<RequestGroupMan> rgman)
{
  requestGroupMan_ = std::move(rgman);
}

#ifdef ENABLE_BITTORRENT
void DownloadEngine::setBtSession(std::unique_ptr<BtSession> session)
{
  btSession_ = std::move(session);
}
#endif // ENABLE_BITTORRENT

void DownloadEngine::setFileAllocationMan(
    std::unique_ptr<FileAllocationMan> faman)
{
  fileAllocationMan_ = std::move(faman);
}

void DownloadEngine::setCheckIntegrityMan(
    std::unique_ptr<CheckIntegrityMan> ciman)
{
  checkIntegrityMan_ = std::move(ciman);
}

#ifdef ENABLE_WEBSOCKET
void DownloadEngine::setWebSocketSessionMan(
    std::unique_ptr<rpc::WebSocketSessionMan> wsman)
{
  webSocketSessionMan_ = std::move(wsman);
}
#endif // ENABLE_WEBSOCKET

bool DownloadEngine::validateToken(const std::string& token)
{
  using namespace util::security;

  if (!option_->defined(PREF_RPC_SECRET)) {
    return true;
  }

  if (!tokenHMAC_) {
    tokenHMAC_ = HMAC::createRandom();
    if (!tokenHMAC_) {
      A2_LOG_ERROR("Failed to create HMAC");
      return false;
    }
    tokenExpected_ = make_unique<HMACResult>(
        tokenHMAC_->getResult(option_->get(PREF_RPC_SECRET)));
  }

  return *tokenExpected_ == tokenHMAC_->getResult(token);
}

} // namespace aria2
