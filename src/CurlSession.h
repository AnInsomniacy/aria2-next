/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_CURL_SESSION_H
#define D_CURL_SESSION_H

#include "common.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "transport/CurlMulti.h"
#include "RangePlanner.h"
#include "StreamStore.h"
#include "error_code.h"

namespace aria2 {

class Command;
class CurlDownload;
class DownloadEngine;
class Option;
class RequestGroup;
struct CurlHandle;


class CurlSession {
public:
  explicit CurlSession(const Option* option);
  ~CurlSession();

  std::unique_ptr<Command> start(const std::shared_ptr<CurlDownload>& download,
                                 RequestGroup* group, DownloadEngine* engine);
  size_t activeCount() const { return tasks_.size(); }
  void setGlobalDownloadLimit(int64_t limit);
  void setExternalDownloadCount(size_t count);
  void poll();
  void armTimeout() { transport_.armTimeout(); }
  void advance(const std::shared_ptr<CurlDownload>& download);
  void stop(const std::shared_ptr<CurlDownload>& download, bool retainState);
  void discardRecovery(const std::shared_ptr<CurlDownload>& download);
  void restorePaused(const std::shared_ptr<CurlDownload>& download,
                     RequestGroup* group);

private:
  CurlMulti transport_;
  const Option* option_;
  DownloadEngine* engine_ = nullptr;
  int64_t globalDownloadLimit_ = 0;
  size_t externalDownloadCount_ = 0;
  long connectionPoolLimit_ = 0;
  StreamStore store_;
  std::map<CURL*, std::pair<std::shared_ptr<CurlDownload>, CurlHandle*>>
      downloads_;
  std::map<CurlDownload*, std::shared_ptr<CurlDownload>> tasks_;
  uint64_t loggingRevision_ = 0;

  static int effectiveStreamMaxConnections(const Option* option);
  bool prepare(const std::shared_ptr<CurlDownload>& download,
               RequestGroup* group);
  void activate(const std::shared_ptr<CurlDownload>& download);
  bool createHandle(const std::shared_ptr<CurlDownload>& download,
                    RangeLease lease, bool primary, bool ranged,
                    long addressFamily = CURL_IPRESOLVE_WHATEVER);
  void finish(const std::shared_ptr<CurlDownload>& download, CurlHandle* handle,
              CURLcode result);
  bool checkpoint(const std::shared_ptr<CurlDownload>& download, bool force);
  void configurePlanner(const std::shared_ptr<CurlDownload>& download,
                        const RangeLease* retainedLease = nullptr);
  void schedule(const std::shared_ptr<CurlDownload>& download);
  bool rebalanceEndgame(const std::shared_ptr<CurlDownload>& download,
                        int64_t pieceLength);
  std::optional<std::chrono::milliseconds>
  retryRange(const std::shared_ptr<CurlDownload>& download,
             const RangeLease& lease, curl_off_t retryAfter);
  void penalizeConnectionLimit(const std::shared_ptr<CurlDownload>& download,
                               uint64_t requestEpoch);
  void rewardConnectionLimit(const std::shared_ptr<CurlDownload>& download);
  std::vector<RangeLease>
  activeLeases(const std::shared_ptr<CurlDownload>& download) const;
  void finalize(const std::shared_ptr<CurlDownload>& download,
                curl_off_t reportedFileTime);
  void failTask(const std::shared_ptr<CurlDownload>& download,
                error_code::Value errorCode, const std::string& message,
                bool retainState = true);
  void cancelHandles(const std::shared_ptr<CurlDownload>& download);
  void restartFullDownload(const std::shared_ptr<CurlDownload>& download,
                           const char* reason = "range_ignored");
  static bool openOutput(CurlDownload* download, bool preserveExisting,
                         bool truncateOwned = false);
  static bool resolveOutput(CurlDownload* download, CURL* easy);
  void closeOutput(CurlDownload* download) noexcept;
  static bool retryableFailure(CURLcode result, long responseCode,
                               int fileNotFoundCount, int maxFileNotFound,
                               bool validatedRange, bool applicationConnected);

  void rebalanceLimits();
  // Lifts CURL_WRITEFUNC_PAUSE on handles whose offloaded writer has drained.
  void resumeFromBacklog();
  bool refreshConnectionPoolLimits();
  void eraseTask(CurlDownload* download);
  void
  refreshConnectionCount(const std::shared_ptr<CurlDownload>& download) const;
  void processMessages();
  static int socketOptionCallback(void* userData, curl_socket_t socket,
                                  curlsocktype purpose) noexcept;

  friend class CurlSessionTest;
  friend struct CurlHandle;
};

} // namespace aria2

#endif // D_CURL_SESSION_H
