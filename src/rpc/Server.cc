/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "rpc/Server.h"

#include "base/Error.h"
#include "base/Log.h"
#include "base/Text.h"
#include "core/Engine.h"
#include "rpc/Dispatcher.h"
#include "rpc/XmlRpc.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <array>
#include <deque>
#include <optional>
#include <set>

namespace aria2::rpc {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;
using asio::awaitable;
using asio::use_awaitable;
using namespace std::chrono_literals;

namespace {

// One WebSocket client. Writes are queued so notifications never interleave
// with responses.
class Socket : public std::enable_shared_from_this<Socket> {
public:
  explicit Socket(tcp::socket socket) : ws(std::move(socket)) {}

  void send(std::string message)
  {
    queue_.push_back(std::move(message));
    if (queue_.size() == 1) {
      asio::co_spawn(ws.get_executor(), drain(shared_from_this()),
                     asio::detached);
    }
  }

  websocket::stream<tcp::socket> ws;

private:
  static awaitable<void> drain(std::shared_ptr<Socket> self)
  {
    while (!self->queue_.empty()) {
      self->ws.text(true);
      boost::system::error_code error;
      co_await self->ws.async_write(asio::buffer(self->queue_.front()),
                                    asio::redirect_error(use_awaitable, error));
      if (error) {
        self->queue_.clear();
        co_return;
      }
      self->queue_.pop_front();
    }
  }

  std::deque<std::string> queue_;
};

} // namespace

struct Server::State : std::enable_shared_from_this<State> {
  State(Engine& engine, Dispatcher& dispatcher)
      : engine(engine), dispatcher(dispatcher)
  {
  }

  Engine& engine;
  Dispatcher& dispatcher;
  std::vector<std::unique_ptr<tcp::acceptor>> acceptors;
  std::set<std::shared_ptr<Socket>> sockets;
  bool stopped = false;

  template <typename Body> void decorate(http::response<Body>& response)
  {
    response.set(http::field::server, "aria2-next");
    if (engine.global().flag("rpc-allow-origin-all")) {
      response.set(http::field::access_control_allow_origin, "*");
    }
  }

  // Resumes with the response document once the dispatcher answers.
  awaitable<std::string> dispatch(std::string body)
  {
    co_return co_await
        asio::async_initiate<decltype(use_awaitable), void(std::string)>(
            [this, &body](auto handler) {
              auto shared =
                  std::make_shared<decltype(handler)>(std::move(handler));
              dispatcher.handle(body, [shared](std::string reply) {
                asio::post(asio::get_associated_executor(*shared),
                           [shared, reply = std::move(reply)]() mutable {
                             std::move (*shared)(std::move(reply));
                           });
              });
            },
            use_awaitable);
  }

  // Answers an XML-RPC call through the JSON-RPC dispatcher; nullopt when
  // the body is not a methodCall.
  awaitable<std::optional<std::string>> dispatchXml(const std::string& body)
  {
    boost::json::object request;
    try {
      request = xml::parseCall(body);
    }
    catch (const std::exception& error) {
      log::info("RPC: rejected XML-RPC request: {}", error.what());
      co_return std::nullopt;
    }
    const bool multicall =
        request.at("method").as_string() == "system.multicall";
    co_return co_await
        asio::async_initiate<decltype(use_awaitable), void(std::string)>(
            [this, &request, multicall](auto handler) {
              auto shared =
                  std::make_shared<decltype(handler)>(std::move(handler));
              dispatcher.respond(
                  request, [shared, multicall](boost::json::value response) {
                    auto reply = xml::encodeResponse(response, multicall);
                    asio::post(asio::get_associated_executor(*shared),
                               [shared, reply = std::move(reply)]() mutable {
                                 std::move (*shared)(std::move(reply));
                               });
                  });
            },
            use_awaitable);
  }

