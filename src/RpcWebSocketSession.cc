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
#include "RpcWebSocketSession.h"

#include <vector>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "A2STR.h"
#include "DownloadEngine.h"
#include "LogFactory.h"
#include "Option.h"
#include "RpcResponse.h"
#include "ValueBaseJsonParser.h"
#include "WebSocketSessionMan.h"
#include "json.h"
#include "prefs.h"
#include "rpc_helper.h"

namespace aria2 {

namespace rpc {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

namespace {
std::unique_ptr<ValueBase> parseJson(const std::string& body, ssize_t& error)
{
  return json::ValueBaseJsonParser().parseFinal(body.c_str(), body.size(),
                                                error);
}

bool responseAuthorized(const RpcResponse& res)
{
  return !not_authorized(res);
}

bool responseAuthorized(const std::vector<RpcResponse>& results)
{
  return !any_not_authorized(results.begin(), results.end());
}
} // namespace

RpcWebSocketSession::RpcWebSocketSession(tcp::socket socket,
                                         DownloadEngine* engine,
                                         http::request<http::string_body> request)
    : ws_(std::move(socket)),
      request_(std::move(request)),
      engine_(engine),
      authorized_(engine->validateToken(A2STR::NIL)),
      writing_(false)
{
}

RpcWebSocketSession::~RpcWebSocketSession()
{
}

void RpcWebSocketSession::start()
{
  auto self = shared_from_this();
  ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
  ws_.set_option(websocket::stream_base::decorator(
      [](websocket::response_type& res) {
        res.set(http::field::server, "aria2-next");
      }));
  ws_.async_accept(
      request_, [self](const boost::system::error_code& ec) {
        if (ec) {
          A2_LOG_INFO(fmt("WebSocket handshake failed: %s", ec.message().c_str()));
          return;
        }
        if (self->engine_->getWebSocketSessionMan()) {
          self->engine_->getWebSocketSessionMan()->addSession(self);
        }
        self->read();
        self->engine_->wakeRuntime();
      });
}

void RpcWebSocketSession::sendText(std::string message)
{
  auto writeIdle = outbound_.empty() && !writing_;
  outbound_.push_back(std::move(message));
  if (writeIdle) {
    writeNext();
  }
}

void RpcWebSocketSession::read()
{
  ws_.async_read(buffer_, [self = shared_from_this()](
                              const boost::system::error_code& ec,
                              std::size_t) {
    if (ec) {
      self->close();
      return;
    }

    const auto text = beast::buffers_to_string(self->buffer_.data());
    self->buffer_.consume(self->buffer_.size());
    self->sendText(self->processMessage(text));
    self->read();
    self->engine_->wakeRuntime();
  });
}

std::string RpcWebSocketSession::processMessage(const std::string& message)
{
  if (engine_->getOption()->getAsInt(PREF_RPC_MAX_REQUEST_SIZE) > 0 &&
      message.size() >
          static_cast<size_t>(engine_->getOption()->getAsInt(
              PREF_RPC_MAX_REQUEST_SIZE))) {
    RpcResponse res(createJsonRpcErrorResponse(-32600, "Invalid Request.",
                                               Null::g()));
    return toJson(res, A2STR::NIL, false);
  }

  ssize_t error = 0;
  auto json = parseJson(message, error);
  if (error < 0) {
    RpcResponse res(
        createJsonRpcErrorResponse(-32700, "Parse error.", Null::g()));
    return toJson(res, A2STR::NIL, false);
  }

  if (auto jsondict = downcast<Dict>(json)) {
    auto res = processJsonRpcRequest(jsondict, engine_);
    if (responseAuthorized(res)) {
      markAuthorized();
    }
    return toJson(res, A2STR::NIL, false);
  }

  if (auto jsonlist = downcast<List>(json)) {
    std::vector<RpcResponse> results;
    for (const auto& entry : *jsonlist) {
      if (auto jsondict = downcast<Dict>(entry)) {
        results.push_back(processJsonRpcRequest(jsondict, engine_));
      }
    }
    if (responseAuthorized(results)) {
      markAuthorized();
    }
    return toJsonBatch(results, A2STR::NIL, false);
  }

  RpcResponse res(
      createJsonRpcErrorResponse(-32600, "Invalid Request.", Null::g()));
  return toJson(res, A2STR::NIL, false);
}

void RpcWebSocketSession::writeNext()
{
  if (outbound_.empty() || writing_) {
    return;
  }

  writing_ = true;
  ws_.text(true);
  ws_.async_write(
      boost::asio::buffer(outbound_.front()),
      [self = shared_from_this()](const boost::system::error_code& ec,
                                  std::size_t) {
        self->writing_ = false;
        if (ec) {
          self->close();
          return;
        }
        self->outbound_.pop_front();
        self->writeNext();
        self->engine_->wakeRuntime();
      });
}

void RpcWebSocketSession::close()
{
  if (engine_->getWebSocketSessionMan()) {
    engine_->getWebSocketSessionMan()->removeSession(shared_from_this());
  }
  boost::system::error_code ec;
  ws_.next_layer().shutdown(tcp::socket::shutdown_both, ec);
  ws_.next_layer().close(ec);
}

} // namespace rpc

} // namespace aria2
