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
#include "Option.h"
#include "common.h" // IWYU pragma: keep
#include "RangePlanner.h"
#include "a2functional.h"

#include "CurlSession.h"

#include "error_code.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <curl/system.h>
#include <curl/curl.h>
#include <curl/multi.h>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "CurlDownload.h"
#include "media/MediaDownload.h"
#include "CurlDownloadCommand.h"
#include "CurlDownloadImpl.h"
#include "stream/CurlHandle.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "DefaultDiskWriterFactory.h"
#include "DiskWriterFactory.h"
#include "Exception.h"
#include "Log.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SimpleRandomizer.h"
#include "fmt.h"
#include "prefs.h"
#include "wallclock.h"

namespace aria2 {
namespace {
constexpr int64_t rangePipelineDepth = 2;

int64_t adaptiveRangeSize(int64_t length, int maxConnections, int64_t quantum)
{
  length = std::max<int64_t>(0, length);
  quantum = std::max<int64_t>(1, quantum);
  if (maxConnections <= 1) {
    return std::max(quantum, length);
  }
  const auto targetRanges =
      static_cast<int64_t>(maxConnections) * rangePipelineDepth;
  const auto target =
      std::max<int64_t>(quantum, (length - 1) / targetRanges + 1);
  return ((target + quantum - 1) / quantum) * quantum;
}
} // namespace

std::vector<RangeLease>
CurlSession::activeLeases(const std::shared_ptr<CurlDownload>& download) const
{
  std::vector<RangeLease> result;
  for (const auto& entry : download->impl_->handles) {
    if (entry->value) {
      result.push_back(entry->lease);
    }
  }
  return result;
}

void CurlSession::configurePlanner(
    const std::shared_ptr<CurlDownload>& download,
    const RangeLease* retainedLease)
{
  auto& impl = *download->impl_;
  const auto total = download->snapshot_.totalLength;
  if (impl.plannerConfigured || total <= 0) {
    return;
  }
  if (!impl.rangeValidated) {
    return;
  }
  const auto quantum = std::max<int64_t>(
      1_m, impl.group->getOption()->getAsInt(PREF_PIECE_LENGTH));
  const auto naturalSize =
      adaptiveRangeSize(total, impl.maxConnections, quantum);
  const auto rangeSize = impl.maxRangeSize > 0
                             ? std::min(naturalSize, impl.maxRangeSize)
                             : naturalSize;
  auto active = activeLeases(download);
  if (retainedLease && !retainedLease->empty()) {
    active.push_back(*retainedLease);
  }
  impl.planner.configure(total, rangeSize, active);
  impl.plannerConfigured = true;
  // Workers retain their transport choice as they pull ranges. Fast workers
  // naturally claim more work; no task-wide address-family winner is needed.
  impl.idleWorkers.clear();
  for (int i = 0; i < impl.maxConnections; ++i) {
    impl.idleWorkers.push_back(impl.families[i % impl.families.size()]);
  }
  for (const auto& handle : impl.handles) {
    const auto worker =
        std::find(impl.idleWorkers.begin(), impl.idleWorkers.end(),
                  handle->addressFamily);
    if (worker != impl.idleWorkers.end()) {
      impl.idleWorkers.erase(worker);
    }
  }

  const auto taskOption = impl.group->getOption();
  const bool sizeOutput =
      taskOption->get(PREF_FILE_ALLOCATION) != V_NONE &&
      total >= taskOption->getAsLLInt(PREF_NO_FILE_ALLOCATION_LIMIT);
  if (!sizeOutput || !impl.writer) {
    return;
  }
  try {
    impl.io.submit([writer = impl.writer, total] { writer->truncate(total); });
  }
  catch (const Exception& error) {
    failTask(download, error.getErrorCode(), error.what());
  }
  catch (const std::exception& error) {
    failTask(download, error_code::FILE_IO_ERROR, error.what());
  }
}

void CurlSession::schedule(const std::shared_ptr<CurlDownload>& download)
{
  auto& impl = *download->impl_;
  if (impl.stopRequested || download->stopped()) {
    return;
  }
  configurePlanner(download);
  if (download->failed()) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (impl.fullRetryAt) {
    if (now < *impl.fullRetryAt) {
      engine_->setRefreshInterval(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              *impl.fullRetryAt - now));
      return;
    }
    impl.fullRetryAt.reset();
    const RangeLease lease{0, std::numeric_limits<int64_t>::max(), 0,
                           impl.preferredUriIndex};
    if (!createHandle(download, lease, true, false)) {
      failTask(download, error_code::NETWORK_PROBLEM,
               "Unable to restart the complete transfer");
      return;
    }
    auto* handle = impl.handles.back().get();
    downloads_[handle->value] = std::make_pair(download, handle);
    impl.kickPending = true;
    rebalanceLimits();
  }
  if (!impl.plannerConfigured) {
    if (impl.handles.empty()) {
      if (auto lease = impl.planner.takeReady(now)) {
        const bool ranged =
            impl.http && (impl.maxConnections > 1 || lease->begin > 0);
        if (!createHandle(download, *lease, true, ranged)) {
          failTask(download, error_code::NETWORK_PROBLEM,
                   "Unable to restart the stream transfer");
          return;
        }
        auto* handle = impl.handles.back().get();
        downloads_[handle->value] = std::make_pair(download, handle);
        impl.kickPending = true;
        rebalanceLimits();
      }
    }
    if (const auto deadline = impl.planner.nextDeadline()) {
      engine_->setRefreshInterval(
          std::max(std::chrono::milliseconds(0),
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       *deadline - now)));
    }
    if (impl.handles.empty() && !impl.planner.hasPending()) {
      failTask(download, error_code::NETWORK_PROBLEM,
               "The stream ended without completing the download");
    }
    return;
  }

  rewardConnectionLimit(download);
  const auto pieceLength = std::max<int64_t>(
      1_m, impl.group->getOption()->getAsInt(PREF_PIECE_LENGTH));
  bool scheduled = false;
  for (;;) {
    const auto remaining =
        std::max<int64_t>(0, download->snapshot_.totalLength -
                                 download->snapshot_.completedLength);
    const auto preferredPiece =
        adaptiveRangeSize(remaining, impl.connectionLimit, pieceLength);
    impl.planner.refillReady(static_cast<size_t>(impl.connectionLimit),
                             preferredPiece, pieceLength);
    auto available = impl.idleWorkers.size();
    while (impl.handles.size() < static_cast<size_t>(impl.connectionLimit) &&
           available-- > 0) {
      auto lease = impl.planner.takeReady(now);
      if (!lease) {
        break;
      }
      auto family = impl.idleWorkers.front();
      impl.idleWorkers.pop_front();
      if (impl.endpoint(lease->uriIndex, family).unavailable) {
        family = CurlEndpoint::alternateFamily(family);
      }
      auto& endpoint = impl.endpoint(lease->uriIndex, family);
      if (impl.http && endpoint.uri.empty() &&
          (endpoint.resolving || endpoint.readyAt > now)) {
        impl.planner.enqueue(*lease);
        impl.idleWorkers.push_back(family);
        if (endpoint.readyAt > now) {
          engine_->setRefreshInterval(
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  endpoint.readyAt - now));
        }
        continue;
      }
      if (!createHandle(download, *lease, false, true, family)) {
        failTask(download, error_code::NETWORK_PROBLEM,
                 "Unable to create a ranged transfer");
        return;
      }
      auto* handle = impl.handles.back().get();
      downloads_[handle->value] = std::make_pair(download, handle);
      scheduled = true;
    }
    if (!impl.planner.hasReady(now) &&
        rebalanceEndgame(download, pieceLength)) {
      continue;
    }
    break;
  }

  if (const auto deadline = impl.planner.nextDeadline()) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now);
    engine_->setRefreshInterval(
        std::max(std::chrono::milliseconds(0), remaining));
  }
  if (scheduled) {
    impl.kickPending = true;
    rebalanceLimits();
  }
  if (impl.handles.empty() && !impl.planner.hasPending() &&
      !impl.planner.complete() && !download->stopped()) {
    failTask(download, error_code::UNKNOWN_ERROR,
             fmt("Stream scheduler lost an unfinished range: completed=%" PRId64
                 " total=%" PRId64,
                 impl.planner.completedLength(), impl.planner.totalLength()));
  }
}

