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
#include "RpcBeastServer.h"

#include <algorithm>
#include <memory>
#include <string>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>

#include "AsioRuntime.h"
#include "DownloadEngine.h"
#include "LogFactory.h"
#include "Option.h"
#include "RpcHttpHandler.h"
#include "RpcWebSocketSession.h"
#include "TimeA2.h"
#include "fmt.h"
#include "message.h"
#include "prefs.h"
#include "util.h"

namespace aria2 {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

namespace {
constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(30);

std::string lowerHeaderName(beast::string_view name)
{
  std::string out(name.data(), name.size());
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

bool acceptsGzip(const http::request<http::string_body>& req)
{
  auto enc = req[http::field::accept_encoding];
  const std::string gzip = "gzip";
  return util::strifind(enc.begin(), enc.end(), gzip.begin(), gzip.end()) !=
         enc.end();
}

std::string toString(beast::string_view value)
{
  return {value.data(), value.size()};
}

template <typename Body, typename Fields>
void setCommonHeaders(http::response<Body, Fields>& res,
                      const RpcHttpResponse& rpc)
{
  const auto httpDate = Time().toHTTPDate();
  res.version(11);
  res.result(static_cast<http::status>(rpc.status));
  res.set(http::field::date, httpDate);
  res.set(http::field::expires, httpDate);
  res.set(http::field::cache_control, "no-cache");
  if (!rpc.contentType.empty()) {
    res.set(http::field::content_type, rpc.contentType);
  }
  for (const auto& header : rpc.headers) {
    res.set(header.first, header.second);
  }
  if (rpc.gzip) {
    res.set(http::field::content_encoding, "gzip");
  }
  if (rpc.closeConnection) {
    res.keep_alive(false);
  }
}

class RpcBeastSession : public std::enable_shared_from_this<RpcBeastSession> {
public:
  RpcBeastSession(tcp::socket socket, DownloadEngine* engine)
      : socket_(std::move(socket)),
        timer_(socket_.get_executor()),
        engine_(engine),
        handler_(engine)
  {
  }

  void start() { read(); }

private:
  void read()
  {
    req_ = {};
    timer_.expires_after(REQUEST_TIMEOUT);
    timer_.async_wait([self = shared_from_this()](
                          const boost::system::error_code& ec) {
      if (!ec) {
        beast::error_code closeEc;
        self->socket_.close(closeEc);
      }
    });
    http::async_read(socket_, buffer_, req_,
                     [self = shared_from_this()](
                         const boost::system::error_code& ec, std::size_t) {
      self->timer_.cancel();
      if (!ec) {
        if (websocket::is_upgrade(self->req_) &&
            self->req_.target() == "/jsonrpc") {
          std::make_shared<rpc::RpcWebSocketSession>(
              std::move(self->socket_), self->engine_, std::move(self->req_))
              ->start();
          self->engine_->wakeRuntime();
          return;
        }
        self->write();
      }
    });
  }

  RpcHttpRequest createRequest() const
  {
    RpcHttpRequest out;
    out.method = toString(req_.method_string());
    out.target = toString(req_.target());
    out.body = req_.body();
    out.acceptsGzip = acceptsGzip(req_);
    for (const auto& header : req_) {
      out.headers.emplace(lowerHeaderName(header.name_string()),
                          toString(header.value()));
    }
    return out;
  }

  void write()
  {
    auto rpc = handler_.handle(createRequest());
    auto res = std::make_shared<http::response<http::string_body>>();
    setCommonHeaders(*res, rpc);
    res->keep_alive(req_.keep_alive() && !rpc.closeConnection);
    res->body() = std::move(rpc.body);
    res->prepare_payload();

    http::async_write(socket_, *res,
                      [self = shared_from_this(), res, close = rpc.closeConnection,
                       delay = rpc.delayAfterWrite](
                          const boost::system::error_code& ec, std::size_t) {
      if (ec || close) {
        beast::error_code closeEc;
        self->socket_.shutdown(tcp::socket::shutdown_send, closeEc);
        return;
      }
      if (delay) {
        self->timer_.expires_after(std::chrono::seconds(1));
        self->timer_.async_wait([self](const boost::system::error_code& ec) {
          if (!ec) {
            self->read();
          }
        });
        return;
      }
      self->read();
    });
    engine_->wakeRuntime();
  }

  tcp::socket socket_;
  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  asio::steady_timer timer_;
  DownloadEngine* engine_;
  RpcHttpHandler handler_;
};
} // namespace

RpcBeastServer::RpcBeastServer(DownloadEngine* engine, int family)
    : engine_(engine),
      family_(family),
      acceptor_(engine->getRuntime().ioContext())
{
}

RpcBeastServer::~RpcBeastServer() { stop(); }

bool RpcBeastServer::bindPort(uint16_t port)
{
  boost::system::error_code ec;
  const bool listenAll = engine_->getOption()->getAsBool(PREF_RPC_LISTEN_ALL);
  auto address =
      family_ == AF_INET6
          ? asio::ip::address(listenAll ? asio::ip::address_v6::any()
                                        : asio::ip::make_address_v6("::1"))
          : asio::ip::address(listenAll ? asio::ip::address_v4::any()
                                        : asio::ip::make_address_v4("127.0.0.1"));
  tcp::endpoint endpoint(address, port);
  const auto ipv = family_ == AF_INET6 ? 6 : 4;

  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    A2_LOG_ERROR(fmt("IPv%d RPC: failed to open TCP port %u: %s", ipv, port,
                     ec.message().c_str()));
    return false;
  }

  acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
  if (ec) {
    A2_LOG_ERROR(fmt("IPv%d RPC: failed to prepare TCP port %u: %s", ipv, port,
                     ec.message().c_str()));
    return false;
  }

  acceptor_.bind(endpoint, ec);
  if (ec) {
    A2_LOG_ERROR(fmt("IPv%d RPC: failed to bind TCP port %u: %s", ipv, port,
                     ec.message().c_str()));
    return false;
  }

  acceptor_.listen(asio::socket_base::max_listen_connections, ec);
  if (ec) {
    A2_LOG_ERROR(fmt("IPv%d RPC: failed to listen on TCP port %u: %s", ipv,
                     port, ec.message().c_str()));
    return false;
  }

  A2_LOG_INFO(fmt(MSG_LISTENING_PORT, engine_->newCUID(), port));
  A2_LOG_NOTICE(fmt(_("IPv%d RPC: listening on TCP port %u"), ipv, port));
  accept();
  return true;
}

void RpcBeastServer::stop()
{
  boost::system::error_code ec;
  acceptor_.close(ec);
}

void RpcBeastServer::accept()
{
  acceptor_.async_accept(
      [self = shared_from_this()](const boost::system::error_code& ec,
                                  tcp::socket socket) {
    if (!ec) {
      socket.set_option(tcp::no_delay(true));
      auto endpoint = socket.remote_endpoint();
      A2_LOG_INFO(fmt("RPC: Accepted the connection from %s:%u.",
                      endpoint.address().to_string().c_str(),
                      endpoint.port()));
      std::make_shared<RpcBeastSession>(std::move(socket), self->engine_)
          ->start();
    }
    if (self->acceptor_.is_open()) {
      self->accept();
    }
    self->engine_->wakeRuntime();
  });
}

} // namespace aria2