  awaitable<http::response<http::string_body>>
  answer(const http::request<http::string_body>& request)
  {
    http::response<http::string_body> response{http::status::ok,
                                               request.version()};
    decorate(response);
    response.keep_alive(request.keep_alive());
    const auto target = std::string_view(request.target());
    const auto path = target.substr(0, target.find('?'));
    const bool xmlRpc = path == "/rpc";
    if (path != "/jsonrpc" && !xmlRpc) {
      response.result(http::status::not_found);
      response.prepare_payload();
      co_return response;
    }
    if (request.method() == http::verb::options) {
      response.set(http::field::access_control_allow_methods,
                   "POST, GET, OPTIONS");
      response.set(http::field::access_control_allow_headers,
                   request[http::field::access_control_request_headers]);
      response.set(http::field::access_control_max_age, "1728000");
      response.prepare_payload();
      co_return response;
    }
    if (xmlRpc) {
      co_return co_await answerXml(request, std::move(response));
    }
    std::string body;
    if (request.method() == http::verb::post) {
      body = co_await dispatch(request.body());
    }
    else if (request.method() == http::verb::get) {
      body = co_await dispatch(getRequest(target));
    }
    else {
      response.result(http::status::method_not_allowed);
      response.prepare_payload();
      co_return response;
    }
    response.set(http::field::content_type, "application/json-rpc");
    response.body() = std::move(body);
    response.prepare_payload();
    co_return response;
  }

  // XML-RPC is POST only, and a document that is not a call is a bad
  // request, as in aria2.
  awaitable<http::response<http::string_body>>
  answerXml(const http::request<http::string_body>& request,
            http::response<http::string_body> response)
  {
    if (request.method() != http::verb::post) {
      response.result(http::status::method_not_allowed);
      response.prepare_payload();
      co_return response;
    }
    auto body = co_await dispatchXml(request.body());
    if (!body) {
      response.result(http::status::bad_request);
      response.prepare_payload();
      co_return response;
    }
    response.set(http::field::content_type, "text/xml");
    response.body() = std::move(*body);
    response.prepare_payload();
    co_return response;
  }

  // GET /jsonrpc?method=M&id=I&params=BASE64(JSON array)
  static std::string getRequest(std::string_view target)
  {
    const auto query =
        target.substr(std::min(target.find('?') + 1, target.size()));
    boost::json::object request{{"jsonrpc", "2.0"}};
    for (const auto& pair : text::split(query, '&')) {
      const auto equals = pair.find('=');
      const auto name = pair.substr(0, equals);
      const auto value = equals == std::string::npos
                             ? std::string()
                             : text::percentDecode(pair.substr(equals + 1));
      if (name == "method") {
        request["method"] = value;
      }
      else if (name == "id") {
        request["id"] = value;
      }
      else if (name == "params") {
        boost::system::error_code error;
        auto params =
            boost::json::parse(text::base64Decode(value).value_or(""), error);
        if (!error) {
          request["params"] = std::move(params);
        }
      }
    }
    return boost::json::serialize(request);
  }

  awaitable<void> accept(tcp::acceptor& acceptor)
  {
    auto self = shared_from_this();
    while (!stopped) {
      boost::system::error_code error;
      auto socket = co_await acceptor.async_accept(
          asio::redirect_error(use_awaitable, error));
      if (error) {
        if (stopped || error == asio::error::operation_aborted) {
          co_return;
        }
        continue;
      }
      asio::co_spawn(acceptor.get_executor(), session(self, std::move(socket)),
                     asio::detached);
    }
  }

  static awaitable<void> session(std::shared_ptr<State> self,
                                 tcp::socket socket)
  {
    beast::tcp_stream stream(std::move(socket));
    beast::flat_buffer buffer;
    const auto limit = static_cast<uint64_t>(
        self->engine.global().integer("rpc-max-request-size"));
    for (;;) {
      http::request_parser<http::string_body> parser;
      parser.body_limit(limit);
      stream.expires_after(120s);
      boost::system::error_code error;
      co_await http::async_read(stream, buffer, parser,
                                asio::redirect_error(use_awaitable, error));
      if (error == http::error::body_limit) {
        co_await rejectTooLarge(stream);
      }
      if (error) {
        co_return;
      }
      auto request = parser.release();
      if (websocket::is_upgrade(request)) {
        stream.expires_never();
        co_await serveSocket(self, stream.release_socket(), std::move(request));
        co_return;
      }
      auto response = co_await self->answer(request);
      // The answer may take long (aria2.moveDownload); time the write alone.
      stream.expires_after(120s);
      co_await http::async_write(stream, response,
                                 asio::redirect_error(use_awaitable, error));
      if (error || !response.keep_alive()) {
        co_return;
      }
    }
  }

