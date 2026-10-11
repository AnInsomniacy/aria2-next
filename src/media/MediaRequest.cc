/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "MediaTransport.h"
#include "MediaRequest.h"
#include "BufferedFile.h"
#include "Option.h"
#include "Log.h"
#include "fmt.h"
#include "prefs.h"
#include "transport/CurlOptions.h"
#include "transport/HttpHeaders.h"
#include <algorithm>
#include <chrono>
#include <curl/header.h>
#include <memory>
#include <optional>
#include <sstream>

namespace aria2::media {

HttpError::HttpError(long status, CURLcode result, curl_off_t delay)
    : Failure(status == 401 || status == 403
                  ? FailureCode::AuthenticationRequired
                  : FailureCode::ProbeFailed,
              "Media HTTP request failed: status=" + std::to_string(status) +
                  " curl=" + std::to_string(result)),
      status(status),
      retryable(status == 429 || status >= 500 ||
                result == CURLE_COULDNT_CONNECT ||
                result == CURLE_COULDNT_RESOLVE_HOST ||
                result == CURLE_OPERATION_TIMEDOUT ||
                result == CURLE_RECV_ERROR || result == CURLE_PARTIAL_FILE),
      retryAfter(delay)
{
}

Request::Request(CURLM* multi, Control* control, const std::string& path,
                 std::map<CURL*, CURLcode>& completed)
    : multi_(multi),
      control_(control),
      completed_(completed),
      output_(path.c_str(), "wb")
{
  if (!easy_ || !output_)
    throw std::runtime_error("Cannot open media HTTP request");
  std::lock_guard<std::mutex> lock(control_->mutex);
  metadata_ = control_->snapshot.state == "probing";
  set(CURLOPT_WRITEFUNCTION, write);
  set(CURLOPT_WRITEDATA, this);
  set(CURLOPT_XFERINFOFUNCTION, progress);
  set(CURLOPT_XFERINFODATA, control_);
  set(CURLOPT_NOPROGRESS, 0L);
}

Request::~Request() { detach(); }

void Request::detach() noexcept
{
  if (!started_)
    return;
  curl_multi_remove_handle(multi_, handle());
  completed_.erase(handle());
  started_ = false;
}

void Request::header(const std::string& line)
{
  auto* next = curl_slist_append(headers_.get(), line.c_str());
  if (!next)
    throw std::bad_alloc();
  headers_.release();
  headers_.reset(next);
  set(CURLOPT_HTTPHEADER, headers_.get());
}

void Request::start()
{
  if (started_)
    return;
  if (curl_multi_add_handle(multi_, handle()) != CURLM_OK)
    throw std::runtime_error("Cannot schedule media transfer");
  started_ = true;
}

void Request::perform()
{
  start();
  CURLMcode multiResult = CURLM_OK;
  auto done = completed_.find(handle());
  while (done == completed_.end() && !control_->cancel) {
    int running = 0;
    multiResult = curl_multi_perform(multi_, &running);
    if (multiResult != CURLM_OK)
      break;
    int remaining = 0;
    while (auto* message = curl_multi_info_read(multi_, &remaining))
      if (message->msg == CURLMSG_DONE)
        completed_[message->easy_handle] = message->data.result;
    done = completed_.find(handle());
    if (done != completed_.end())
      break;
    multiResult = curl_multi_poll(multi_, nullptr, 0, 100, nullptr);
    if (multiResult != CURLM_OK)
      break;
  }
  const auto result =
      done == completed_.end() ? CURLE_ABORTED_BY_CALLBACK : done->second;
  detach();
  if (output_.close() != 0)
    throw std::runtime_error("Cannot close media cache file");
  if (metadataTooLarge_)
    throw Failure(FailureCode::UnsupportedSource,
                  "Media inspection metadata exceeds 16 MiB");
  long status = 0;
  curl_off_t retryAfter = 0;
  curl_easy_getinfo(handle(), CURLINFO_RESPONSE_CODE, &status);
  curl_easy_getinfo(handle(), CURLINFO_RETRY_AFTER, &retryAfter);
  if (result != CURLE_OK || multiResult != CURLM_OK || status >= 400)
    throw HttpError(status, result, retryAfter);
}

size_t Request::write(char* data, size_t size, size_t count,
                      void* opaque) noexcept
{
  auto& request = *static_cast<Request*>(opaque);
  if (request.control_->cancel)
    return CURL_WRITEFUNC_ERROR;
  const auto length = size * count;
  long status = 0;
  curl_easy_getinfo(request.handle(), CURLINFO_RESPONSE_CODE, &status);
  if (status >= 300 && status < 400)
    return length;
  if (request.metadata_ && length > 16 * 1024 * 1024 - request.bytes_) {
    request.metadataTooLarge_ = true;
    return CURL_WRITEFUNC_ERROR;
  }
  if (request.output_.write(data, length) != length)
    return CURL_WRITEFUNC_ERROR;
  request.bytes_ += length;
  request.control_->received.fetch_add(length);
  return length;
}

int Request::progress(void* opaque, curl_off_t, curl_off_t, curl_off_t,
                      curl_off_t) noexcept
{
  return static_cast<Control*>(opaque)->cancel;
}

namespace {
void configureNetwork(Request& request, const Option* option,
                      const std::string& url, CURLSH* share,
                      std::string& loadedCookies)
{
  request.set(CURLOPT_URL, url.c_str());
  request.set(CURLOPT_SHARE, share);
  request.set(CURLOPT_PROTOCOLS_STR, "http,https");
  request.set(CURLOPT_DISALLOW_USERNAME_IN_URL, 1L);
  request.set(CURLOPT_FOLLOWLOCATION, 0L);
  request.set(CURLOPT_FAILONERROR, 1L);
  request.set(CURLOPT_NOSIGNAL, 1L);
  request.set(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
  request.set(CURLOPT_TCP_KEEPALIVE, 1L);
  request.set(CURLOPT_COOKIEFILE, "");
  request.set(CURLOPT_CONNECTTIMEOUT,
              static_cast<long>(option->getAsInt(PREF_CONNECT_TIMEOUT)));
  request.set(CURLOPT_LOW_SPEED_TIME,
              static_cast<long>(option->getAsInt(PREF_TIMEOUT)));
  request.set(CURLOPT_LOW_SPEED_LIMIT, 1L);
  request.set(CURLOPT_USERAGENT, option->get(PREF_USER_AGENT).c_str());
  const auto tls = http::configureTls(request.handle(), option);
  if (tls != CURLE_OK)
    throw std::runtime_error(curl_easy_strerror(tls));
  if (!option->blank(PREF_INTERFACE))
    request.set(CURLOPT_INTERFACE, option->get(PREF_INTERFACE).c_str());
  if (option->getAsBool(PREF_DISABLE_IPV6))
    request.set(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
  const auto proxy =
      url.rfind("https:", 0) == 0 ? PREF_HTTPS_PROXY : PREF_HTTP_PROXY;
  const bool useAll = option->blank(proxy);
  const auto& proxyUrl = option->get(useAll ? PREF_ALL_PROXY : proxy);
  if (!proxyUrl.empty())
    request.set(CURLOPT_PROXY, proxyUrl.c_str());
  const auto user = useAll                      ? PREF_ALL_PROXY_USER
                    : proxy == PREF_HTTPS_PROXY ? PREF_HTTPS_PROXY_USER
                                                : PREF_HTTP_PROXY_USER;
  const auto password = useAll                      ? PREF_ALL_PROXY_PASSWD
                        : proxy == PREF_HTTPS_PROXY ? PREF_HTTPS_PROXY_PASSWD
                                                    : PREF_HTTP_PROXY_PASSWD;
  if (!option->blank(user)) {
    request.set(CURLOPT_PROXYUSERNAME, option->get(user).c_str());
    request.set(CURLOPT_PROXYPASSWORD, option->get(password).c_str());
  }
  if (!option->blank(PREF_NO_PROXY)) {
    const auto bypass = http::noProxyFor(option->get(PREF_NO_PROXY), url);
    request.set(CURLOPT_NOPROXY, bypass.c_str());
  }
  if (!option->blank(PREF_REFERER))
    request.set(CURLOPT_REFERER, option->get(PREF_REFERER).c_str());
  if (const auto result =
          http::loadCookies(request.handle(), option, loadedCookies);
      result != CURLE_OK)
    throw std::runtime_error(std::string("Unable to load cookies: ") +
                             curl_easy_strerror(result));
}

void configureHeaders(Request& request, const Option* option,
                      const std::string& source, const std::string& url,
                      const std::vector<RequestContext>& contexts)
{
  const auto context =
      std::find_if(contexts.begin(), contexts.end(), [&](const auto& item) {
        return http::sameOrigin(item.url, url);
      });
  if (context != contexts.end()) {
    for (const auto& [name, value] : context->headers) {
      if (name == "cookie")
        request.set(CURLOPT_COOKIE, value.c_str());
      else if (name == "referer")
        request.set(CURLOPT_REFERER, value.c_str());
      else if (name == "user-agent")
        request.set(CURLOPT_USERAGENT, value.c_str());
      else
        request.header(name + (value.empty() ? ";" : ": " + value));
    }
    return;
  }
  if (!http::sameOrigin(source, url))
    return;
  std::istringstream configured(option->get(PREF_HEADER));
  std::string line;
  while (std::getline(configured, line))
    if (!line.empty() && !http::startsWithHeader(line, "Range:") &&
        !http::startsWithHeader(line, "Host:"))
      request.header(line);
  if (!option->blank(PREF_HTTP_USER)) {
    request.set(CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    request.set(CURLOPT_USERNAME, option->get(PREF_HTTP_USER).c_str());
    request.set(CURLOPT_PASSWORD, option->get(PREF_HTTP_PASSWD).c_str());
  }
}
} // namespace

std::unique_ptr<Request> Transport::open(const std::string& url, int64_t begin,
                                         int64_t end,
                                         const std::string& temporary)
{
  auto transfer =
      std::make_unique<Request>(multi_, control_.get(), temporary, completed_);
  configureNetwork(*transfer, option_, url, share_, loadedCookies_);
  configureHeaders(*transfer, option_, source_, url, contexts_);
  const auto range =
      begin || end >= 0
          ? std::to_string(begin) + "-" + (end >= 0 ? std::to_string(end) : "")
          : std::string();
  transfer->set(CURLOPT_ACCEPT_ENCODING, range.empty() ? "" : "identity");
  if (!range.empty())
    transfer->set(CURLOPT_RANGE, range.c_str());
  // Concurrent segment requests share the task's rate limit.
  transfer->set(CURLOPT_MAX_RECV_SPEED_LARGE,
                static_cast<curl_off_t>(control_->downloadLimit.load() /
                                        static_cast<int64_t>(
                                            prefetchLimit_ + 1)));
  return transfer;
}

Resource Transport::describe(Request& transfer, const std::string& url,
                             int64_t begin, int64_t end,
                             const std::string& temporary)
{
  auto* handle = transfer.handle();
  long status = 0;
  curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
  if (status < 200 || status >= 300)
    throw HttpError(status, CURLE_OK, 0);
  const bool ranged = begin || end >= 0;
  if ((ranged && status != 206) ||
      (status == 206 &&
       !http::matchesRange(handle, begin, end, transfer.bytes())))
    throw std::runtime_error("Server returned an incorrect media byte range");
  char* mime = nullptr;
  curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &mime);
  Resource resource{temporary, url, mime ? mime : "",
                    static_cast<int64_t>(transfer.bytes())};
  curl_off_t elapsed = 0, firstByte = 0;
  curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME_T, &elapsed);
  curl_easy_getinfo(handle, CURLINFO_STARTTRANSFER_TIME_T, &firstByte);
  resource.utcStart = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count() -
                      (elapsed - firstByte) / 1000;
  curl_header* field = nullptr;
  while ((field = curl_easy_nextheader(handle, CURLH_HEADER, -1, field)))
    resource.headers[field->name] = field->value;
  A2_LOG_DEBUG(
      fmt("component=media event=http_complete url=%s status=%ld bytes=%llu",
          logging::sanitizeUri(url).c_str(), status,
          static_cast<unsigned long long>(transfer.bytes())));
  return resource;
}

Resource Transport::request(const std::string& url, int64_t begin, int64_t end,
                            const std::string& temporary)
{
  auto current = url;
  for (unsigned redirects = 0; redirects <= 10; ++redirects) {
    auto transfer = open(current, begin, end, temporary);
    transfer->perform();
    long status = 0;
    curl_easy_getinfo(transfer->handle(), CURLINFO_RESPONSE_CODE, &status);
    if (status >= 300 && status < 400) {
      char* location = nullptr;
      curl_easy_getinfo(transfer->handle(), CURLINFO_REDIRECT_URL, &location);
      if (!location)
        throw Failure(FailureCode::UnsupportedSource,
                      "Media redirect has no destination");
      current = location; // libcurl resolves relative Location values.
      continue;
    }
    return describe(*transfer, current, begin, end, temporary);
  }
  throw Failure(FailureCode::UnsupportedSource, "Too many media redirects");
}

std::optional<Resource> Transport::collect(const std::string& key,
                                           const std::string& url,
                                           int64_t begin, int64_t end,
                                           const std::string& temporary)
{
  auto found = prefetched_.find(key);
  if (found == prefetched_.end())
    return std::nullopt;
  auto transfer = std::move(found->second);
  prefetched_.erase(found);
  try {
    transfer->perform();
    long status = 0;
    curl_easy_getinfo(transfer->handle(), CURLINFO_RESPONSE_CODE, &status);
    // Redirects and failures repeat through the ordinary request path, which
    // owns redirect limits and retry policy.
    if (status >= 300 && status < 400)
      return std::nullopt;
    return describe(*transfer, url, begin, end, temporary);
  }
  catch (const std::exception& error) {
    A2_LOG_DEBUG(fmt("component=media event=prefetch_discarded url=%s "
                     "message=%s",
                     logging::sanitizeUri(url).c_str(), error.what()));
    return std::nullopt;
  }
}
} // namespace aria2::media