bool CurlSession::rebalanceEndgame(
    const std::shared_ptr<CurlDownload>& download, int64_t pieceLength)
{
  auto& impl = *download->impl_;
  const auto active = impl.handles.size();
  if (active == 0) {
    return false;
  }
  const auto capacity = static_cast<size_t>(impl.connectionLimit);
  if (capacity <= 1 || active >= capacity || globalDownloadLimit_ > 0 ||
      impl.group->getMaxDownloadSpeedLimit() > 0) {
    return false;
  }
  // Deferred retries still own work. Creating helpers during their cooldown
  // would bypass backoff; an in-flight retry must receive data first.
  if (impl.planner.hasPending() ||
      std::any_of(impl.handles.begin(), impl.handles.end(),
                  [](const auto& handle) {
                    return handle->lease.attempts > 0 &&
                           handle->writeOffset == handle->lease.begin;
                  })) {
    return false;
  }
  // Assign idle workers the largest unfinished suffix, preserving the live
  // prefix. Splitting does not need to predict a connection's future speed.
  CurlHandle* handle = nullptr;
  int64_t largest = 0;
  long double longest = 0;
  int64_t splitQuantum = pieceLength;
  const auto& now = global::wallclock();
  for (const auto& candidate : impl.handles) {
    const auto remaining = candidate->lease.end - candidate->writeOffset;
    if (candidate->completed || candidate->pausedForDisk ||
        !candidate->rangeAccepted || remaining <= 0) {
      continue;
    }
    if (remaining >= 2 * 64_k) {
      if (remaining > largest) {
        largest = remaining;
        handle = candidate.get();
        splitQuantum = 64_k;
      }
      continue;
    }
    // Small tails cannot be split. Preserve fresh requests until measured
    // remaining time justifies paying for another request.
    if (largest > 0 || candidate->bodySampleStart.isZero()) {
      continue;
    }
    curl_off_t firstByte = 0;
    curl_easy_getinfo(candidate->value, CURLINFO_STARTTRANSFER_TIME_T,
                      &firstByte);
    const auto requestCost = std::max(1.0L, firstByte / 1000000.0L);
    const auto bodySeconds = std::chrono::duration<long double>(
                                 candidate->bodySampleStart.difference(now))
                                 .count();
    if (bodySeconds < std::max(2.0L, requestCost)) {
      continue;
    }
    const auto speed = candidate->payloadSpeed.calculateNewestSpeed(2);
    const auto idleSeconds = std::chrono::duration<long double>(
                                 candidate->lastPayload.difference(now))
                                 .count();
    if (speed == 0 && idleSeconds < requestCost * 2) {
      continue;
    }
    const auto remainingTime =
        speed > 0 ? static_cast<long double>(remaining) / speed
                  : std::numeric_limits<long double>::infinity();
    const auto quantum =
        std::max<int64_t>(64_k, static_cast<int64_t>(std::min<long double>(
                                    pieceLength, speed * requestCost)));
    if (remainingTime > requestCost * 2 && remainingTime > longest) {
      handle = candidate.get();
      longest = remainingTime;
      splitQuantum = quantum;
    }
  }
  if (!handle) {
    return false;
  }
  try {
    stream::flushWriteBuffer(impl, *handle);
    download->snapshot_.completedLength =
        impl.planner.completedLength() + stream::bufferedLength(impl);
  }
  catch (const Exception& error) {
    failTask(download, error.getErrorCode(), error.what());
    return false;
  }
  catch (const std::exception& error) {
    failTask(download, error_code::FILE_IO_ERROR, error.what());
    return false;
  }

  auto remainder = handle->lease.remainder(handle->writeOffset);
  if (remainder.length() >= splitQuantum * 2) {
    remainder.begin += (remainder.length() / 2 / splitQuantum) * splitQuantum;
    handle->lease.end = remainder.begin;
    impl.planner.enqueue(remainder);
    A2_LOG_DEBUG(fmt("component=stream event=tail_assisted gid=%s "
                     "range=%" PRId64 "-%" PRId64,
                     CurlHandle::gid(download.get()).c_str(), remainder.begin,
                     remainder.end));
    return true;
  }
  auto found = downloads_.find(handle->value);
  if (found != downloads_.end()) {
    const auto result =
        curl_multi_remove_handle(transport_.get(), handle->value);
    if (result != CURLM_OK) {
      A2_LOG_WARN(fmt("component=stream event=handle_remove_failed gid=%s "
                      "curlm=%d message=%s",
                      CurlHandle::gid(download.get()).c_str(),
                      static_cast<int>(result), curl_multi_strerror(result)));
    }
    downloads_.erase(found);
  }
  handle->reset();
  impl.eraseHandle(handle);
  impl.planner.enqueue(remainder);
  A2_LOG_DEBUG(fmt("component=stream event=tail_reassigned gid=%s "
                   "range=%" PRId64 "-%" PRId64,
                   CurlHandle::gid(download.get()).c_str(), remainder.begin,
                   remainder.end));
  return true;
}

