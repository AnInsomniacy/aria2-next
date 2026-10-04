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

#include "CurlSession.h"

#include "error_code.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <ctime>
#include <curl/curl.h>
#include <curl/system.h>
#include <exception>
#include <memory>
#include <optional>
#include <utility>

#include "CurlDownload.h"
#include "media/MediaDownload.h"
#include "CurlDownloadCommand.h"
#include "CurlDownloadImpl.h"
#include "stream/CurlHandle.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Exception.h"
#include "File.h"
#include "Log.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "fmt.h"
#include "prefs.h"
#include "wallclock.h"

namespace aria2 {
namespace {
bool retryableHttpFailure(CURLcode result, long responseCode,
                          int fileNotFoundCount, int maxFileNotFound,
                          bool validatedRange)
{
  if (result == CURLE_REMOTE_FILE_NOT_FOUND) {
    return maxFileNotFound > 0 && fileNotFoundCount < maxFileNotFound;
  }
  if (result != CURLE_HTTP_RETURNED_ERROR) {
    return false;
  }
  if (responseCode == 404) {
    return maxFileNotFound > 0 && fileNotFoundCount < maxFileNotFound;
  }
  if (responseCode == 403) {
    return validatedRange;
  }
  return responseCode == 408 || responseCode == 425 || responseCode == 429 ||
         responseCode == 500 || responseCode == 502 || responseCode == 503 ||
         responseCode == 504;
}

bool retryableTransportFailure(CURLcode result, bool applicationConnected)
{
  switch (result) {
  case CURLE_COULDNT_RESOLVE_HOST:
  case CURLE_COULDNT_CONNECT:
  case CURLE_OPERATION_TIMEDOUT:
  case CURLE_PARTIAL_FILE:
  case CURLE_SEND_ERROR:
  case CURLE_RECV_ERROR:
  case CURLE_GOT_NOTHING:
  case CURLE_HTTP2:
  case CURLE_HTTP2_STREAM:
  case CURLE_AGAIN:
  case CURLE_SSL_CONNECT_ERROR:
    return true;
  case CURLE_SSH:
    return applicationConnected;
  default:
    return false;
  }
}

error_code::Value curlErrorCode(CURLcode result, long responseCode)
{
  if (responseCode == 401 || responseCode == 403) {
    return error_code::HTTP_AUTH_FAILED;
  }
  if (responseCode == 404) {
    return error_code::RESOURCE_NOT_FOUND;
  }
  if (responseCode == 408 || responseCode == 425 || responseCode == 429 ||
      responseCode == 500 || responseCode == 502 || responseCode == 503 ||
      responseCode == 504) {
    return error_code::HTTP_SERVICE_UNAVAILABLE;
  }
  if (responseCode >= 400) {
    return error_code::HTTP_PROTOCOL_ERROR;
  }
  switch (result) {
  case CURLE_OPERATION_TIMEDOUT:
    return error_code::TIME_OUT;
  case CURLE_COULDNT_RESOLVE_HOST:
  case CURLE_COULDNT_RESOLVE_PROXY:
    return error_code::NAME_RESOLVE_ERROR;
  case CURLE_TOO_MANY_REDIRECTS:
    return error_code::HTTP_TOO_MANY_REDIRECTS;
  case CURLE_LOGIN_DENIED:
  case CURLE_REMOTE_ACCESS_DENIED:
    return error_code::HTTP_AUTH_FAILED;
  case CURLE_REMOTE_FILE_NOT_FOUND:
    return error_code::RESOURCE_NOT_FOUND;
  case CURLE_RANGE_ERROR:
    return error_code::CANNOT_RESUME;
  default:
    return error_code::NETWORK_PROBLEM;
  }
}
} // namespace

bool CurlSession::retryableFailure(CURLcode result, long responseCode,
                                   int fileNotFoundCount, int maxFileNotFound,
                                   bool validatedRange,
                                   bool applicationConnected)
{
  return retryableHttpFailure(result, responseCode, fileNotFoundCount,
                              maxFileNotFound, validatedRange) ||
         retryableTransportFailure(result, applicationConnected);
}

void CurlSession::finish(const std::shared_ptr<CurlDownload>& download,
                         CurlHandle* handle, CURLcode result)
{
  auto& impl = *download->impl_;
  long responseCode = handle->responseCode;
  if (handle->manualRedirect && responseCode >= 300 && responseCode < 400) {
    char* destination = nullptr;
    curl_easy_getinfo(handle->value, CURLINFO_REDIRECT_URL, &destination);
    const std::string redirect = destination ? destination : "";
    const auto lease = handle->lease;
    const auto primary = handle->primary;
    const auto ranged = handle->ranged;
    const auto family = handle->addressFamily;
    const auto redirects = handle->redirects + 1;
    handle->reset();
    impl.handles.erase(std::remove_if(impl.handles.begin(), impl.handles.end(),
                                      [handle](const auto& item) {
                                        return item.get() == handle;
                                      }),
                       impl.handles.end());
    if (redirects > 10 || !(redirect.rfind("https://", 0) == 0 ||
                            redirect.rfind("http://", 0) == 0)) {
      failTask(download, error_code::HTTP_TOO_MANY_REDIRECTS,
               "Invalid or excessive HTTP redirects");
      return;
    }
    if (!createHandle(download, lease, primary, ranged, family, redirect,
                      redirects)) {
      failTask(download, error_code::NETWORK_PROBLEM,
               "Unable to follow HTTP redirect");
      return;
    }
    auto* next = impl.handles.back().get();
    downloads_[next->value] = std::make_pair(download, next);
    impl.kickPending = true;
    return;
  }
  if (download->snapshot_.mediaManifest) {
    cancelHandles(download);
    closeOutput(download.get());
    discardRecovery(download);
    if (impl.createdOutput) {
      impl.io.submit([path = impl.path] {
        if (File(path).size() == 0)
          File(path).remove();
      });
    }
    impl.finalCheckpointQueued = true;
    stop(download, false);
    return;
  }
  curl_off_t retryAfter = 0;
  curl_off_t reportedLength = 0;
  curl_off_t reportedFileTime = -1;
  curl_off_t reportedSpeed = 0;
  curl_off_t transferId = -1;
  curl_off_t connectionId = -1;
  curl_off_t nameLookupTime = 0;
  curl_off_t connectTime = 0;
  curl_off_t appConnectTime = 0;
  curl_off_t startTransferTime = 0;
  curl_off_t queueTime = 0;
  long newConnections = 0;
  long httpVersion = 0;
  long osError = 0;
  long primaryPort = 0;
  char* primaryIp = nullptr;
  char* effectiveUri = nullptr;
  if (handle->value) {
    curl_easy_getinfo(handle->value, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_getinfo(handle->value, CURLINFO_RETRY_AFTER, &retryAfter);
    curl_easy_getinfo(handle->value, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                      &reportedLength);
    curl_easy_getinfo(handle->value, CURLINFO_FILETIME_T, &reportedFileTime);
    curl_easy_getinfo(handle->value, CURLINFO_SPEED_DOWNLOAD_T, &reportedSpeed);
    curl_easy_getinfo(handle->value, CURLINFO_XFER_ID, &transferId);
    curl_easy_getinfo(handle->value, CURLINFO_CONN_ID, &connectionId);
    curl_easy_getinfo(handle->value, CURLINFO_NAMELOOKUP_TIME_T,
                      &nameLookupTime);
    curl_easy_getinfo(handle->value, CURLINFO_CONNECT_TIME_T, &connectTime);
    curl_easy_getinfo(handle->value, CURLINFO_APPCONNECT_TIME_T,
                      &appConnectTime);
    curl_easy_getinfo(handle->value, CURLINFO_STARTTRANSFER_TIME_T,
                      &startTransferTime);
    curl_easy_getinfo(handle->value, CURLINFO_QUEUE_TIME_T, &queueTime);
    curl_easy_getinfo(handle->value, CURLINFO_NUM_CONNECTS, &newConnections);
    curl_easy_getinfo(handle->value, CURLINFO_HTTP_VERSION, &httpVersion);
    curl_easy_getinfo(handle->value, CURLINFO_OS_ERRNO, &osError);
    curl_easy_getinfo(handle->value, CURLINFO_PRIMARY_IP, &primaryIp);
    curl_easy_getinfo(handle->value, CURLINFO_PRIMARY_PORT, &primaryPort);
    curl_easy_getinfo(handle->value, CURLINFO_EFFECTIVE_URL, &effectiveUri);
  }
  if (result == CURLE_WRITE_ERROR && handle->rangeAccepted &&
      handle->writeOffset == handle->lease.end &&
      handle->lease.end < handle->responseRangeEnd &&
      download->snapshot_.errorCode == error_code::UNDEFINED) {
    // The native callback stopped a donor after completing its shortened
    // assignment. Its suffix is independently owned by another request.
    result = CURLE_OK;
  }
  const auto nativeFailure =
      CurlHandle::failureMessage(*handle, result, responseCode);
  const auto safePrimaryIp =
      logging::sanitizeText(primaryIp ? primaryIp : "unknown");
  const auto safeEffectiveUri =
      logging::sanitizeUri(effectiveUri ? effectiveUri : impl.currentUri);
  try {
    stream::flushWriteBuffer(impl, *handle);
    download->snapshot_.completedLength =
        impl.planner.completedLength() + stream::bufferedLength(impl);
  }
  catch (const Exception& error) {
    CurlHandle::fail(download.get(), error.getErrorCode(), error.what());
  }
  catch (const std::exception& error) {
    CurlHandle::fail(download.get(), error_code::FILE_IO_ERROR, error.what());
  }
  const auto lease = handle->lease;
  const auto writeOffset = handle->writeOffset;
  const auto unsatisfiedTotalLength = handle->unsatisfiedTotalLength;
  const bool ranged = handle->ranged;
  const bool rangeAccepted = handle->rangeAccepted;
  const bool fullResponseAccepted = handle->fullResponseAccepted;
  const bool encoded = handle->encoded;
  const auto responseFailure = responseCode == 412
                                   ? CurlResponseFailure::PreconditionFailed
                                   : handle->responseFailure;
  if (responseFailure != CurlResponseFailure::None) {
    if (responseFailure == CurlResponseFailure::EncodedRange) {
      restartFullDownload(download, "encoded_range");
      return;
    }
    A2_LOG_DEBUG(
        fmt("component=stream event=response_rejected gid=%s "
            "transfer=%" PRId64 " reason=%s http=%ld range=%" PRId64 "-%" PRId64
            " expected_etag=%s received_etag=%s expected_modified=%s"
            " received_modified=%s expected_length=%" PRId64
            " received_length=%" PRId64 " uri=%s",
            CurlHandle::gid(download.get()).c_str(),
            static_cast<int64_t>(transferId),
            stream::responseFailureName(responseFailure), responseCode,
            lease.begin, lease.end, logging::sanitizeText(impl.etag).c_str(),
            logging::sanitizeText(handle->responseEtag).c_str(),
            logging::sanitizeText(impl.lastModified).c_str(),
            logging::sanitizeText(handle->responseLastModified).c_str(),
            download->snapshot_.totalLength,
            responseCode == 206 ? handle->responseTotalLength
                                : handle->responseContentLength,
            safeEffectiveUri.c_str()));
  }
  const bool primary = handle->primary;
  const auto requestEpoch = handle->connectionEpoch;
  const auto endpointGeneration = handle->endpointGeneration;
  const bool resolvingEndpoint = handle->resolvingEndpoint;
  const bool redirectedEndpoint = handle->redirectedEndpoint;
  const auto addressFamily = handle->addressFamily;
  auto& endpoint = impl.endpoint(lease.uriIndex, addressFamily);
  const auto& peer = impl.endpoint(
      lease.uriIndex, CurlEndpoint::alternateFamily(addressFamily));
  const bool unavailableRoute =
      resolvingEndpoint && addressFamily != CURL_IPRESOLVE_WHATEVER &&
      !peer.unavailable && !peer.uri.empty() &&
      (result == CURLE_COULDNT_RESOLVE_HOST ||
       result == CURLE_COULDNT_CONNECT || result == CURLE_OPERATION_TIMEDOUT ||
       (result == CURLE_HTTP_RETURNED_ERROR &&
        (responseCode == 401 || responseCode == 403 || responseCode == 404)));
  const bool expiredEndpoint =
      redirectedEndpoint &&
      (responseCode == 401 || responseCode == 403 || responseCode == 404);
  if (endpointGeneration == endpoint.generation) {
    if (unavailableRoute) {
      endpoint.unavailable = true;
    }
    if (resolvingEndpoint) {
      endpoint.resolving = false;
    }
    if (expiredEndpoint) {
      const auto generation = endpoint.generation + 1;
      endpoint = CurlEndpoint{};
      endpoint.generation = generation;
    }
  }
  const bool outputFailure =
      download->snapshot_.errorCode != error_code::UNDEFINED;
  handle->reset();
  impl.eraseHandle(handle);
  if (primary && rangeAccepted && !impl.plannerConfigured) {
    configurePlanner(download, &lease);
    if (download->failed()) {
      return;
    }
  }

  A2_LOG_TRACE(fmt(
      "component=stream event=range_finished gid=%s "
      "transfer=%" PRId64 " connection=%" PRId64 " range=%" PRId64 "-%" PRId64
      " http=%ld curl=%d os_error=%ld remote=%s:%ld speed=%" PRId64
      " dns_us=%" PRId64 " connect_us=%" PRId64 " tls_us=%" PRId64
      " first_byte_us=%" PRId64 " queue_us=%" PRId64
      " new_connections=%ld http_version=%ld uri=%s",
      CurlHandle::gid(download.get()).c_str(), static_cast<int64_t>(transferId),
      static_cast<int64_t>(connectionId), lease.begin, lease.end, responseCode,
      static_cast<int>(result), osError, safePrimaryIp.c_str(), primaryPort,
      static_cast<int64_t>(reportedSpeed), static_cast<int64_t>(nameLookupTime),
      static_cast<int64_t>(connectTime), static_cast<int64_t>(appConnectTime),
      static_cast<int64_t>(startTransferTime), static_cast<int64_t>(queueTime),
      newConnections, httpVersion, safeEffectiveUri.c_str()));

  if (impl.dryRun) {
    download->snapshot_.totalLength = std::max<curl_off_t>(0, reportedLength);
    download->snapshot_.state = CurlSnapshot::State::Complete;
    discardRecovery(download);
    eraseTask(download.get());
    return;
  }

  if (outputFailure) {
    failTask(download, download->snapshot_.errorCode,
             download->snapshot_.error);
    return;
  }
  if (responseFailure != CurlResponseFailure::None) {
    const bool identityChanged =
        responseFailure == CurlResponseFailure::EtagChanged ||
        responseFailure == CurlResponseFailure::ModifiedChanged ||
        responseFailure == CurlResponseFailure::LengthChanged ||
        responseFailure == CurlResponseFailure::ValidatorUnavailable;
    if (ranged && identityChanged &&
        (responseCode == 200 || responseCode == 206) && impl.allowFullRestart &&
        !impl.fullDownload && impl.maxRangeSize == 0) {
      restartFullDownload(download,
                          stream::responseFailureName(responseFailure));
      return;
    }
    failTask(download,
             responseFailure == CurlResponseFailure::InvalidRange
                 ? error_code::HTTP_PROTOCOL_ERROR
                 : error_code::CANNOT_RESUME,
             stream::responseFailureMessage(responseFailure));
    return;
  }
  if (responseCode == 416) {
    if ((unsatisfiedTotalLength == 0 && lease.begin == 0 &&
         impl.planner.completedLength() == 0 && impl.handles.empty()) ||
        (unsatisfiedTotalLength >= 0 &&
         impl.planner.completedRanges().size() == 1 &&
         impl.planner.completedRanges().front().first == 0 &&
         impl.planner.completedRanges().front().second >=
             unsatisfiedTotalLength)) {
      download->snapshot_.totalLength = unsatisfiedTotalLength;
      impl.planner.configure(unsatisfiedTotalLength,
                             std::max<int64_t>(1, unsatisfiedTotalLength), {});
      impl.plannerConfigured = true;
      finalize(download, reportedFileTime);
      return;
    }
    if (ranged && !impl.fullDownload) {
      restartFullDownload(download, "range_not_satisfiable");
      return;
    }
    failTask(download, error_code::CANNOT_RESUME,
             "The requested byte range is no longer satisfiable");
    return;
  }

  if (ranged && !rangeAccepted && !fullResponseAccepted &&
      responseCode == 200) {
    restartFullDownload(download);
    return;
  }

  if (result != CURLE_OK) {
    if (impl.fullDownload &&
        retryableFailure(result, responseCode, impl.fileNotFoundCount, 0, false,
                         appConnectTime > 0 || startTransferTime > 0)) {
      restartFullDownload(download, "complete_response_interrupted");
      return;
    }
    if (unavailableRoute && endpointGeneration == endpoint.generation) {
      // One unusable route does not invalidate an already validated peer.
      impl.planner.enqueue(lease.remainder(writeOffset));
      schedule(download);
      return;
    }
    if (responseCode == 404) {
      ++impl.fileNotFoundCount;
    }
    const auto maxFileNotFound =
        impl.group->getOption()->getAsInt(PREF_MAX_FILE_NOT_FOUND);
    const bool alternateMirror =
        impl.uris.size() > 1 && result != CURLE_WRITE_ERROR &&
        result != CURLE_OUT_OF_MEMORY && result != CURLE_ABORTED_BY_CALLBACK;
    const bool overloaded = ranged && impl.rangeValidated &&
                            (responseCode == 429 || responseCode == 503);
    if (overloaded) {
      penalizeConnectionLimit(download, requestEpoch);
    }
    auto remainder = lease.remainder(writeOffset);
    if (download->snapshot_.totalLength > 0) {
      remainder.end = std::min(remainder.end, download->snapshot_.totalLength);
    }
    std::optional<std::chrono::milliseconds> retryDelay;
    if (!impl.fullDownload && !remainder.empty() &&
        (alternateMirror || expiredEndpoint ||
         retryableFailure(result, responseCode, impl.fileNotFoundCount,
                          maxFileNotFound, ranged && impl.rangeValidated,
                          appConnectTime > 0 || startTransferTime > 0 ||
                              writeOffset > lease.begin))) {
      retryDelay = retryRange(download, remainder,
                              overloaded ? std::max<curl_off_t>(1, retryAfter)
                                         : retryAfter);
    }
    if (retryDelay) {
      if (resolvingEndpoint && endpointGeneration == endpoint.generation) {
        endpoint.readyAt = std::chrono::steady_clock::now() + *retryDelay;
      }
      A2_LOG_DEBUG(fmt(
          "component=stream event=range_retry gid=%s "
          "transfer=%" PRId64 " connection=%" PRId64 " range=%" PRId64
          "-%" PRId64 " attempt=%lu http=%ld curl=%d retry_in_ms=%" PRId64
          " message=%s",
          CurlHandle::gid(download.get()).c_str(),
          static_cast<int64_t>(transferId), static_cast<int64_t>(connectionId),
          remainder.begin, remainder.end,
          static_cast<unsigned long>(lease.attempts + 1), responseCode,
          static_cast<int>(result), static_cast<int64_t>(retryDelay->count()),
          nativeFailure.c_str()));
      schedule(download);
      return;
    }
    failTask(download, curlErrorCode(result, responseCode), nativeFailure);
    return;
  }

  if (ranged && rangeAccepted) {
    impl.planner.enqueue(lease.remainder(writeOffset));
    schedule(download);
    if (download->stopped() || !impl.planner.complete() ||
        !impl.handles.empty()) {
      return;
    }
  }
  else if (fullResponseAccepted || !impl.http) {
    if (encoded) {
      download->snapshot_.totalLength = writeOffset;
      reportedLength = writeOffset;
    }
    auto length = download->snapshot_.totalLength;
    if (length <= 0) {
      length = std::max<int64_t>(impl.planner.completedLength(),
                                 lease.begin + reportedLength);
      download->snapshot_.totalLength = length;
    }
    impl.planner.configure(length, std::max<int64_t>(1, length), {});
    impl.plannerConfigured = true;
    if (length > 0 && !impl.planner.complete()) {
      failTask(download, error_code::NETWORK_PROBLEM,
               "The complete response ended before the declared file length");
      return;
    }
  }
  finalize(download, reportedFileTime);
}

} // namespace aria2
