/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// Keep the Windows socket ABI consistent before native library headers.
#include "Option.h"
#include "CurlHandle.h"
#include "common.h" // IWYU pragma: keep

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <ctime>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/urlapi.h>
#include <curl/system.h>
#include <exception>
#include <iterator>
#include <limits>
#include <string>
#include <memory>

#include "CurlDownload.h"
#include "CurlSession.h"
#include "CurlDownloadImpl.h"
#include "DownloadContext.h"
#include "DlAbortEx.h"
#include "Exception.h"
#include "GroupId.h"
#include "Log.h"
#include "RequestGroup.h"
#include "error_code.h"
#include "fmt.h"
#include "prefs.h"
#include "spdlog/common.h"
#include "support/Network.h"
#include "wallclock.h"
#include "media/MediaDownload.h"
#include "transport/HttpHeaders.h"

namespace aria2 {
namespace stream {
void rememberIdentity(CurlDownloadImpl& impl, const std::string& etag,
                      const std::string& modified, const std::string& date)
{
  if (impl.etag.empty()) {
    impl.etag = http::normalizeStrongEtag(etag);
  }
  const auto modifiedTime = curl_getdate(modified.c_str(), nullptr);
  const auto responseTime = curl_getdate(date.c_str(), nullptr);
  // A parseable date alone is not a strong validator. Use the conservative
  // HTTP date-age rule also used by browser download implementations.
  if (impl.lastModified.empty() && modifiedTime != -1 && responseTime != -1 &&
      std::difftime(responseTime, modifiedTime) >= 60) {
    impl.lastModified = modified;
  }
}

const char* responseFailureName(CurlResponseFailure failure)
{
  switch (failure) {
  case CurlResponseFailure::EtagChanged:
    return "etag_changed";
  case CurlResponseFailure::ValidatorUnavailable:
    return "validator_unavailable";
  case CurlResponseFailure::ModifiedChanged:
    return "last_modified_changed";
  case CurlResponseFailure::LengthChanged:
    return "length_changed";
  case CurlResponseFailure::InvalidRange:
    return "invalid_range";
  case CurlResponseFailure::RangeUnsupported:
    return "range_limit_unsupported";
  case CurlResponseFailure::EncodedRange:
    return "encoded_range";
  case CurlResponseFailure::PreconditionFailed:
    return "precondition_failed";
  default:
    return "none";
  }
}

const char* responseFailureMessage(CurlResponseFailure failure)
{
  switch (failure) {
  case CurlResponseFailure::EtagChanged:
    return "The remote resource ETag changed; existing data was preserved";
  case CurlResponseFailure::ValidatorUnavailable:
    return "The response no longer confirms the requested resource identity; "
           "existing data was preserved";
  case CurlResponseFailure::ModifiedChanged:
    return "The remote resource Last-Modified changed; existing data was "
           "preserved";
  case CurlResponseFailure::LengthChanged:
    return "The remote resource length changed; existing data was preserved";
  case CurlResponseFailure::InvalidRange:
    return "The server returned an invalid Content-Range response";
  case CurlResponseFailure::RangeUnsupported:
    return "The server ignored the requested byte range; the configured range "
           "limit cannot be honored";
  case CurlResponseFailure::EncodedRange:
    return "An encoded HTTP representation cannot be written as byte ranges";
  case CurlResponseFailure::PreconditionFailed:
    return "HTTP 412: the server rejected a request precondition; existing "
           "data was preserved";
  default:
    return "Invalid HTTP response";
  }
}

void flushWriteBuffer(CurlDownloadImpl& impl, CurlHandle& handle)
{
  if (handle.writeBuffer.empty()) {
    return;
  }
  if (!impl.writer) {
    throw DL_ABORT_EX2("The output file is not open",
                       error_code::FILE_OPEN_ERROR);
  }
  const auto size = handle.writeBuffer.size();
  const auto offset = handle.bufferOffset;
  impl.io.submit(
      [writer = impl.writer, data = std::move(handle.writeBuffer), offset] {
        writer->writeData(data.data(), data.size(), offset);
      },
      offset, size);
  handle.writeBuffer = {};
}

int64_t bufferedLength(const CurlDownloadImpl& impl)
{
  int64_t result = static_cast<int64_t>(impl.io.bytes());
  for (const auto& handle : impl.handles) {
    result += static_cast<int64_t>(handle->writeBuffer.size());
  }
  return result;
}
bool containsSensitiveCurlText(const std::string& value)
{
  std::string lowerValue(value);
  std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  static const char* sensitive[] = {"authorization:", "proxy-authorization:",
                                    "cookie:",        "set-cookie:",
                                    "password",       "bearer ",
                                    "private key",    "client certificate"};
  return std::any_of(std::begin(sensitive), std::end(sensitive),
                     [&](const char* marker) {
                       return lowerValue.find(marker) != std::string::npos;
                     });
}

} // namespace stream

namespace {
void resetResponse(CurlHandle& handle)
{
  handle.responseCode = 0;
  handle.responseRangeEnd = -1;
  handle.responseTotalLength = -1;
  handle.responseContentLength = -1;
  handle.unsatisfiedTotalLength = -1;
  handle.rangeAccepted = false;
  handle.fullResponseAccepted = false;
  handle.encoded = false;
  handle.headersComplete = false;
  handle.responseFailure = CurlResponseFailure::None;
  handle.responseEtag.clear();
  handle.responseLastModified.clear();
  handle.responseDate.clear();
}

CurlResponseFailure identityFailure(const CurlDownloadImpl& impl,
                                    const CurlHandle& handle)
{
  if (!impl.etag.empty()) {
    const auto etag = http::normalizeStrongEtag(handle.responseEtag);
    if (!handle.responseEtag.empty() && etag.empty()) {
      return CurlResponseFailure::ValidatorUnavailable;
    }
    return !etag.empty() && impl.etag != etag ? CurlResponseFailure::EtagChanged
                                              : CurlResponseFailure::None;
  }
  const auto before = curl_getdate(impl.lastModified.c_str(), nullptr);
  const auto after = curl_getdate(handle.responseLastModified.c_str(), nullptr);
  return before != -1 && after != -1 && before != after
             ? CurlResponseFailure::ModifiedChanged
             : CurlResponseFailure::None;
}

bool usefulCurlText(const std::string& value)
{
  std::string lowerValue(value);
  std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (lowerValue.find("netrc file") != std::string::npos ||
      lowerValue.find("newsession ticket") != std::string::npos ||
      lowerValue.find("tls handshake") != std::string::npos ||
      lowerValue.find("change cipher") != std::string::npos ||
      lowerValue.find("certificate level") != std::string::npos ||
      lowerValue.find("server certificate:") != std::string::npos ||
      lowerValue.find("subject:") == 0 || lowerValue.find("issuer:") == 0 ||
      lowerValue.find("start date:") == 0 ||
      lowerValue.find("expire date:") == 0) {
    return false;
  }
  static const char* useful[] = {"was resolved",
                                 "trying ",
                                 "established connection",
                                 "connected to",
                                 "reusing existing",
                                 "ssl connection using",
                                 "certificate verified",
                                 "alpn: server",
                                 "using http/",
                                 "request completely",
                                 "redirect",
                                 "closing connection",
                                 "could not",
                                 "failed",
                                 "error",
                                 "timed out",
                                 "proxy",
                                 "ssh"};
  return std::any_of(std::begin(useful), std::end(useful),
                     [&](const char* marker) {
                       return lowerValue.find(marker) != std::string::npos;
                     });
}
} // namespace

CurlHandle::~CurlHandle() { reset(); }

void CurlHandle::reset() noexcept
{
  if (headers) {
    curl_slist_free_all(headers);
    headers = nullptr;
  }
  if (value) {
    curl_easy_cleanup(value);
    value = nullptr;
  }
}

void CurlHandle::rememberEndpoint(CurlHandle& handle)
{
  auto& impl = *handle.download->impl_;
  if (!handle.value || impl.endpoints.empty() ||
      (!handle.rangeAccepted && !handle.fullResponseAccepted) ||
      handle.responseFailure != CurlResponseFailure::None) {
    return;
  }
  auto& endpoint = impl.endpoint(handle.lease.uriIndex, handle.addressFamily);
  if (handle.endpointGeneration != endpoint.generation) {
    return;
  }
  char* effective = nullptr;
  if (curl_easy_getinfo(handle.value, CURLINFO_EFFECTIVE_URL, &effective) ==
          CURLE_OK &&
      effective) {
    // The native handle owns this pointer. Retain the validated URL, not its
    // storage, and keep the original task URI for restart and authentication.
    endpoint.uri = effective;
    endpoint.resolving = false;
    endpoint.readyAt = {};
    if (!impl.plannerConfigured &&
        impl.families[0] == CURL_IPRESOLVE_WHATEVER && impl.group &&
        impl.maxConnections > 1 && handle.rangeAccepted &&
        !impl.group->getOption()->getAsBool(PREF_DISABLE_IPV6) &&
        impl.group->getOption()->blank(PREF_INTERFACE) &&
        impl.group->getMaxDownloadSpeedLimit() == 0 &&
        impl.group->getOption()->getAsLLInt(PREF_MAX_OVERALL_DOWNLOAD_LIMIT) ==
            0) {
      auto url = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(
          curl_url(), curl_url_cleanup);
      char* host = nullptr;
      if (url &&
          curl_url_set(url.get(), CURLUPART_URL, effective, 0) == CURLUE_OK &&
          curl_url_get(url.get(), CURLUPART_HOST, &host, 0) == CURLUE_OK) {
        auto hostname =
            std::unique_ptr<char, decltype(&curl_free)>(host, curl_free);
        char* address = nullptr;
        long proxy = 0;
        curl_easy_getinfo(handle.value, CURLINFO_PRIMARY_IP, &address);
        curl_easy_getinfo(handle.value, CURLINFO_USED_PROXY, &proxy);
        // Literal addresses do not offer a second family. DNS, connection
        // racing and connection reuse remain native libcurl operations.
        if (!proxy && address && *address && host[0] != '[' &&
            !util::isNumericHost(host)) {
          handle.addressFamily =
              std::strchr(address, ':') ? CURL_IPRESOLVE_V6 : CURL_IPRESOLVE_V4;
          impl.families = {{handle.addressFamily, CurlEndpoint::alternateFamily(
                                                      handle.addressFamily)}};
          auto& route =
              impl.endpoint(handle.lease.uriIndex, handle.addressFamily);
          route.uri = effective;
          route.resolving = false;
          route.readyAt = {};
          handle.endpointGeneration = route.generation;
        }
      }
    }
  }
}

std::string CurlHandle::gid(const CurlDownload* download)
{
  if (!download || !download->impl_ || !download->impl_->group) {
    return "unknown";
  }
  return GroupId::toHex(download->impl_->group->getGID());
}

size_t CurlHandle::writeData(char* data, size_t size, size_t count,
                             void* userData) noexcept
{
  auto* handle = static_cast<CurlHandle*>(userData);
  auto* download = handle->download;
  try {
    auto& impl = *download->impl_;
    if (impl.io.bytes() >= stream::IoQueue::capacity) {
      handle->pausedForDisk = true;
      return CURL_WRITEFUNC_PAUSE;
    }
    if (size != 0 && count > std::numeric_limits<size_t>::max() / size) {
      fail(download, error_code::FILE_IO_ERROR,
           "Received an oversized output block");
      return CURL_WRITEFUNC_ERROR;
    }
    auto length = size * count;
    if (handle->responseCode >= 300 && handle->responseCode < 400)
      return length;
    if (handle->responseFailure != CurlResponseFailure::None ||
        (handle->ranged && handle->headersComplete && !handle->rangeAccepted &&
         !handle->fullResponseAccepted)) {
      return CURL_WRITEFUNC_ERROR;
    }
    if (download->snapshot_.mediaManifest)
      return CURL_WRITEFUNC_ERROR;
    if (!impl.writer) {
      fail(download, error_code::FILE_OPEN_ERROR,
           "The output file is not open");
      return CURL_WRITEFUNC_ERROR;
    }
    if (length > static_cast<size_t>(std::numeric_limits<int64_t>::max()) ||
        handle->writeOffset > std::numeric_limits<int64_t>::max() -
                                  static_cast<int64_t>(length) ||
        (handle->rangeAccepted &&
         handle->writeOffset + static_cast<int64_t>(length) >
             handle->responseRangeEnd)) {
      fail(download, error_code::HTTP_PROTOCOL_ERROR,
           "The response body exceeds its declared byte range");
      return CURL_WRITEFUNC_ERROR;
    }
    // A helper can own the suffix of an in-flight HTTP response. Stop this
    // request at its assigned boundary without writing into the helper's range.
    const bool assisted =
        handle->rangeAccepted && handle->lease.end < handle->responseRangeEnd;
    if (assisted) {
      length =
          std::min<uint64_t>(length, handle->lease.end - handle->writeOffset);
    }
    if (handle->writeBuffer.empty()) {
      handle->bufferOffset = handle->writeOffset;
    }
    else if (handle->bufferOffset +
                 static_cast<int64_t>(handle->writeBuffer.size()) !=
             handle->writeOffset) {
      stream::flushWriteBuffer(impl, *handle);
      handle->bufferOffset = handle->writeOffset;
    }
    if (length > 0 && handle->writeOffset == handle->lease.begin &&
        A2_LOG_ENABLED(spdlog::level::debug)) {
      char* address = nullptr;
      curl_easy_getinfo(handle->value, CURLINFO_PRIMARY_IP, &address);
      A2_LOG_DEBUG(fmt("component=stream event=route_payload gid=%s family=%s "
                       "bytes=%" PRId64,
                       gid(download).c_str(),
                       address && std::strchr(address, ':') ? "ipv6" : "ipv4",
                       static_cast<int64_t>(length)));
    }
    const auto* bytes = reinterpret_cast<unsigned char*>(data);
    handle->writeBuffer.insert(handle->writeBuffer.end(), bytes,
                               bytes + length);
    handle->writeOffset += static_cast<int64_t>(length);
    if (length > 0) {
      handle->lastPayload = global::wallclock();
    }
    if (length > 0) {
      if (handle->bodySampleStart.isZero()) {
        handle->bodySampleStart = global::wallclock();
        handle->payloadSpeed.reset();
      }
      handle->payloadSpeed.update(length);
    }
    if (handle->writeBuffer.size() >= handle->bufferLimit) {
      stream::flushWriteBuffer(impl, *handle);
    }
    download->snapshot_.completedLength =
        impl.planner.completedLength() + stream::bufferedLength(impl);
    download->snapshot_.sessionDownloadLength += static_cast<int64_t>(length);
    if (impl.group) {
      impl.group->getDownloadContext()->updateDownload(length);
    }
    return assisted && handle->writeOffset == handle->lease.end
               ? CURL_WRITEFUNC_ERROR
               : length;
  }
  catch (const Exception& error) {
    fail(download, error.getErrorCode(), error.what());
  }
  catch (const std::exception& error) {
    fail(download, error_code::FILE_IO_ERROR, error.what());
  }
  catch (...) {
    fail(download, error_code::FILE_IO_ERROR, "Unable to write output data");
  }
  return CURL_WRITEFUNC_ERROR;
}

void CurlHandle::validateResponse(CurlHandle& handle,
                                  const std::string& contentRange)
{
  auto& download = *handle.download;
  auto& impl = *download.impl_;
  handle.headersComplete = true;
  if (handle.responseCode == 412) {
    handle.responseFailure = CurlResponseFailure::PreconditionFailed;
    return;
  }
  if (handle.responseCode == 416) {
    http::parseUnsatisfiedContentRange(contentRange,
                                       handle.unsatisfiedTotalLength);
    return;
  }
  if (handle.responseCode != 200 && handle.responseCode != 206) {
    return;
  }
  if (handle.ranged && handle.encoded) {
    handle.responseFailure = CurlResponseFailure::EncodedRange;
    return;
  }
  handle.responseFailure = identityFailure(impl, handle);
  if (handle.responseCode == 206 && handle.ranged) {
    int64_t first = -1;
    if (!http::parseContentRange(contentRange, first, handle.responseRangeEnd,
                                 handle.responseTotalLength) ||
        first != handle.lease.begin ||
        handle.responseRangeEnd > handle.lease.end) {
      handle.responseFailure = CurlResponseFailure::InvalidRange;
    }
    else if ((impl.planner.totalLength() > 0 &&
              impl.planner.totalLength() != handle.responseTotalLength) ||
             (download.snapshot_.totalLength > 0 &&
              download.snapshot_.totalLength != handle.responseTotalLength)) {
      handle.responseFailure = CurlResponseFailure::LengthChanged;
    }
    if (handle.responseFailure == CurlResponseFailure::None) {
      handle.lease.end = std::min(handle.lease.end, handle.responseTotalLength);
      handle.rangeAccepted = true;
      impl.rangeValidated = true;
      download.snapshot_.totalLength = handle.responseTotalLength;
      stream::rememberIdentity(impl, handle.responseEtag,
                               handle.responseLastModified,
                               handle.responseDate);
    }
  }
  else if (handle.responseCode == 200) {
    if (handle.ranged && impl.maxRangeSize > 0) {
      handle.responseFailure = CurlResponseFailure::RangeUnsupported;
      return;
    }
    if (!handle.rangeValidator.empty() &&
        ((!impl.etag.empty() && handle.responseEtag.empty()) ||
         (impl.etag.empty() && handle.responseLastModified.empty()))) {
      handle.responseFailure = CurlResponseFailure::ValidatorUnavailable;
    }
    if (!handle.encoded && download.snapshot_.totalLength > 0 &&
        handle.responseContentLength >= 0 &&
        download.snapshot_.totalLength != handle.responseContentLength) {
      handle.responseFailure = CurlResponseFailure::LengthChanged;
    }
    if (handle.responseFailure == CurlResponseFailure::None &&
        handle.lease.begin == 0 && impl.planner.completedLength() == 0 &&
        !impl.plannerConfigured) {
      handle.fullResponseAccepted = true;
      impl.fullDownload = true;
      impl.maxConnections = impl.connectionLimit = 1;
      handle.lease.end = std::numeric_limits<int64_t>::max();
      stream::rememberIdentity(impl, handle.responseEtag,
                               handle.responseLastModified,
                               handle.responseDate);
    }
  }
}

size_t CurlHandle::receiveHeader(char* data, size_t size, size_t count,
                                 void* userData) noexcept
{
  auto* handle = static_cast<CurlHandle*>(userData);
  auto* download = handle->download;
  try {
    if (size != 0 && count > std::numeric_limits<size_t>::max() / size) {
      fail(download, error_code::HTTP_PROTOCOL_ERROR, "Oversized HTTP header");
      return CURL_WRITEFUNC_ERROR;
    }
    const auto length = size * count;
    const std::string line(data, length);
    if (http::startsWithHeader(line, "http/")) {
      resetResponse(*handle);
    }
    else if (!handle->headersComplete && (line == "\r\n" || line == "\n")) {
      curl_easy_getinfo(handle->value, CURLINFO_RESPONSE_CODE,
                        &handle->responseCode);
      handle->responseEtag = http::responseHeader(handle->value, "ETag");
      handle->responseLastModified =
          http::responseHeader(handle->value, "Last-Modified");
      handle->responseDate = http::responseHeader(handle->value, "Date");
      const auto encoding = http::trimHeader(
          http::responseHeader(handle->value, "Content-Encoding"));
      handle->encoded =
          !encoding.empty() && !curl_strequal(encoding.c_str(), "identity");
      http::parseContentLength(
          http::responseHeader(handle->value, "Content-Length"),
          handle->responseContentLength);
      if (handle->responseCode >= 200 && handle->responseCode < 300 &&
          handle->lease.begin == 0 &&
          download->impl_->group->getOption()->get(PREF_MEDIA) == "auto" &&
          media::Download::manifestMime(
              http::responseHeader(handle->value, "Content-Type"))) {
        download->snapshot_.mediaManifest = true;
        const auto mime = http::responseHeader(handle->value, "Content-Type");
        download->impl_->group->getOption()->put(
            PREF_MEDIA, curl_strequal(mime.substr(0, mime.find(';')).c_str(),
                                      "application/dash+xml")
                            ? "dash"
                            : "hls");
      }
      validateResponse(*handle,
                       http::responseHeader(handle->value, "Content-Range"));
      if (!download->snapshot_.mediaManifest && handle->responseCode >= 200 &&
          handle->responseCode < 300 &&
          handle->responseFailure == CurlResponseFailure::None &&
          !CurlSession::resolveOutput(download, handle->value)) {
        return CURL_WRITEFUNC_ERROR;
      }
      rememberEndpoint(*handle);
    }
    return length;
  }
  catch (const Exception& error) {
    fail(download, error.getErrorCode(), error.what());
  }
  catch (const std::exception& error) {
    fail(download, error_code::UNKNOWN_ERROR, error.what());
  }
  catch (...) {
    fail(download, error_code::UNKNOWN_ERROR,
         "Unable to process response headers");
  }
  return CURL_WRITEFUNC_ERROR;
}

int CurlHandle::updateProgress(void* userData, curl_off_t downloadTotal,
                               curl_off_t downloaded, curl_off_t,
                               curl_off_t) noexcept
{
  try {
    auto* handle = static_cast<CurlHandle*>(userData);
    auto* download = handle->download;
    auto& impl = *download->impl_;
    const auto total =
        handle->encoded || (handle->ranged && !handle->fullResponseAccepted)
            ? 0
            : handle->lease.begin + std::max<curl_off_t>(0, downloadTotal);
    if (total > 0 && download->snapshot_.totalLength == 0) {
      download->snapshot_.totalLength = total;
    }
    (void)downloaded;
    return impl.stopRequested ? 1 : 0;
  }
  catch (...) {
    return 1;
  }
}

int CurlHandle::debugCallback(CURL* easy, curl_infotype type, char* data,
                              size_t size, void* userData) noexcept
{
  try {
    auto* handle = static_cast<CurlHandle*>(userData);
    void* privateData = nullptr;
    if (!handle || !handle->download ||
        curl_easy_getinfo(easy, CURLINFO_PRIVATE, &privateData) != CURLE_OK ||
        privateData != handle) {
      return 0;
    }

    std::string message;
    const char* direction = nullptr;
    if (type == CURLINFO_TEXT) {
      message.assign(data, size);
      message = http::trimHeader(message);
      if (message.empty() || stream::containsSensitiveCurlText(message) ||
          !usefulCurlText(message)) {
        return 0;
      }
      message = logging::sanitizeUri(message);
      direction = "info";
    }
    else if (type == CURLINFO_HEADER_IN || type == CURLINFO_HEADER_OUT) {
      message = logging::summarizeHttpMessage(std::string(data, size));
      if (message.empty()) {
        return 0;
      }
      direction = type == CURLINFO_HEADER_IN ? "recv" : "send";
    }
    else {
      return 0;
    }

    curl_off_t transferId = -1;
    curl_easy_getinfo(easy, CURLINFO_XFER_ID, &transferId);
    logging::tryWrite(
        spdlog::level::trace, __FILE__, __LINE__,
        fmt("component=stream event=curl_trace gid=%s transfer=%" PRId64
            " direction=%s %s",
            gid(handle->download).c_str(), static_cast<int64_t>(transferId),
            direction, message.c_str()));
  }
  catch (...) {
  }
  return 0;
}

void CurlHandle::fail(CurlDownload* download, error_code::Value errorCode,
                      const std::string& message) noexcept
{
  if (!download || download->snapshot_.errorCode != error_code::UNDEFINED) {
    return;
  }
  download->snapshot_.errorCode = errorCode;
  download->snapshot_.state = CurlSnapshot::State::Error;
  try {
    download->snapshot_.error = message;
  }
  catch (...) {
  }
}

std::string CurlHandle::failureMessage(const CurlHandle& handle,
                                       CURLcode result, long responseCode)
{
  auto detail = handle.errorBuffer[0] != '\0'
                    ? std::string(handle.errorBuffer.data())
                    : std::string(curl_easy_strerror(result));
  detail = stream::containsSensitiveCurlText(detail)
               ? "Sensitive native diagnostic redacted"
               : logging::sanitizeUri(detail);
  const bool httpFailure =
      result == CURLE_HTTP_RETURNED_ERROR || responseCode >= 400;
  return httpFailure ? "HTTP " + std::to_string(responseCode) + ": " + detail
                     : detail;
}

} // namespace aria2
