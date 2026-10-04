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
// Keep the Windows socket ABI consistent before native library headers.
#include "common.h" // IWYU pragma: keep
#include "a2functional.h"
#include "RangePlanner.h"

#include "CurlSession.h"

#include "error_code.h"
#include "spdlog/common.h"
#include "transport/CurlOptions.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <curl/multi.h>
#include <curl/curl.h>
#include <curl/system.h>
#include <exception>
#include <stdexcept>
#include <limits>
#include <memory>
#include <map>
#include <set>
#include <utility>

#include "ApplicationStatePath.h"
#include "CurlDownload.h"
#include "media/MediaDownload.h"
#include "CurlDownloadCommand.h"
#include "CurlDownloadImpl.h"
#include "stream/CurlHandle.h"
#include "Command.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Exception.h"
#include "Log.h"
#include "Option.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "fmt.h"
#include "prefs.h"
#include "wallclock.h"

namespace aria2 {

CurlSession::CurlSession(const Option* option)
    : transport_([this] {
        processMessages();
        for (const auto& entry : tasks_)
          refreshConnectionCount(entry.second);
      }),
      option_(option),
      globalDownloadLimit_(option->getAsLLInt(PREF_MAX_OVERALL_DOWNLOAD_LIMIT)),
      store_(std::make_shared<StreamStore>(state::streamDatabaseFile(option))),
      loggingRevision_(logging::revision())
{
  if (!transport_.get())
    return;
  store_->open();
  if (!refreshConnectionPoolLimits())
    transport_.disable();
}

CurlSession::~CurlSession()
{
  transport_.shutdown();
  for (auto& entry : downloads_) {
    const auto result = curl_multi_remove_handle(transport_.get(), entry.first);
    if (result != CURLM_OK) {
      A2_LOG_WARN(fmt("component=stream event=handle_remove_failed curlm=%d "
                      "message=%s",
                      static_cast<int>(result), curl_multi_strerror(result)));
    }
    entry.second.second->reset();
  }
  downloads_.clear();
  for (const auto& entry : tasks_) {
    try {
      for (auto& handle : entry.second->impl_->handles) {
        stream::flushWriteBuffer(*entry.second->impl_, *handle);
      }
      entry.second->snapshot_.completedLength =
          entry.second->impl_->planner.completedLength();
    }
    catch (const std::exception& error) {
      A2_LOG_ERROR(fmt("Flushing stream output failed: %s", error.what()));
    }
    checkpoint(entry.second, true);
    closeOutput(entry.first);
  }
  tasks_.clear();
}

std::unique_ptr<Command>
CurlSession::start(const std::shared_ptr<CurlDownload>& download,
                   RequestGroup* group, DownloadEngine* engine)
{
  engine_ = engine;
  transport_.bind(engine);
  if (prepare(download, group)) {
    tasks_[download.get()] = download;
    if (!refreshConnectionPoolLimits()) {
      failTask(download, error_code::NETWORK_PROBLEM,
               "Unable to configure the libcurl connection pool", false);
      return std::unique_ptr<Command>(new CurlDownloadCommand(
          engine->newCUID(), download, this, group, engine));
    }
    activate(download);
  }
  else if (!download->failed()) {
    CurlHandle::fail(download.get(), error_code::NETWORK_PROBLEM,
                     "Unable to start the curl transfer");
  }
  return std::unique_ptr<Command>(new CurlDownloadCommand(
      engine->newCUID(), download, this, group, engine));
}

void CurlSession::activate(const std::shared_ptr<CurlDownload>& download)
{
  constexpr int64_t probeSize = 4_m;
  auto& impl = *download->impl_;
  if (impl.planner.complete()) {
    finalize(download, -1);
    return;
  }
  const auto rangeStart = impl.planner.contiguousLength();
  const bool ranged = impl.http && (impl.maxConnections > 1 || rangeStart > 0);
  auto rangeEnd =
      ranged ? rangeStart +
                   (impl.maxConnections > 1
                        ? probeSize
                        : std::numeric_limits<int64_t>::max() - rangeStart)
             : std::numeric_limits<int64_t>::max();
  if (download->snapshot_.totalLength > 0) {
    rangeEnd = std::min(rangeEnd, download->snapshot_.totalLength);
  }
  rangeEnd = impl.planner.gapEnd(rangeStart, rangeEnd);
  const RangeLease lease{rangeStart, rangeEnd, 0, impl.preferredUriIndex};
  if (!createHandle(download, lease, true, ranged)) {
    CurlHandle::fail(download.get(), error_code::NETWORK_PROBLEM,
                     "Unable to start the curl transfer");
    eraseTask(download.get());
    return;
  }
  auto* handle = download->impl_->handles.back().get();
  downloads_[handle->value] = std::make_pair(download, handle);
  rebalanceLimits();
  transport_.socketAction(CURL_SOCKET_TIMEOUT, 0);
}

void CurlSession::cancelHandles(const std::shared_ptr<CurlDownload>& download)
{
  auto& impl = *download->impl_;
  for (auto& handle : impl.handles) {
    if (handle->value) {
      auto found = downloads_.find(handle->value);
      if (found != downloads_.end()) {
        const auto result =
            curl_multi_remove_handle(transport_.get(), handle->value);
        if (result != CURLM_OK) {
          A2_LOG_WARN(fmt("component=stream event=handle_remove_failed gid=%s "
                          "curlm=%d message=%s",
                          CurlHandle::gid(download.get()).c_str(),
                          static_cast<int>(result),
                          curl_multi_strerror(result)));
        }
        downloads_.erase(found);
      }
      handle->reset();
    }
  }
  impl.handles.clear();
  impl.idleWorkers.clear();
  download->snapshot_.connections = 0;
}

void CurlSession::restartFullDownload(
    const std::shared_ptr<CurlDownload>& download, const char* reason)
{
  auto& impl = *download->impl_;
  const auto configuredTries =
      impl.group->getOption()->getAsInt(PREF_MAX_TRIES);
  if (impl.io.busy()) {
    impl.stopRequested = true;
    impl.pendingRestart = reason;
    cancelHandles(download);
    return;
  }
  impl.stopRequested = false;
  const auto maxRestarts =
      static_cast<unsigned>(configuredTries > 0 ? configuredTries : 3);
  if (impl.maxRangeSize > 0 || !impl.allowFullRestart ||
      impl.fullRestarts >= maxRestarts) {
    failTask(download, error_code::CANNOT_RESUME,
             "The resource cannot be resumed safely; the existing file and "
             "range policy were preserved");
    return;
  }
  cancelHandles(download);
  closeOutput(download.get());
  if (impl.io.busy()) {
    impl.pendingRestart = reason;
    return;
  }
  discardRecovery(download);
  impl.planner.clear();
  impl.plannerConfigured = false;
  impl.rangeValidated = false;
  impl.fullDownload = true;
  ++impl.fullRestarts;
  impl.etag.clear();
  impl.lastModified.clear();
  impl.maxConnections = 1;
  impl.connectionLimit = 1;
  download->snapshot_.totalLength = 0;
  download->snapshot_.completedLength = 0;
  if (!impl.filenamePending && !openOutput(download.get(), false, true)) {
    failTask(download, download->snapshot_.errorCode, download->snapshot_.error,
             false);
    return;
  }
  impl.fullRetryAt =
      std::chrono::steady_clock::now() +
      std::chrono::seconds(
          std::max(1, impl.group->getOption()->getAsInt(PREF_RETRY_WAIT)));
  rebalanceLimits();
  engine_->setNoWait(true);
  A2_LOG_INFO(fmt("component=stream event=full_download_restart gid=%s "
                  "reason=%s",
                  CurlHandle::gid(download.get()).c_str(), reason));
}

void CurlSession::poll()
{
  if (!transport_.get()) {
    return;
  }
  const auto revision = logging::revision();
  if (revision != loggingRevision_) {
    loggingRevision_ = revision;
    const auto verbose = A2_LOG_ENABLED(spdlog::level::trace) ? 1L : 0L;
    for (const auto& entry : downloads_) {
      const auto result =
          curl_easy_setopt(entry.first, CURLOPT_VERBOSE, verbose);
      if (result != CURLE_OK) {
        A2_LOG_WARN(fmt("component=stream event=trace_reconfigure_failed "
                        "gid=%s curl=%d message=%s",
                        CurlHandle::gid(entry.second.first.get()).c_str(),
                        static_cast<int>(result), curl_easy_strerror(result)));
      }
    }
  }
  rebalanceLimits();
  for (const auto& entry : tasks_) {
    checkpoint(entry.second, false);
  }
  transport_.poll();
}

void CurlSession::refreshConnectionCount(
    const std::shared_ptr<CurlDownload>& download) const
{
  std::set<curl_off_t> connections;
  for (const auto& handle : download->impl_->handles) {
    if (!handle->value) {
      continue;
    }
    curl_off_t connectionId = -1;
    if (curl_easy_getinfo(handle->value, CURLINFO_CONN_ID, &connectionId) ==
            CURLE_OK &&
        connectionId >= 0) {
      connections.insert(connectionId);
    }
  }
  download->snapshot_.connections = static_cast<int>(connections.size());
}

void CurlSession::processMessages()
{
  int remaining = 0;
  while (auto* message = curl_multi_info_read(transport_.get(), &remaining)) {
    if (message->msg != CURLMSG_DONE) {
      continue;
    }
    auto found = downloads_.find(message->easy_handle);
    if (found == downloads_.end()) {
      continue;
    }
    auto download = found->second.first;
    auto* handle = found->second.second;
    const auto result = message->data.result;
    const auto removeResult =
        curl_multi_remove_handle(transport_.get(), message->easy_handle);
    if (removeResult != CURLM_OK) {
      A2_LOG_ERROR(fmt("component=stream event=handle_remove_failed gid=%s "
                       "curlm=%d message=%s",
                       CurlHandle::gid(download.get()).c_str(),
                       static_cast<int>(removeResult),
                       curl_multi_strerror(removeResult)));
    }
    downloads_.erase(found);
    rebalanceLimits();
    handle->completed = result;
    try {
      stream::flushWriteBuffer(*download->impl_, *handle);
    }
    catch (const std::exception& error) {
      failTask(download, error_code::FILE_IO_ERROR, error.what());
    }
  }
}

void CurlSession::stop(const std::shared_ptr<CurlDownload>& download,
                       bool retainState)
{
  auto& impl = *download->impl_;
  impl.stopRequested = true;
  impl.pendingStop = retainState;
  try {
    for (auto& handle : impl.handles) {
      stream::flushWriteBuffer(impl, *handle);
    }
    download->snapshot_.completedLength = impl.planner.completedLength();
  }
  catch (const Exception& error) {
    failTask(download, error.getErrorCode(), error.what(), false);
    return;
  }
  catch (const std::exception& error) {
    failTask(download, error_code::FILE_IO_ERROR, error.what(), false);
    return;
  }
  cancelHandles(download);
  if (impl.io.busy())
    return;
  if (!impl.finalCheckpointQueued) {
    checkpoint(download, true);
    impl.finalCheckpointQueued = true;
    if (impl.io.busy())
      return;
  }
  rebalanceLimits();
  closeOutput(download.get());
  if (impl.io.busy())
    return;
  impl.pendingStop.reset();
  if (!download->failed()) {
    if (retainState) {
      download->snapshot_.state = CurlSnapshot::State::Paused;
    }
    else {
      download->snapshot_.state = CurlSnapshot::State::Stopped;
    }
  }
  eraseTask(download.get());
}

void CurlSession::discardRecovery(const std::shared_ptr<CurlDownload>& download)
{
  download->impl_->io.submit(
      [store = store_, gid = CurlHandle::gid(download.get())] {
        if (!store->remove(gid))
          throw std::runtime_error("Unable to remove stream recovery state");
      });
}

bool CurlSession::refreshConnectionPoolLimits()
{
  if (!transport_.get() || !option_) {
    return false;
  }
  const auto maximum = std::numeric_limits<long>::max() / 2;
  const auto saturatingAdd = [maximum](long lhs, long rhs) {
    return lhs >= maximum - rhs ? maximum : lhs + rhs;
  };
  const auto maxTasks = static_cast<long>(
      std::max(1, option_->getAsInt(PREF_MAX_CONCURRENT_DOWNLOADS)));
  const auto defaultPerTask =
      static_cast<long>(effectiveStreamMaxConnections(option_));
  const auto baseline = maxTasks >= maximum / defaultPerTask
                            ? maximum
                            : maxTasks * defaultPerTask;
  long active = 0;
  for (const auto& entry : tasks_) {
    active = saturatingAdd(
        active,
        static_cast<long>(std::max(1, entry.second->impl_->maxConnections)));
  }
  const auto limit = std::max<long>(1, std::max(baseline, active));
  if (limit == connectionPoolLimit_) {
    return true;
  }
  const CURLMcode results[] = {
      curl_multi_setopt(transport_.get(), CURLMOPT_MAX_TOTAL_CONNECTIONS,
                        limit),
      curl_multi_setopt(transport_.get(), CURLMOPT_MAX_HOST_CONNECTIONS, limit),
      curl_multi_setopt(transport_.get(), CURLMOPT_MAXCONNECTS, limit * 2)};
  const auto failed =
      std::find_if(std::begin(results), std::end(results),
                   [](CURLMcode value) { return value != CURLM_OK; });
  if (failed != std::end(results)) {
    A2_LOG_ERROR(fmt("component=stream event=connection_pool_config_failed "
                     "limit=%ld curlm=%d message=%s",
                     limit, static_cast<int>(*failed),
                     curl_multi_strerror(*failed)));
    return false;
  }
  connectionPoolLimit_ = limit;
  A2_LOG_DEBUG(fmt("component=stream event=connection_pool_configured "
                   "limit=%ld active_tasks=%lu",
                   limit, static_cast<unsigned long>(tasks_.size())));
  return true;
}

void CurlSession::eraseTask(CurlDownload* download)
{
  tasks_.erase(download);
  if (transport_.get()) {
    refreshConnectionPoolLimits();
  }
}

void CurlSession::rebalanceLimits()
{
  if (downloads_.empty()) {
    return;
  }
  std::map<CurlDownload*, size_t> taskHandles;
  for (const auto& entry : downloads_) {
    ++taskHandles[entry.second.first.get()];
  }
  const auto taskShare =
      globalDownloadLimit_ > 0
          ? std::max<int64_t>(1,
                              globalDownloadLimit_ /
                                  static_cast<int64_t>(taskHandles.size() +
                                                       externalDownloadCount_))
          : 0;
  for (const auto& entry : downloads_) {
    const auto task =
        entry.second.first->impl_->group->getMaxDownloadSpeedLimit();
    const auto taskLimit = task > 0 && taskShare > 0
                               ? std::min<int64_t>(task, taskShare)
                           : task > 0 ? static_cast<int64_t>(task)
                                      : taskShare;
    const auto count = taskHandles[entry.second.first.get()];
    const auto limit =
        taskLimit > 0
            ? std::max<int64_t>(1, taskLimit / static_cast<int64_t>(count))
            : 0;
    auto* handle = entry.second.second;
    if (handle->appliedLimit != limit) {
      const auto result =
          curl_easy_setopt(entry.first, CURLOPT_MAX_RECV_SPEED_LARGE,
                           static_cast<curl_off_t>(limit));
      if (result != CURLE_OK) {
        A2_LOG_ERROR(fmt("component=stream event=rate_limit_failed gid=%s "
                         "curl=%d message=%s",
                         CurlHandle::gid(entry.second.first.get()).c_str(),
                         static_cast<int>(result), curl_easy_strerror(result)));
        continue;
      }
      handle->appliedLimit = limit;
    }
  }
}

void CurlSession::setGlobalDownloadLimit(int64_t limit)
{
  limit = std::max<int64_t>(0, limit);
  if (globalDownloadLimit_ == limit) {
    return;
  }
  globalDownloadLimit_ = limit;
  rebalanceLimits();
}

void CurlSession::setExternalDownloadCount(size_t count)
{
  if (externalDownloadCount_ == count)
    return;
  externalDownloadCount_ = count;
  rebalanceLimits();
}

int CurlSession::effectiveStreamMaxConnections(const Option* option)
{
  return std::clamp(option->getAsInt(PREF_STREAM_MAX_CONNECTIONS), 1, 256);
}

} // namespace aria2
