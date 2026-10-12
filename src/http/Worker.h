/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "http/Client.h"

#include <string>

namespace aria2::http {

class HttpTask;

// One connection of an HttpTask: a ranged request and its response.
class Worker final : public Transfer {
public:
  Worker(HttpTask& task, int id, size_t slot, size_t source, int64_t begin,
         int64_t requestEnd);

  const int id;
  const size_t slot;
  const size_t source;
  const int64_t begin;
  const int64_t requestEnd;
  bool redirected = false; // requested through a cached redirect
  int family = 0;          // forced address family (4 or 6), 0: any

  // Final response of this request.
  long status = 0;
  bool accepted = false;
  bool done = false; // assignment complete; an abort is not a failure
  std::string etag;
  std::string lastModified;
  std::string date;
  std::string contentRange;
  std::string disposition;
  std::string contentType;
  std::string contentEncoding;
  std::string acceptRanges;
  std::string retryAfter;
  int64_t contentLength = -1;

  // Received bytes not yet handed to the disk pool.
  std::string buffer;
  int64_t bufferOffset = 0;

  Clock::time_point started = Clock::now();
  Clock::time_point lastData = Clock::now();
  bool connected = false; // the stall clock runs once the connection is up
  int64_t tickBytes = 0;
  int64_t received = 0;
  bool receiving = false;

private:
  void onHeader(std::string_view line) override;
  size_t onData(const char* data, size_t size) override;
  void onDone(CURLcode result) override;

  HttpTask& task_;
  bool block_ = false; // inside a header block
};

} // namespace aria2::http
