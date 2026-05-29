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
#ifndef D_DOWNLOAD_ENGINE_H
#define D_DOWNLOAD_ENGINE_H

#include "common.h"

#include <string>
#include <deque>
#include <map>
#include <vector>
#include <memory>

#include "a2netcompat.h"
#include "TimerA2.h"
#include "a2io.h"
#include "CUIDCounter.h"
#include "FileAllocationMan.h"
#include "CheckIntegrityMan.h"
#include "DNSCache.h"

namespace aria2 {

class Option;
class RequestGroupMan;
class StatCalc;
class SocketCore;
class Request;
class EventPoll;
class Command;
class AsioRuntime;
class CurlSession;
class RpcBeastServer;
#ifdef ENABLE_BITTORRENT
class LibtorrentSession;
#endif // ENABLE_BITTORRENT
#ifdef ENABLE_WEBSOCKET
namespace rpc {
class WebSocketSessionMan;
} // namespace rpc
#endif // ENABLE_WEBSOCKET

namespace util {
namespace security {
class HMAC;
class HMACResult;
} // namespace security
} // namespace util

class DownloadEngine {
private:
  void waitData();

  void drainRuntime();

  std::string sessionId_;

  std::unique_ptr<EventPoll> eventPoll_;

  std::unique_ptr<AsioRuntime> runtime_;

  std::unique_ptr<CurlSession> curlSession_;

  std::vector<std::shared_ptr<RpcBeastServer>> rpcServers_;

  std::unique_ptr<StatCalc> statCalc_;

  int haltRequested_;

  bool noWait_;

  std::chrono::milliseconds refreshInterval_;
  Timer lastRefresh_;

#ifdef ENABLE_BITTORRENT
  std::unique_ptr<LibtorrentSession> libtorrentSession_;
#endif // ENABLE_BITTORRENT

  CUIDCounter cuidCounter_;

  std::unique_ptr<DNSCache> dnsCache_;

#ifdef ENABLE_WEBSOCKET
  std::unique_ptr<rpc::WebSocketSessionMan> webSocketSessionMan_;
#endif // ENABLE_WEBSOCKET

  /**
   * Delegates to StatCalc
   */
  void calculateStatistics();

  void onEndOfRun();

  void afterEachIteration();

  std::unique_ptr<RequestGroupMan> requestGroupMan_;
  std::unique_ptr<FileAllocationMan> fileAllocationMan_;
  std::unique_ptr<CheckIntegrityMan> checkIntegrityMan_;
  Option* option_;
  // Ensure that Commands are cleaned up before requestGroupMan_ is
  // deleted.
  std::deque<std::unique_ptr<Command>> routineCommands_;
  std::deque<std::unique_ptr<Command>> commands_;

  std::unique_ptr<util::security::HMAC> tokenHMAC_;
  std::unique_ptr<util::security::HMACResult> tokenExpected_;
  uint16_t ed2kTcpPort_ = 0;
  bool ed2kTcpListenActive_ = false;

public:
  DownloadEngine(std::unique_ptr<EventPoll> eventPoll);

  ~DownloadEngine();

  // If oneshot is true, this function returns after one event polling
  // and performing action for them. This function returns 1 when
  // oneshot is true and there are still downloads to be
  // processed. Otherwise, returns 0.
  int run(bool oneshot = false);

  bool addSocketForReadCheck(const std::shared_ptr<SocketCore>& socket,
                             Command* command);
  bool deleteSocketForReadCheck(const std::shared_ptr<SocketCore>& socket,
                                Command* command);
  bool addSocketForWriteCheck(const std::shared_ptr<SocketCore>& socket,
                              Command* command);
  bool deleteSocketForWriteCheck(const std::shared_ptr<SocketCore>& socket,
                                 Command* command);
  bool addRawSocketCheck(sock_t socket, Command* command, int events);
  bool deleteRawSocketCheck(sock_t socket, Command* command, int events);


  void addCommand(std::vector<std::unique_ptr<Command>> commands);

  void addCommand(std::unique_ptr<Command> command);

  const std::unique_ptr<RequestGroupMan>& getRequestGroupMan() const
  {
    return requestGroupMan_;
  }

  void setRequestGroupMan(std::unique_ptr<RequestGroupMan> rgman);

  const std::unique_ptr<FileAllocationMan>& getFileAllocationMan() const
  {
    return fileAllocationMan_;
  }

  void setFileAllocationMan(std::unique_ptr<FileAllocationMan> faman);

  const std::unique_ptr<CheckIntegrityMan>& getCheckIntegrityMan() const
  {
    return checkIntegrityMan_;
  }

  void setCheckIntegrityMan(std::unique_ptr<CheckIntegrityMan> ciman);

  Option* getOption() const { return option_; }

  void setOption(Option* op) { option_ = op; }

  uint16_t getEd2kTcpPort() const { return ed2kTcpPort_; }

  void setEd2kTcpPort(uint16_t port) { ed2kTcpPort_ = port; }

  bool isEd2kTcpListenActive() const { return ed2kTcpListenActive_; }

  void setEd2kTcpListenActive(bool active) { ed2kTcpListenActive_ = active; }

  void setStatCalc(std::unique_ptr<StatCalc> statCalc);

  bool isHaltRequested() const { return haltRequested_; }

  bool isForceHaltRequested() const { return haltRequested_ >= 2; }

  void requestHalt();

  void requestForceHalt();

  void setNoWait(bool b);

  AsioRuntime& getRuntime();

  CurlSession& getCurlSession();

  void wakeRuntime();

  void scheduleRuntimeWake(std::chrono::milliseconds delay);

  void addRpcServer(std::shared_ptr<RpcBeastServer> server);

  void addRoutineCommand(std::unique_ptr<Command> command);

#ifdef ENABLE_BITTORRENT
  LibtorrentSession& getLibtorrentSession();
  LibtorrentSession* getInitializedLibtorrentSession() const
  {
    return libtorrentSession_.get();
  }
#endif // ENABLE_BITTORRENT

  cuid_t newCUID();

  const std::string& findCachedIPAddress(const std::string& hostname,
                                         uint16_t port) const;

  template <typename OutputIterator>
  void findAllCachedIPAddresses(OutputIterator out, const std::string& hostname,
                                uint16_t port) const
  {
    dnsCache_->findAll(out, hostname, port);
  }

  void cacheIPAddress(const std::string& hostname, const std::string& ipaddr,
                      uint16_t port);

  void markBadIPAddress(const std::string& hostname, const std::string& ipaddr,
                        uint16_t port);

  void removeCachedIPAddress(const std::string& hostname, uint16_t port);

  void setRefreshInterval(std::chrono::milliseconds interval);

  const std::string getSessionId() const { return sessionId_; }

#ifdef ENABLE_WEBSOCKET
  void setWebSocketSessionMan(std::unique_ptr<rpc::WebSocketSessionMan> wsman);
  const std::unique_ptr<rpc::WebSocketSessionMan>&
  getWebSocketSessionMan() const
  {
    return webSocketSessionMan_;
  }
#endif // ENABLE_WEBSOCKET

  bool validateToken(const std::string& token);
};

} // namespace aria2

#endif // D_DOWNLOAD_ENGINE_H
