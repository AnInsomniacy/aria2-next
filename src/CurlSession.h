/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 AnInsomniacy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_CURL_SESSION_H
#define D_CURL_SESSION_H

#include "common.h"

#include <curl/curl.h>

#include <map>
#include <set>
#include <vector>

#include "a2netcompat.h"

namespace aria2 {

class Command;
class DownloadEngine;
class RequestGroup;

class CurlSession {
public:
  CurlSession(DownloadEngine* engine);
  ~CurlSession();

  CurlSession(const CurlSession&) = delete;
  CurlSession& operator=(const CurlSession&) = delete;

  CURLM* multi() { return multi_; }

  void perform();

  void add(CURL* easy, Command* command);

  void remove(CURL* easy);

  void refreshRateLimits();

  size_t activeHandleCount() const { return active_.size(); }

  size_t activeTaskCount() const;

  std::vector<RequestGroup*> activeTasks() const;

  int64_t testDownloadLimit(CURL* easy) const;
  int64_t testUploadLimit(CURL* easy) const;

  bool takeDoneResult(CURL* easy, CURLcode& result);

  int runningHandles() const { return runningHandles_; }

private:
  static int socketCallback(CURL* easy, curl_socket_t socket, int action,
                            void* userdata, void* socketData);

  static int timerCallback(CURLM* multi, long timeoutMs, void* userdata);

  bool addSocket(curl_socket_t socket, Command* command, int action);

  bool deleteSocket(curl_socket_t socket, Command* command, int action);

  void clearSocket(curl_socket_t socket);

  void drainMessages();

  DownloadEngine* engine_;
  CURLM* multi_;
  int runningHandles_;
  std::map<curl_socket_t, std::pair<Command*, int>> sockets_;
  std::map<CURL*, CURLcode> doneResults_;
  std::map<CURL*, Command*> active_;
  std::map<CURL*, std::pair<int64_t, int64_t>> appliedLimits_;
};

} // namespace aria2

#endif // D_CURL_SESSION_H
