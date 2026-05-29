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
#include "Log.h"
#include "DownloadEngine.h"

#include <signal.h>

#include <cstring>
#include <cerrno>
#include <algorithm>
#include <numeric>
#include <iterator>

#include "StatCalc.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "DownloadResult.h"
#include "StatCalc.h"
#include "util.h"
#include "a2functional.h"
#include "DlAbortEx.h"
#include "EventPoll.h"
#include "Command.h"
#include "FileAllocationEntry.h"
#include "CheckIntegrityEntry.h"
#include "ProgressInfoFile.h"
#include "DownloadContext.h"
#include "SocketCore.h"
#include "fmt.h"
#include "wallclock.h"
#ifdef ENABLE_BITTORRENT
#  include "LibtorrentSession.h"
#endif // ENABLE_BITTORRENT
#ifdef ENABLE_WEBSOCKET
#  include "WebSocketSessionMan.h"
#endif // ENABLE_WEBSOCKET
#include "Option.h"
#include "RpcBeastServer.h"
#include "util_security.h"
#include "AsioRuntime.h"
#include "CurlSession.h"

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
} // namespace

DownloadEngine::DownloadEngine(std::unique_ptr<EventPoll> eventPoll)
    : eventPoll_(std::move(eventPoll)),
      runtime_(make_unique<AsioRuntime>()),
      curlSession_(make_unique<CurlSession>(this)),
      haltRequested_(0),
      noWait_(true),
      refreshInterval_(DEFAULT_REFRESH_INTERVAL),
      lastRefresh_(Timer::zero()),
      dnsCache_(make_unique<DNSCache>()),
      option_(nullptr)
{
  unsigned char sessionId[20];
  util::generateRandomKey(sessionId);
  sessionId_.assign(&sessionId[0], &sessionId[sizeof(sessionId)]);
}

DownloadEngine::~DownloadEngine() {}

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
    if (com->execute()) {
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
    calculateStatistics();
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
    if (!noWait_ && oneshot) {
      return 1;
    }
  }
  onEndOfRun();
  return 0;
}

void DownloadEngine::waitData()
{
  drainRuntime();
  const auto runtimeWakeRequested = runtime_->consumeWakeRequest();
  struct timeval tv;
  if (noWait_ || runtimeWakeRequested) {
    tv.tv_sec = tv.tv_usec = 0;
  }
  else {
    auto t =
        std::chrono::duration_cast<std::chrono::microseconds>(refreshInterval_);
    tv.tv_sec = t.count() / 1000000;
    tv.tv_usec = t.count() % 1000000;
  }
  eventPoll_->poll(tv);
  drainRuntime();
}

void DownloadEngine::drainRuntime()
{
  runtime_->runReady();
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

bool DownloadEngine::addRawSocketCheck(sock_t socket, Command* command,
                                       int events)
{
  auto mappedEvents = static_cast<EventPoll::EventType>(events);
  return eventPoll_->addEvents(socket, command, mappedEvents);
}

bool DownloadEngine::deleteRawSocketCheck(sock_t socket, Command* command,
                                          int events)
{
  auto mappedEvents = static_cast<EventPoll::EventType>(events);
  return eventPoll_->deleteEvents(socket, command, mappedEvents);
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
  requestGroupMan_->save();
}

void DownloadEngine::afterEachIteration()
{
  if (global::globalHaltRequested == 1) {
    ARIA2_LOG_INFO(_("Shutdown sequence commencing..."
                    " Press Ctrl-C again for emergency shutdown."));
    requestHalt();
    global::globalHaltRequested = 2;
    setNoWait(true);
    setRefreshInterval(std::chrono::milliseconds(0));
    return;
  }

  if (global::globalHaltRequested == 3) {
    ARIA2_LOG_INFO(_("Emergency shutdown sequence commencing..."));
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
  wakeRuntime();
}

void DownloadEngine::requestForceHalt()
{
  haltRequested_ = std::max(haltRequested_, 2);
  requestGroupMan_->forceHalt();
  wakeRuntime();
}

void DownloadEngine::setStatCalc(std::unique_ptr<StatCalc> statCalc)
{
  statCalc_ = std::move(statCalc);
}


void DownloadEngine::setNoWait(bool b) { noWait_ = b; }

AsioRuntime& DownloadEngine::getRuntime() { return *runtime_; }

CurlSession& DownloadEngine::getCurlSession() { return *curlSession_; }

void DownloadEngine::wakeRuntime()
{
  runtime_->wake();
  setNoWait(true);
  setRefreshInterval(std::chrono::milliseconds(0));
}

void DownloadEngine::scheduleRuntimeWake(std::chrono::milliseconds delay)
{
  runtime_->scheduleWake(delay);
  setRefreshInterval(delay);
}

void DownloadEngine::addRpcServer(std::shared_ptr<RpcBeastServer> server)
{
  rpcServers_.push_back(std::move(server));
}

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
  refreshInterval_ = std::move(interval);
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

#ifdef ENABLE_BITTORRENT
LibtorrentSession& DownloadEngine::getLibtorrentSession()
{
  if (!libtorrentSession_) {
    libtorrentSession_ = make_unique<LibtorrentSession>(option_);
  }
  return *libtorrentSession_;
}
#endif // ENABLE_BITTORRENT

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
      ARIA2_LOG_ERROR("Failed to create HMAC");
      return false;
    }
    tokenExpected_ = make_unique<HMACResult>(
        tokenHMAC_->getResult(option_->get(PREF_RPC_SECRET)));
  }

  return *tokenExpected_ == tokenHMAC_->getResult(token);
}

} // namespace aria2
