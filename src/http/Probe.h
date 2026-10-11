/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace aria2 {
class Options;
}

namespace aria2::http {

class Client;

// What a download of the URL would receive, learned from the first byte.
struct ProbeResult {
  long status = 0;      // final HTTP status; 0 when no response arrived
  std::string finalUrl; // after redirects
  std::string contentType;
  std::string filename; // from Content-Disposition; empty when absent
  int64_t size = -1;    // -1: unknown
  bool resumable = false;
  std::string error; // transport or HTTP failure; empty on success
};

// Requests the first byte of an HTTP(S) URL with the request settings of
// `options` (proxy, TLS, cookies, headers) and reports the response. The
// body is discarded. `done` runs on the control thread exactly once, unless
// the engine shuts down first.
void probe(Client& client, const Options& options, const std::string& url,
           std::function<void(ProbeResult)> done);

} // namespace aria2::http
