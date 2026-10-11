/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// One-byte requests that tell a client what a download would receive before
// it commits to one (aria2.probe).
#include "http/Probe.h"

#include "base/ContentDisposition.h"
#include "base/Text.h"
#include "http/Client.h"
#include "http/Request.h"
#include "options/Options.h"

#include <algorithm>

namespace aria2::http {
namespace {

// The whole exchange, redirects included, must fit in this budget: a client
// waits for the answer before it decides where the download goes.
constexpr long PROBE_TIMEOUT_SECONDS = 20;
constexpr int64_t PROBE_CONNECT_TIMEOUT_SECONDS = 10;

class Probe final : public Transfer {
public:
  Probe(Client& client, std::function<void(ProbeResult)> done)
      : Transfer(client), done_(std::move(done))
  {
  }

  std::shared_ptr<Probe> self;

private:
  void onHeader(std::string_view line) override
  {
    if (line.starts_with("HTTP/")) {
      const auto space = line.find(' ');
      status_ = text::toInt(text::trim(line.substr(space + 1, 3))).value_or(0);
      result_ = {};
      complete_ = false;
      contentLength_ = -1;
      contentRange_.clear();
      acceptRanges_ = false;
      return;
    }
    const auto value = text::trim(line);
    if (value.empty()) {
      // Redirect bodies never reach onData; a success ends the exchange.
      complete_ = status_ >= 200 && status_ < 300;
      return;
    }
    const auto colon = value.find(':');
    if (colon == std::string_view::npos) {
      return;
    }
    const auto name = text::lower(value.substr(0, colon));
    const std::string field(text::trim(value.substr(colon + 1)));
    if (name == "content-type") {
      result_.contentType = field;
    }
    else if (name == "content-disposition") {
      result_.filename = contentDispositionFileName(field);
    }
    else if (name == "content-length") {
      contentLength_ = text::toInt(field).value_or(-1);
    }
    else if (name == "content-range") {
      contentRange_ = field;
    }
    else if (name == "accept-ranges") {
      acceptRanges_ = text::lower(field) == "bytes";
    }
  }

  // The headers are all a probe needs; stop at the first body byte.
  size_t onData(const char*, size_t size) override
  {
    return complete_ ? 0 : size;
  }

  void onDone(CURLcode code) override
  {
    auto result = std::move(result_);
    result.status = responseCode();
    result.finalUrl = effectiveUrl();
    const auto range = parseContentRange(contentRange_);
    if (result.status == 206 && range) {
      result.size = range->total;
      result.resumable = true;
    }
    else if (result.status == 416 && range && range->total == 0) {
      // An empty file cannot satisfy "bytes=0-0".
      result.status = 200;
      result.size = 0;
      complete_ = true;
    }
    else if (result.status == 200) {
      result.size = contentLength_;
      result.resumable = acceptRanges_;
    }
    if (!complete_) {
      result.error = *errorText() ? errorText() : curl_easy_strerror(code);
    }
    auto done = std::move(done_);
    self.reset();
    done(std::move(result));
  }

  std::function<void(ProbeResult)> done_;
  ProbeResult result_;
  long status_ = 0;
  int64_t contentLength_ = -1;
  std::string contentRange_;
  bool acceptRanges_ = false;
  bool complete_ = false;
};

} // namespace

void probe(Client& client, const Options& options, const std::string& url,
           std::function<void(ProbeResult)> done)
{
  auto request = std::make_shared<Probe>(client, std::move(done));
  client.configure(*request, options, url, url);
  curl_easy_setopt(request->easy(), CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(request->easy(), CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(request->easy(), CURLOPT_RANGE, "0-0");
  curl_easy_setopt(request->easy(), CURLOPT_TIMEOUT, PROBE_TIMEOUT_SECONDS);
  curl_easy_setopt(
      request->easy(), CURLOPT_CONNECTTIMEOUT,
      static_cast<long>(std::min(options.integer("connect-timeout"),
                                 PROBE_CONNECT_TIMEOUT_SECONDS)));
  request->self = request;
  try {
    request->start();
  }
  catch (...) {
    request->self.reset();
    throw;
  }
}

} // namespace aria2::http