std::optional<std::chrono::milliseconds>
CurlSession::retryRange(const std::shared_ptr<CurlDownload>& download,
                        const RangeLease& failed, curl_off_t retryAfter)
{
  auto& impl = *download->impl_;
  auto retry = failed;
  ++retry.attempts;
  const auto maxTries = impl.group->getOption()->getAsInt(PREF_MAX_TRIES);
  if (maxTries > 0 && retry.attempts >= static_cast<size_t>(maxTries)) {
    return std::nullopt;
  }
  if (impl.uris.size() > 1) {
    retry.uriIndex = (retry.uriIndex + 1) % impl.uris.size();
  }
  const auto configured = impl.group->getOption()->getAsInt(PREF_RETRY_WAIT);
  const auto wait =
      std::max<curl_off_t>(configured, std::max<curl_off_t>(0, retryAfter));
  const auto backoff =
      std::min<long>(30000, 100L << std::min<size_t>(retry.attempts - 1, 9));
  const auto jitter = std::chrono::milliseconds(
      SimpleRandomizer::getInstance()->getRandomNumber(backoff + 1));
  const auto delay = std::chrono::seconds(wait) + jitter;
  impl.planner.defer(retry, std::chrono::steady_clock::now() + delay);
  engine_->setNoWait(true);
  return delay;
}

