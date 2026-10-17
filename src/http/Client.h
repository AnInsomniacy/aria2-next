/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "http/HostPool.h"

#include <curl/curl.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace aria2 {
class Engine;
class Options;
} // namespace aria2

namespace aria2::http {

class Client;

// One libcurl request. Subclasses receive headers, body bytes and the final
// result on the control thread. Transfers are shared objects: cancel() may be
// called from inside libcurl callbacks, so removal is deferred and keeps the
// transfer alive until libcurl has let go of it.
class Transfer : public std::enable_shared_from_this<Transfer> {
public:
  explicit Transfer(Client& client);
  virtual ~Transfer();
  Transfer(const Transfer&) = delete;
  Transfer& operator=(const Transfer&) = delete;

  CURL* easy() const { return easy_; }
  // Adds the request to the multi handle. Options must be set first.
  void start();
  // Stops the request without calling onDone(). Safe inside callbacks;
  // pending data is discarded.
  void cancel();
  bool cancelled() const { return cancelled_; }
  void pause(bool paused);
  bool running() const { return running_; }
  bool paused() const { return paused_; }

  // Appends a request header; the list is applied by start().
  void header(const std::string& line);
  long responseCode() const;
  std::string effectiveUrl() const;
  // Address family of the last connection: 4, 6, or 0 before connecting.
  int connectedFamily() const;
  const char* errorText() const { return errorBuffer_.data(); }

protected:
  // Called once per header line, including status lines of redirects.
  virtual void onHeader(std::string_view) {}
  // Returns the bytes consumed. Returning fewer aborts the request
  // (CURLE_WRITE_ERROR); CURL_WRITEFUNC_PAUSE pauses it.
  virtual size_t onData(const char* data, size_t size) = 0;
  virtual void onDone(CURLcode result) = 0;

  Client& client_;

private:
  friend class Client;
  static size_t headerCallback(char* data, size_t, size_t size, void* self);
  static size_t writeCallback(char* data, size_t, size_t size, void* self);

  CURL* easy_;
  curl_slist* headers_ = nullptr;
  std::array<char, CURL_ERROR_SIZE> errorBuffer_{};
  bool running_ = false;
  bool paused_ = false;
  bool cancelled_ = false;
};

// libcurl's multi interface driven by Asio socket readiness and timers.
class Client {
public:
  explicit Client(Engine& engine);
  ~Client();
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  Engine& engine() { return engine_; }
  CURLSH* share() const { return share_; }
  void optionsChanged();
  // max-overall-download-limit, shared by all HTTP transfers.
  RateLimit& downloadLimit() { return downloadLimit_; }
  // Connections per server, shared by the downloads from it.
  HostPool& hosts() { return hosts_; }
  void close();

  void add(Transfer& transfer);
  void remove(Transfer& transfer);

  // Applies the per-request settings derived from task options: TLS,
  // proxies, credentials, cookies, headers and timeouts.
  void configure(Transfer& transfer, const Options& options,
                 const std::string& url, const std::string& originalUrl);

private:
  struct Socket;

  static int socketCallback(CURL*, curl_socket_t fd, int what, void* self,
                            void*);
  static int timerCallback(CURLM*, long timeoutMs, void* self);
  static curl_socket_t openCallback(void* self, curlsocktype,
                                    curl_sockaddr* address);
  static int closeCallback(void* self, curl_socket_t fd);

  void watch(const std::shared_ptr<Socket>& socket);
  void action(curl_socket_t fd, int events);
  void finishTransfers();

  Engine& engine_;
  boost::asio::io_context& io_;
  CURLM* multi_;
  CURLSH* share_;
  boost::asio::steady_timer timer_;
  std::map<curl_socket_t, std::shared_ptr<Socket>> sockets_;
  std::string loadedCookies_;
  RateLimit downloadLimit_;
  HostPool hosts_;
};

} // namespace aria2::http
