/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <curl/curl.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace aria2 {
class Options;
}

namespace aria2::http {

// Applies the per-request settings derived from task options to `easy`:
// TLS, proxies, credentials, cookies, headers and timeouts. Header lines go
// to `header`. `loadedCookies` remembers which cookie store revision the
// share handle's jar already holds. `originalUrl` is the URL the user gave;
// credentials and custom auth headers stay with its origin.
void configureRequest(CURL* easy, CURLSH* share, std::string& loadedCookies,
                      const Options& options, const std::string& url,
                      const std::string& originalUrl,
                      const std::function<void(const std::string&)>& header);

struct ContentRange {
  int64_t first = -1;
  int64_t last = -1;
  int64_t total = -1; // -1: unknown ("*")
};

// "bytes 100-199/1000" or "bytes 100-199/*"; "bytes=" is accepted as well.
std::optional<ContentRange> parseContentRange(std::string_view value);
// Where the body of a 206 without Content-Range belongs, when its length
// leaves one answer: the tail of a request open to the end (`requestEnd` is
// `unbounded`) or exactly the bounded range [begin, requestEnd).
std::optional<ContentRange> impliedContentRange(int64_t begin,
                                                int64_t requestEnd,
                                                int64_t contentLength);
// The length a 416 answer states: "bytes */1000".
std::optional<int64_t> unsatisfiedRangeLength(std::string_view value);

} // namespace aria2::http
