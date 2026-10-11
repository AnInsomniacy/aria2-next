/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace aria2 {
class Engine;
}

namespace aria2::rpc {

// Maps JSON-RPC method names to handlers and applies the token secret.
class Dispatcher {
public:
  using Method = std::function<boost::json::value(boost::json::array& params)>;

  explicit Dispatcher(Engine& engine);

  void add(std::string name, Method method);
  // Handles a request object or a batch; returns the response document.
  boost::json::value handle(const boost::json::value& request);
  // Parses and handles a request body, including JSON parse errors.
  std::string handle(std::string_view body);
  std::vector<std::string> methods() const;
  static std::vector<std::string> notifications();

private:
  boost::json::value call(const boost::json::object& request);
  boost::json::value invoke(const std::string& name, boost::json::array params);

  Engine& engine_;
  std::vector<std::pair<std::string, Method>> methods_;
};

// Registers every method of the aria2 interface and its extensions.
void registerMethods(Dispatcher& dispatcher, Engine& engine);
void registerBtMethods(Dispatcher& dispatcher, Engine& engine);

} // namespace aria2::rpc
