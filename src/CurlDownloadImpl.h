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
#ifndef D_CURL_DOWNLOAD_IMPL_H
#define D_CURL_DOWNLOAD_IMPL_H

#include "common.h"

#include <array>
#include <algorithm>

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

#include "stream/CurlHandle.h"
#include "DiskWriter.h"
#include "RangePlanner.h"
#include "SpeedCalc.h"
#include "TimerA2.h"
#include "stream/IoQueue.h"

namespace aria2 {

class RequestGroup;
class CurlDownload;

struct CurlEndpoint {
  static long alternateFamily(long family)
  {
    return family == CURL_IPRESOLVE_V4 ? CURL_IPRESOLVE_V6 : CURL_IPRESOLVE_V4;
  }

  std::string uri;
  uint64_t generation = 0;
  bool resolving = false;
  std::chrono::steady_clock::time_point readyAt{};
  bool unavailable = false;
};

struct CurlDownloadImpl {
  void eraseHandle(CurlHandle* handle)
  {
    if (plannerConfigured && !fullDownload) {
      idleWorkers.push_back(handle->addressFamily);
    }
    handles.erase(
        std::remove_if(handles.begin(), handles.end(),
                       [handle](const std::unique_ptr<CurlHandle>& entry) {
                         return entry.get() == handle;
                       }),
        handles.end());
  }

  // Redirect destinations belong to an original URI and address family. The
  // generation prevents late completions from overwriting a refreshed route.
  CurlEndpoint& endpoint(size_t uriIndex, long family)
  {
    const size_t slot = family == CURL_IPRESOLVE_V4   ? 1
                        : family == CURL_IPRESOLVE_V6 ? 2
                                                      : 0;
    return endpoints[(uriIndex % uris.size()) * 3 + slot];
  }

  std::vector<std::string> uris;
  std::vector<CurlEndpoint> endpoints;
  std::array<long, 2> families{
      {CURL_IPRESOLVE_WHATEVER, CURL_IPRESOLVE_WHATEVER}};
  std::deque<long> idleWorkers;
  size_t preferredUriIndex = 0;
  std::string path;
  std::string currentUri;
  std::string etag;
  std::string lastModified;
  std::shared_ptr<DiskWriter> writer;
  stream::IoQueue io;
  std::optional<bool> pendingStop;
  std::optional<std::pair<error_code::Value, std::string>> pendingFailure;
  bool retainFailure = true;
  std::string pendingRestart;
  std::optional<curl_off_t> pendingFinalize;
  bool finalCheckpointQueued = false;
  std::vector<std::unique_ptr<CurlHandle>> handles;
  RangePlanner planner;
  RequestGroup* group = nullptr;
  int maxConnections = 1;
  int connectionLimit = 1;
  int64_t maxRangeSize = 0;
  uint64_t connectionEpoch = 0;
  int64_t lastRecoveryDownloadLength = 0;
  std::chrono::steady_clock::time_point recoverConnectionsAt{};
  int fileNotFoundCount = 0;
  bool dryRun = false;
  bool http = false;
  bool rangeValidated = false;
  bool allowFullRestart = false;
  bool fullDownload = false;
  unsigned fullRestarts = 0;
  std::optional<std::chrono::steady_clock::time_point> fullRetryAt;
  bool plannerConfigured = false;
  bool kickPending = false;
  bool stopRequested = false;
  bool createdOutput = false;
  bool filenamePending = false;
  Timer lastCheckpoint = Timer::zero();
};

} // namespace aria2

#endif // D_CURL_DOWNLOAD_IMPL_H