  // Answers 413 instead of dropping the connection, then reads what the
  // client is still sending so the answer is not lost to a reset.
  static awaitable<void> rejectTooLarge(beast::tcp_stream& stream)
  {
    http::response<http::string_body> response{http::status::payload_too_large,
                                               11};
    response.set(http::field::connection, "close");
    response.body() = "Request too large; see --rpc-max-request-size.";
    response.prepare_payload();
    boost::system::error_code error;
    co_await http::async_write(stream, response,
                               asio::redirect_error(use_awaitable, error));
    if (error) {
      co_return;
    }
    stream.socket().shutdown(tcp::socket::shutdown_send, error);
    stream.expires_after(5s);
    std::array<char, 64 << 10> discard;
    while (!error) {
      co_await stream.async_read_some(
          asio::buffer(discard), asio::redirect_error(use_awaitable, error));
    }
  }

  static awaitable<void> serveSocket(std::shared_ptr<State> self,
                                     tcp::socket socket,
                                     http::request<http::string_body> request)
  {
    auto client = std::make_shared<Socket>(std::move(socket));
    client->ws.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));
    client->ws.read_message_max(static_cast<uint64_t>(
        self->engine.global().integer("rpc-max-request-size")));
    boost::system::error_code error;
    co_await client->ws.async_accept(
        request, asio::redirect_error(use_awaitable, error));
    if (error) {
      co_return;
    }
    self->sockets.insert(client);
    beast::flat_buffer buffer;
    for (;;) {
      co_await client->ws.async_read(
          buffer, asio::redirect_error(use_awaitable, error));
      if (error) {
        break;
      }
      self->dispatcher.handle(
          beast::buffers_to_string(buffer.data()),
          [client](std::string reply) { client->send(std::move(reply)); });
      buffer.consume(buffer.size());
    }
    self->sockets.erase(client);
  }
};

Server::Server(Engine& engine, Dispatcher& dispatcher)
    : state_(std::make_shared<State>(engine, dispatcher))
{
}

Server::~Server() { stop(); }

void Server::start()
{
  auto& engine = state_->engine;
  const auto port =
      static_cast<unsigned short>(engine.global().integer("rpc-listen-port"));
  const bool all = engine.global().flag("rpc-listen-all");
  std::vector<tcp::endpoint> endpoints = {
      {all ? asio::ip::address(asio::ip::address_v4::any())
           : asio::ip::address(asio::ip::address_v4::loopback()),
       port},
      {all ? asio::ip::address(asio::ip::address_v6::any())
           : asio::ip::address(asio::ip::address_v6::loopback()),
       port}};
  std::string failure;
  for (const auto& endpoint : endpoints) {
    auto acceptor = std::make_unique<tcp::acceptor>(engine.io());
    boost::system::error_code error;
    acceptor->open(endpoint.protocol(), error);
    if (!error && endpoint.address().is_v6()) {
      acceptor->set_option(asio::ip::v6_only(true), error);
    }
#ifndef _WIN32
    if (!error) {
      acceptor->set_option(tcp::acceptor::reuse_address(true), error);
    }
#endif
    if (!error) {
      acceptor->bind(endpoint, error);
    }
    if (!error) {
      acceptor->listen(asio::socket_base::max_listen_connections, error);
    }
    if (error) {
      failure = fmt::format("{} ({})", error.message(),
                            endpoint.address().to_string());
      continue;
    }
    log::notice("RPC: listening on {} port {}", endpoint.address().to_string(),
                port);
    asio::co_spawn(engine.io(), state_->accept(*acceptor), asio::detached);
    state_->acceptors.push_back(std::move(acceptor));
  }
  if (state_->acceptors.empty()) {
    fail(ErrorCode::Unknown, "RPC: cannot listen on port {}: {}", port,
         failure);
  }
}

void Server::stop()
{
  state_->stopped = true;
  for (auto& acceptor : state_->acceptors) {
    boost::system::error_code ignored;
    acceptor->close(ignored);
  }
  for (const auto& socket : state_->sockets) {
    boost::system::error_code ignored;
    beast::get_lowest_layer(socket->ws).close(ignored);
  }
  state_->sockets.clear();
}

void Server::notify(const std::string& method, const std::string& gid)
{
  if (state_->sockets.empty()) {
    return;
  }
  const auto message = boost::json::serialize(boost::json::object{
      {"jsonrpc", "2.0"},
      {"method", method},
      {"params", boost::json::array{boost::json::object{{"gid", gid}}}}});
  for (const auto& socket : state_->sockets) {
    socket->send(message);
  }
}

} // namespace aria2::rpc
