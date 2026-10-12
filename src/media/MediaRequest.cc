/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"
#include "media/MediaTransport.h"
#include "media/MediaRequest.h"
#include "base/Log.h"
#include "base/Uri.h"
#include "http/Request.h"
#include "media/MediaFiles.h"
#include <algorithm>
#include <chrono>
#include <curl/header.h>
#include <memory>
#include <optional>
#include <utility>

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

void Throttle::resume()
{
  if (paused.empty() || !bucket.available())
    return;
  // Resuming may deliver data that pauses a handle again.
  for (auto* handle : std::exchange(paused, {}))
    curl_easy_pause(handle, CURLPAUSE_CONT);
}

void Throttle::forget(CURL* handle)
{
  paused.erase(std::remove(paused.begin(), paused.end(), handle), paused.end());
}

Request::Request(CURLM* multi, Control* control, Throttle& throttle,
                 const std::string& path, std::map<CURL*, CURLcode>& completed)
    : multi_(multi),
      control_(control),
      throttle_(throttle),
      completed_(completed)
{
#ifdef _WIN32
  output_.reset(_wfopen(nativePath(path).c_str(), L"wb"));
#else
  output_.reset(std::fopen(path.c_str(), "wb"));
#endif
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
  throttle_.forget(handle());
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
    throttle_.bucket.setRate(control_->downloadLimit);
    throttle_.resume();
    multiResult = curl_multi_poll(multi_, nullptr, 0, 100, nullptr);
    if (multiResult != CURLM_OK)
      break;
  }
  const auto result =
      done == completed_.end() ? CURLE_ABORTED_BY_CALLBACK : done->second;
  detach();
  if (std::fclose(output_.release()) != 0)
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
  if (!request.throttle_.bucket.available()) {
    request.throttle_.paused.push_back(request.handle());
    return CURL_WRITEFUNC_PAUSE;
  }
  if (std::fwrite(data, 1, length, request.output_.get()) != length)
    return CURL_WRITEFUNC_ERROR;
  request.throttle_.bucket.charge(static_cast<int64_t>(length));
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

// A 206 response must carry exactly the requested bytes, or stop short only
// where the resource ends (RFC 9110 14.1.1: a range past the end is clamped).
bool matchesRange(CURL* handle, int64_t begin, int64_t end, size_t bytes)
{
  curl_header* field = nullptr;
  if (curl_easy_header(handle, "Content-Range", 0, CURLH_HEADER, -1, &field) !=
      CURLHE_OK)
    return false;
  const auto range = http::parseContentRange(field->value);
  if (!range || range->first != begin ||
      range->last - range->first + 1 != static_cast<int64_t>(bytes))
    return false;
  return end < 0 || range->last == end ||
         (range->last < end && range->total == range->last + 1);
}

} // namespace

std::unique_ptr<Request> Transport::open(const std::string& url, int64_t begin,
                                         int64_t end,
                                         const std::string& temporary)
{
  auto transfer = std::make_unique<Request>(multi_, control_.get(), *throttle_,
                                            temporary, completed_);
  // Captured browser headers replace the configured ones for their origin.
  const auto context =
      std::find_if(contexts_.begin(), contexts_.end(), [&](const auto& item) {
        return uri::sameOrigin(item.url, url);
      });
  http::configureRequest(transfer->handle(), share_, loadedCookies_, *option_,
                         url, source_, [&](const std::string& line) {
                           if (context == contexts_.end())
                             transfer->header(line);
                         });
  // The media client follows redirects itself so each hop is checked.
  transfer->set(CURLOPT_PROTOCOLS_STR, "http,https");
  transfer->set(CURLOPT_FOLLOWLOCATION, 0L);
  transfer->set(CURLOPT_DISALLOW_USERNAME_IN_URL, 1L);
  transfer->set(CURLOPT_NOSIGNAL, 1L);
  transfer->set(CURLOPT_HTTP_VERSION,
                static_cast<long>(CURL_HTTP_VERSION_2TLS));
  if (context != contexts_.end()) {
    for (const auto& [name, value] : context->headers) {
      if (name == "cookie")
        transfer->set(CURLOPT_COOKIE, value.c_str());
      else if (name == "referer")
        transfer->set(CURLOPT_REFERER, value.c_str());
      else if (name == "user-agent")
        transfer->set(CURLOPT_USERAGENT, value.c_str());
      else
        transfer->header(name + (value.empty() ? ";" : ": " + value));
    }
  }
  const auto range =
      begin || end >= 0
          ? std::to_string(begin) + "-" + (end >= 0 ? std::to_string(end) : "")
          : std::string();
  transfer->set(CURLOPT_ACCEPT_ENCODING, range.empty() ? "" : "identity");
  if (!range.empty())
    transfer->set(CURLOPT_RANGE, range.c_str());
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
      (status == 206 && !matchesRange(handle, begin, end, transfer.bytes())))
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
  log::debug("media: {} status={} bytes={}", uri::redact(url), status,
             transfer.bytes());
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
    log::debug("media: prefetch of {} discarded: {}", uri::redact(url),
               error.what());
    return std::nullopt;
  }
}
} // namespace aria2::media
