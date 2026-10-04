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
#include "RangePlanner.h"
#include "a2functional.h"
#include "a2netcompat.h"

#include "CurlSession.h"

#include "error_code.h"
#include "spdlog/common.h"
#include "transport/CurlOptions.h"
#include "transport/HttpHeaders.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <curl/easy.h>
#include <curl/curl.h>
#include <curl/system.h>
#include <curl/multi.h>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "ApplicationStatePath.h"
#include "CurlDownload.h"
#include "media/MediaDownload.h"
#include "CurlDownloadCommand.h"
#include "CurlDownloadImpl.h"
#include "stream/CurlHandle.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Log.h"
#include "Option.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "fmt.h"
#include "prefs.h"
#include "uri.h"
#include "support/Text.h"
#include "wallclock.h"

#include "FileEntry.h"
namespace aria2 {
namespace {
std::string proxyFor(const Option* option, const std::string& uriValue)
{
  uri::UriStruct parsed;
  if (uri::parse(parsed, uriValue)) {
    if (util::strieq(parsed.protocol, "https") &&
        !option->blank(PREF_HTTPS_PROXY)) {
      return option->get(PREF_HTTPS_PROXY);
    }
    if (util::strieq(parsed.protocol, "http") &&
        !option->blank(PREF_HTTP_PROXY)) {
      return option->get(PREF_HTTP_PROXY);
    }
  }
  return option->get(PREF_ALL_PROXY);
}

std::string proxyUserFor(const Option* option, const std::string& protocol)
{
  const auto pref = util::strieq(protocol, "https")  ? PREF_HTTPS_PROXY_USER
                    : util::strieq(protocol, "http") ? PREF_HTTP_PROXY_USER
                                                     : PrefPtr();
  return !pref || option->blank(pref) ? option->get(PREF_ALL_PROXY_USER)
                                      : option->get(pref);
}

std::string proxyPasswordFor(const Option* option, const std::string& protocol)
{
  const auto pref = util::strieq(protocol, "https")  ? PREF_HTTPS_PROXY_PASSWD
                    : util::strieq(protocol, "http") ? PREF_HTTP_PROXY_PASSWD
                                                     : PrefPtr();
  return !pref || option->blank(pref) ? option->get(PREF_ALL_PROXY_PASSWD)
                                      : option->get(pref);
}

void markUriUsed(RequestGroup* group, const std::string& uriValue)
{
  auto file = group->getDownloadContext()->getFirstFileEntry();
  auto& remaining = file->getRemainingUris();
  remaining.erase(std::remove(remaining.begin(), remaining.end(), uriValue),
                  remaining.end());
  auto& spent = file->getSpentUris();
  if (std::find(spent.begin(), spent.end(), uriValue) == spent.end()) {
    spent.push_back(uriValue);
  }
}
} // namespace

bool CurlSession::createHandle(const std::shared_ptr<CurlDownload>& download,
                               RangeLease lease, bool primary, bool ranged,
                               long addressFamily, const std::string& redirect,
                               unsigned redirects)
{
  auto& impl = *download->impl_;
  const auto taskOption = impl.group->getOption().get();
  if (impl.http && impl.maxRangeSize > 0) {
    ranged = true;
    if (lease.length() > impl.maxRangeSize) {
      auto remainder = lease;
      lease.end = lease.begin + impl.maxRangeSize;
      remainder.begin = lease.end;
      if (impl.plannerConfigured) {
        impl.planner.enqueue(remainder);
      }
    }
  }
  auto transfer = std::unique_ptr<CurlHandle>(new CurlHandle());
  transfer->download = download.get();
  transfer->lease = lease;
  transfer->connectionEpoch = impl.connectionEpoch;
  transfer->writeOffset = lease.begin;
  transfer->bufferLimit = static_cast<size_t>(std::max<int64_t>(
      256_k, std::min<int64_t>(1_m, 32_m / impl.maxConnections)));
  transfer->writeBuffer.reserve(transfer->bufferLimit);
  transfer->primary = primary;
  transfer->ranged = impl.http && ranged && !impl.dryRun;
  auto* easy = curl_easy_init();
  if (!easy) {
    CurlHandle::fail(download.get(), error_code::NETWORK_PROBLEM,
                     "curl_easy_init returned null");
    return false;
  }
  transfer->value = easy;
  auto*& headers = transfer->headers;
  auto setOption = [&](CURLoption option, auto value, const char* name) {
    const auto result = curl_easy_setopt(easy, option, value);
    if (result == CURLE_OK) {
      return true;
    }
    const auto message =
        fmt("Unable to configure %s: %s", name, curl_easy_strerror(result));
    A2_LOG_ERROR(fmt("component=stream event=handle_setup_failed gid=%s "
                     "option=%s curl=%d message=%s",
                     CurlHandle::gid(download.get()).c_str(), name,
                     static_cast<int>(result),
                     logging::sanitizeText(message).c_str()));
    CurlHandle::fail(download.get(), error_code::NETWORK_PROBLEM, message);
    return false;
  };
  auto appendHeader = [&](const std::string& value) {
    auto* replacement = curl_slist_append(headers, value.c_str());
    if (replacement) {
      headers = replacement;
      return true;
    }
    CurlHandle::fail(download.get(), error_code::UNKNOWN_ERROR,
                     "Unable to allocate HTTP request headers");
    return false;
  };
#define SET_CURL_OPTION(name, value)                                           \
  do {                                                                         \
    if (!setOption(name, value, #name)) {                                      \
      transfer->reset();                                                       \
      return false;                                                            \
    }                                                                          \
  } while (0)
  const auto uriIndex = lease.uriIndex % impl.uris.size();
  auto& endpoint = impl.endpoint(uriIndex, addressFamily);
  const auto& originalUri = impl.uris[uriIndex];
  const auto& uriValue = !redirect.empty()      ? redirect
                         : endpoint.uri.empty() ? originalUri
                                                : endpoint.uri;
  transfer->redirects = redirects;
  transfer->manualRedirect =
      taskOption->get(PREF_NO_PROXY).find('*') != std::string::npos;
  transfer->endpointGeneration = endpoint.generation;
  transfer->resolvingEndpoint = impl.http && endpoint.uri.empty();
  transfer->redirectedEndpoint = uriValue != originalUri;
  transfer->addressFamily = addressFamily;
  const bool sourceCredentials = http::sameOrigin(originalUri, uriValue);
  impl.currentUri = originalUri;
  download->snapshot_.currentUri = originalUri;
  markUriUsed(impl.group, originalUri);
  SET_CURL_OPTION(CURLOPT_URL, uriValue.c_str());
  if (transport_.share()) {
    SET_CURL_OPTION(CURLOPT_SHARE, transport_.share());
  }
  SET_CURL_OPTION(CURLOPT_PRIVATE, transfer.get());
  SET_CURL_OPTION(CURLOPT_ERRORBUFFER, transfer->errorBuffer.data());
  SET_CURL_OPTION(CURLOPT_DEBUGFUNCTION, CurlHandle::debugCallback);
  SET_CURL_OPTION(CURLOPT_DEBUGDATA, transfer.get());
  SET_CURL_OPTION(CURLOPT_VERBOSE,
                  A2_LOG_ENABLED(spdlog::level::trace) ? 1L : 0L);
  SET_CURL_OPTION(CURLOPT_PROTOCOLS_STR, "http,https,sftp");
  SET_CURL_OPTION(CURLOPT_REDIR_PROTOCOLS_STR, "http,https,sftp");
  SET_CURL_OPTION(CURLOPT_FOLLOWLOCATION, transfer->manualRedirect ? 0L : 1L);
  SET_CURL_OPTION(CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
  SET_CURL_OPTION(CURLOPT_FAILONERROR, 1L);
  SET_CURL_OPTION(CURLOPT_MAXREDIRS, 10L);
  // Parallel ranges retain independent connections rather than sharing one
  // HTTP/2 congestion and flow-control window. Ordinary transfers negotiate.
  SET_CURL_OPTION(CURLOPT_HTTP_VERSION,
                  transfer->ranged && impl.maxConnections > 1
                      ? CURL_HTTP_VERSION_1_1
                      : CURL_HTTP_VERSION_2TLS);
  SET_CURL_OPTION(CURLOPT_NOSIGNAL, 1L);
  // Keep paused, automatically decoded responses bounded to curl's normal
  // input chunks; a megabyte of compressed input can expand dramatically.
  SET_CURL_OPTION(CURLOPT_BUFFERSIZE, static_cast<long>(CURL_MAX_WRITE_SIZE));
  SET_CURL_OPTION(CURLOPT_TCP_KEEPALIVE, 1L);
  SET_CURL_OPTION(CURLOPT_DNS_CACHE_TIMEOUT, 300L);
  SET_CURL_OPTION(CURLOPT_SOCKOPTFUNCTION, socketOptionCallback);
  SET_CURL_OPTION(CURLOPT_SOCKOPTDATA, const_cast<Option*>(taskOption));
  SET_CURL_OPTION(CURLOPT_HTTPAUTH, CURLAUTH_ANY);
  SET_CURL_OPTION(CURLOPT_NOBODY, impl.dryRun ? 1L : 0L);
  SET_CURL_OPTION(CURLOPT_FILETIME,
                  taskOption->getAsBool(PREF_REMOTE_TIME) ? 1L : 0L);
  SET_CURL_OPTION(CURLOPT_WRITEFUNCTION, CurlHandle::writeData);
  SET_CURL_OPTION(CURLOPT_WRITEDATA, transfer.get());
  SET_CURL_OPTION(CURLOPT_HEADERFUNCTION, CurlHandle::receiveHeader);
  SET_CURL_OPTION(CURLOPT_HEADERDATA, transfer.get());
  SET_CURL_OPTION(CURLOPT_XFERINFOFUNCTION, CurlHandle::updateProgress);
  SET_CURL_OPTION(CURLOPT_XFERINFODATA, transfer.get());
  SET_CURL_OPTION(CURLOPT_NOPROGRESS, 0L);
  SET_CURL_OPTION(CURLOPT_CONNECTTIMEOUT,
                  taskOption->getAsInt(PREF_CONNECT_TIMEOUT));
  SET_CURL_OPTION(CURLOPT_LOW_SPEED_LIMIT,
                  std::max(1, taskOption->getAsInt(PREF_LOWEST_SPEED_LIMIT)));
  SET_CURL_OPTION(CURLOPT_LOW_SPEED_TIME, taskOption->getAsInt(PREF_TIMEOUT));
  SET_CURL_OPTION(CURLOPT_USERAGENT, taskOption->get(PREF_USER_AGENT).c_str());
  SET_CURL_OPTION(CURLOPT_FORBID_REUSE,
                  taskOption->getAsBool(PREF_ENABLE_HTTP_KEEP_ALIVE) ? 0L : 1L);
  SET_CURL_OPTION(CURLOPT_ACCEPT_ENCODING,
                  !transfer->ranged &&
                          taskOption->getAsBool(PREF_HTTP_ACCEPT_GZIP)
                      ? ""
                      : "identity");
  SET_CURL_OPTION(CURLOPT_HTTP_CONTENT_DECODING, transfer->ranged ? 0L : 1L);
  if (taskOption->getAsBool(PREF_HTTP_NO_CACHE)) {
    if (!appendHeader("Cache-Control: no-cache") ||
        !appendHeader("Pragma: no-cache")) {
      transfer->reset();
      return false;
    }
  }
  if (const auto result = http::configureTls(transfer->value, taskOption);
      result != CURLE_OK) {
    const auto message =
        fmt("Cannot configure TLS: %s", curl_easy_strerror(result));
    A2_LOG_ERROR(message);
    CurlHandle::fail(download.get(), error_code::NETWORK_PROBLEM, message);
    transfer->reset();
    return false;
  }
  if (!taskOption->blank(PREF_INTERFACE)) {
    SET_CURL_OPTION(CURLOPT_INTERFACE, taskOption->get(PREF_INTERFACE).c_str());
  }
  if (taskOption->getAsBool(PREF_DISABLE_IPV6)) {
    SET_CURL_OPTION(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
  }
  else if (transfer->addressFamily != CURL_IPRESOLVE_WHATEVER) {
    SET_CURL_OPTION(CURLOPT_IPRESOLVE, transfer->addressFamily);
  }
  if (!taskOption->blank(PREF_PRIVATE_KEY)) {
    SET_CURL_OPTION(CURLOPT_SSH_PRIVATE_KEYFILE,
                    taskOption->get(PREF_PRIVATE_KEY).c_str());
  }
  if (impl.http && sourceCredentials && !taskOption->blank(PREF_HTTP_USER)) {
    SET_CURL_OPTION(CURLOPT_USERNAME, taskOption->get(PREF_HTTP_USER).c_str());
    SET_CURL_OPTION(CURLOPT_PASSWORD,
                    taskOption->get(PREF_HTTP_PASSWD).c_str());
  }
  else if (!impl.http && !taskOption->blank(PREF_SFTP_USER)) {
    SET_CURL_OPTION(CURLOPT_USERNAME, taskOption->get(PREF_SFTP_USER).c_str());
    SET_CURL_OPTION(CURLOPT_PASSWORD,
                    taskOption->get(PREF_SFTP_PASSWD).c_str());
  }
  else if (!taskOption->getAsBool(PREF_NO_NETRC)) {
    SET_CURL_OPTION(CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
    if (!taskOption->blank(PREF_NETRC_PATH)) {
      SET_CURL_OPTION(CURLOPT_NETRC_FILE,
                      taskOption->get(PREF_NETRC_PATH).c_str());
    }
  }
  if (!impl.http && !taskOption->blank(PREF_SSH_HOST_KEY_SHA256)) {
    SET_CURL_OPTION(CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256,
                    taskOption->get(PREF_SSH_HOST_KEY_SHA256).c_str());
  }
  if (!taskOption->blank(PREF_REFERER)) {
    SET_CURL_OPTION(CURLOPT_REFERER, taskOption->get(PREF_REFERER).c_str());
  }
  const auto proxy = proxyFor(taskOption, uriValue);
  if (!proxy.empty()) {
    SET_CURL_OPTION(CURLOPT_PROXY, proxy.c_str());
    uri::UriStruct parsed;
    uri::parse(parsed, uriValue);
    const auto proxyUser = proxyUserFor(taskOption, parsed.protocol);
    const auto proxyPassword = proxyPasswordFor(taskOption, parsed.protocol);
    if (!proxyUser.empty()) {
      SET_CURL_OPTION(CURLOPT_PROXYUSERNAME, proxyUser.c_str());
      SET_CURL_OPTION(CURLOPT_PROXYPASSWORD, proxyPassword.c_str());
    }
  }
  if (!taskOption->blank(PREF_NO_PROXY)) {
    const auto bypass =
        http::noProxyFor(taskOption->get(PREF_NO_PROXY), uriValue);
    SET_CURL_OPTION(CURLOPT_NOPROXY, bypass.c_str());
  }
  if (impl.http) {
    SET_CURL_OPTION(CURLOPT_COOKIEFILE, "");
  }
  if (!taskOption->blank(PREF_LOAD_COOKIES)) {
    SET_CURL_OPTION(CURLOPT_COOKIEFILE,
                    taskOption->get(PREF_LOAD_COOKIES).c_str());
  }
  if (!taskOption->blank(PREF_SAVE_COOKIES)) {
    SET_CURL_OPTION(CURLOPT_COOKIEJAR,
                    taskOption->get(PREF_SAVE_COOKIES).c_str());
  }
  std::istringstream configuredHeaders(taskOption->get(PREF_HEADER));
  std::string header;
  while (std::getline(configuredHeaders, header)) {
    // libcurl suppresses these on a cross-origin redirect. Starting a new
    // range at that destination must preserve the same credential boundary.
    if (!sourceCredentials &&
        (http::startsWithHeader(header, "Authorization:") ||
         http::startsWithHeader(header, "Cookie:"))) {
      continue;
    }
    if (!header.empty() && !appendHeader(header)) {
      transfer->reset();
      return false;
    }
  }
  if (transfer->ranged) {
    transfer->range =
        lease.end == std::numeric_limits<int64_t>::max()
            ? std::to_string(lease.begin) + '-'
            : std::to_string(lease.begin) + '-' + std::to_string(lease.end - 1);
    SET_CURL_OPTION(CURLOPT_RANGE, transfer->range.c_str());
  }
  else if (lease.begin > 0) {
    SET_CURL_OPTION(CURLOPT_RESUME_FROM_LARGE,
                    static_cast<curl_off_t>(lease.begin));
  }
  // Range validators qualify the requested representation, not a token or
  // redirect endpoint. libcurl owns redirects; response validation prevents a
  // full 200 response from being written at the requested range offset.
  if (transfer->ranged) {
    transfer->rangeValidator =
        !impl.etag.empty() ? impl.etag : impl.lastModified;
    if (!transfer->rangeValidator.empty() &&
        !appendHeader("If-Range: " + transfer->rangeValidator)) {
      transfer->reset();
      return false;
    }
  }
  if (headers) {
    SET_CURL_OPTION(CURLOPT_HTTPHEADER, headers);
  }
  const auto addResult = curl_multi_add_handle(transport_.get(), easy);
  const auto result = addResult == CURLM_OK;
  if (result) {
    if (transfer->resolvingEndpoint) {
      endpoint.resolving = true;
    }
    impl.handles.push_back(std::move(transfer));
  }
  else {
    const auto message = std::string("Unable to add curl transfer: ") +
                         curl_multi_strerror(addResult);
    A2_LOG_ERROR(fmt("component=stream event=handle_add_failed gid=%s "
                     "curlm=%d message=%s",
                     CurlHandle::gid(download.get()).c_str(),
                     static_cast<int>(addResult),
                     logging::sanitizeText(message).c_str()));
    CurlHandle::fail(download.get(), error_code::NETWORK_PROBLEM, message);
    transfer->reset();
  }
#undef SET_CURL_OPTION
  return result;
}

int CurlSession::socketOptionCallback(void* userData, curl_socket_t socket,
                                      curlsocktype) noexcept
{
  try {
    const auto* option = static_cast<const Option*>(userData);
    const auto receiveBuffer = option->getAsInt(PREF_SOCKET_RECV_BUFFER_SIZE);
    if (receiveBuffer > 0) {
      setsockopt(socket, SOL_SOCKET, SO_RCVBUF,
                 reinterpret_cast<const char*>(&receiveBuffer),
                 sizeof(receiveBuffer));
    }
    const auto trafficClass = option->getAsInt(PREF_DSCP) << 2;
    if (trafficClass > 0) {
      setsockopt(socket, IPPROTO_IP, IP_TOS,
                 reinterpret_cast<const char*>(&trafficClass),
                 sizeof(trafficClass));
#ifdef IPV6_TCLASS
      setsockopt(socket, IPPROTO_IPV6, IPV6_TCLASS,
                 reinterpret_cast<const char*>(&trafficClass),
                 sizeof(trafficClass));
#endif
    }
    return CURL_SOCKOPT_OK;
  }
  catch (...) {
    return CURL_SOCKOPT_ERROR;
  }
}

} // namespace aria2