void CurlSession::penalizeConnectionLimit(
    const std::shared_ptr<CurlDownload>& download, uint64_t requestEpoch)
{
  auto& impl = *download->impl_;
  // An overload response reduces one request generation once. Requests still
  // waiting for a response provide no evidence of the origin's capacity.
  if (requestEpoch != impl.connectionEpoch) {
    return;
  }
  ++impl.connectionEpoch;
  const auto previous = impl.connectionLimit;
  const auto admitted = static_cast<int>(std::count_if(
      impl.handles.begin(), impl.handles.end(), [](const auto& handle) {
        return handle->rangeAccepted &&
               handle->writeOffset > handle->lease.begin;
      }));
  impl.connectionLimit =
      std::clamp(std::max(previous / 2, admitted), 1, impl.maxConnections);
  impl.lastRecoveryDownloadLength = download->snapshot_.sessionDownloadLength;
  impl.recoverConnectionsAt =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  if (impl.connectionLimit != previous) {
    A2_LOG_DEBUG(fmt("component=stream event=connection_limit_reduced gid=%s "
                     "previous=%d current=%d",
                     CurlHandle::gid(download.get()).c_str(), previous,
                     impl.connectionLimit));
  }
}

void CurlSession::rewardConnectionLimit(
    const std::shared_ptr<CurlDownload>& download)
{
  auto& impl = *download->impl_;
  const auto now = std::chrono::steady_clock::now();
  if (impl.connectionLimit >= impl.maxConnections ||
      download->snapshot_.sessionDownloadLength <=
          impl.lastRecoveryDownloadLength) {
    return;
  }
  const auto previous = impl.connectionLimit;
  const auto admitted = static_cast<int>(std::count_if(
      impl.handles.begin(), impl.handles.end(), [](const auto& handle) {
        return handle->rangeAccepted &&
               handle->writeOffset > handle->lease.begin;
      }));
  // Late payload corrects the observed floor immediately. Further growth is
  // paced and requires fresh bytes, not completion of an entire long range.
  impl.connectionLimit =
      std::min(impl.maxConnections,
               std::max(admitted,
                        previous + (now >= impl.recoverConnectionsAt ? 1 : 0)));
  if (impl.connectionLimit == previous) {
    return;
  }
  impl.lastRecoveryDownloadLength = download->snapshot_.sessionDownloadLength;
  impl.recoverConnectionsAt = now + std::chrono::seconds(1);
  A2_LOG_DEBUG(fmt("component=stream event=connection_limit_increased gid=%s "
                   "current=%d maximum=%d",
                   CurlHandle::gid(download.get()).c_str(),
                   impl.connectionLimit, impl.maxConnections));
}

