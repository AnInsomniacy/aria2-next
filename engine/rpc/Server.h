/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <boost/json/value.hpp>

#include <memory>
#include <string>
#include <vector>

namespace aria2 {
class Engine;
}

namespace aria2::rpc {

class Dispatcher;

// aria2 JSON-RPC over HTTP POST (/jsonrpc) and WebSocket (/jsonrpc), with
// notifications pushed to every WebSocket client.
class Server {
public:
  Server(Engine& engine, Dispatcher& dispatcher);
  ~Server();
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  // Binds the configured port on loopback or all interfaces. Throws on
  // failure so the process can report a port conflict.
  void start();
  void stop();
  void notify(const std::string& method, const std::string& gid);

private:
  struct State;
  std::shared_ptr<State> state_;
};

} // namespace aria2::rpc
