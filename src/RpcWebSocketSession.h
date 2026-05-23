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
#ifndef D_RPC_WEB_SOCKET_SESSION_H
#define D_RPC_WEB_SOCKET_SESSION_H

#include "common.h"

#include <deque>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/stream.hpp>

namespace aria2 {

class DownloadEngine;

namespace rpc {

class RpcWebSocketSession
    : public std::enable_shared_from_this<RpcWebSocketSession> {
public:
  RpcWebSocketSession(
      boost::asio::ip::tcp::socket socket, DownloadEngine* engine,
      boost::beast::http::request<boost::beast::http::string_body> request);
  ~RpcWebSocketSession();

  void start();
  void sendText(std::string message);
  bool isAuthorized() const { return authorized_; }
  void markAuthorized() { authorized_ = true; }

private:
  void read();
  void writeNext();
  void close();
  std::string processMessage(const std::string& message);

  boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request<boost::beast::http::string_body> request_;
  DownloadEngine* engine_;
  bool authorized_;
  bool writing_;
  std::deque<std::string> outbound_;
};

} // namespace rpc

} // namespace aria2

#endif // D_RPC_WEB_SOCKET_SESSION_H
