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
  // Answers an asynchronous call once, on the control thread, with a result
  // or an error message.
  struct Reply {
    std::function<void(boost::json::value result)> result;
    std::function<void(std::string message)> error;
    void operator()(boost::json::value value) const
    {
      result(std::move(value));
    }
    void fail(std::string message) const { error(std::move(message)); }
  };
  // Answers later through `reply`, called once on the control thread.
  // Throwing before that fails the call.
  using AsyncMethod =
      std::function<void(boost::json::array& params, Reply reply)>;

  explicit Dispatcher(Engine& engine);

  void add(std::string name, Method method);
  // Asynchronous methods must be called alone, not in a batch or multicall.
  void addAsync(std::string name, AsyncMethod method);
  // Handles a request object or a batch; returns the response document.
  boost::json::value handle(const boost::json::value& request);
  // Parses and handles a request body, including JSON parse errors, and
  // passes the response document to `done`, at once or when an
  // asynchronous method answers.
  void handle(std::string_view body, std::function<void(std::string)> done);
  // Handles a parsed request object or batch like handle(body, done) and
  // passes the response document to `done` unserialized.
  void respond(const boost::json::value& request,
               std::function<void(boost::json::value)> done);
  std::vector<std::string> methods() const;
  static std::vector<std::string> notifications();

private:
  boost::json::value call(const boost::json::object& request);
  boost::json::value invoke(const std::string& name, boost::json::array params);
  // Checks and strips the token parameter.
  void authorize(const std::string& name, boost::json::array& params) const;

  Engine& engine_;
  std::vector<std::pair<std::string, Method>> methods_;
  std::vector<std::pair<std::string, AsyncMethod>> asyncMethods_;
};

// Registers every method of the aria2 interface and its extensions.
void registerMethods(Dispatcher& dispatcher, Engine& engine);
void registerBtMethods(Dispatcher& dispatcher, Engine& engine);
void registerMediaMethods(Dispatcher& dispatcher, Engine& engine);
void registerEd2kMethods(Dispatcher& dispatcher, Engine& engine);

} // namespace aria2::rpc