void CurlSession::advance(const std::shared_ptr<CurlDownload>& download)
{
  auto& impl = *download->impl_;
  try {
    impl.io.poll(
        [&](int64_t begin, int64_t end) { impl.planner.commit(begin, end); });
  }
  catch (const Exception& error) {
    if (!impl.pendingFailure || impl.retainFailure)
      impl.finalCheckpointQueued = false;
    failTask(download, error.getErrorCode(), error.what(), false);
    return;
  }
  catch (const std::exception& error) {
    if (!impl.pendingFailure || impl.retainFailure)
      impl.finalCheckpointQueued = false;
    failTask(download, error_code::FILE_IO_ERROR, error.what(), false);
    return;
  }
  if (download->failed() && !impl.pendingFailure) {
    failTask(download, download->snapshot_.errorCode,
             download->snapshot_.error);
    return;
  }
  if (download->stopped())
    return;
  if (impl.io.busy())
    engine_->setRefreshInterval(std::chrono::milliseconds(10));
  download->snapshot_.completedLength =
      impl.planner.completedLength() + stream::bufferedLength(impl);
  if (impl.pendingFailure) {
    auto failure = *impl.pendingFailure;
    failTask(download, failure.first, failure.second, impl.retainFailure);
    return;
  }
  if (impl.pendingStop) {
    stop(download, *impl.pendingStop);
    return;
  }
  if (!impl.pendingRestart.empty()) {
    auto reason = std::move(impl.pendingRestart);
    impl.pendingRestart.clear();
    restartFullDownload(download, reason.c_str());
    return;
  }
  if (impl.pendingFinalize) {
    finalize(download, *impl.pendingFinalize);
    return;
  }
  if (!impl.io.busy()) {
    for (;;) {
      const auto found = std::find_if(
          impl.handles.begin(), impl.handles.end(),
          [](const auto& handle) { return handle->completed.has_value(); });
      if (found == impl.handles.end())
        break;
      auto* handle = found->get();
      finish(download, handle, *handle->completed);
      if (download->stopped() || impl.io.busy() || impl.pendingFinalize ||
          !impl.pendingRestart.empty())
        return;
    }
  }
  for (const auto& handle : impl.handles) {
    if (handle->pausedForDisk && !handle->completed &&
        impl.io.bytes() < stream::IoQueue::capacity / 2) {
      handle->pausedForDisk = false;
      const auto result = curl_easy_pause(handle->value, CURLPAUSE_CONT);
      if (result != CURLE_OK) {
        failTask(download, error_code::NETWORK_PROBLEM,
                 curl_easy_strerror(result));
        return;
      }
    }
  }
  schedule(download);
  if (download->impl_->kickPending) {
    download->impl_->kickPending = false;
    transport_.socketAction(CURL_SOCKET_TIMEOUT, 0);
  }
}

} // namespace aria2
