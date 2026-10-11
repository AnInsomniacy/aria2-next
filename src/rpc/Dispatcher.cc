/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "rpc/Dispatcher.h"

#include "base/Error.h"
#include "base/Log.h"
#include "core/Engine.h"

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <algorithm>

namespace aria2::rpc {

namespace json = boost::json;

namespace {

json::object reply(const json::value& id, json::value result)
{
  return json::object{{"id", id}, {"jsonrpc", "2.0"}, {"result", std::move(result)}};
}

json::object failure(const json::value& id, int code, std::string_view message)
{
  return json::object{{"id", id},
                      {"jsonrpc", "2.0"},
                      {"error", json::object{{"code", code}, {"message", message}}}};
}

// Constant-time comparison so the secret cannot be guessed by timing.
bool sameSecret(std::string_view given, std::string_view secret)
{
  unsigned char difference = given.size() == secret.size() ? 0 : 1;
  for (size_t i = 0; i < given.size(); ++i) {
    difference |= static_cast<unsigned char>(given[i] ^ secret[i % std::max<size_t>(secret.size(), 1)]);
  }
  return difference == 0;
}

} // namespace

Dispatcher::Dispatcher(Engine& engine) : engine_(engine) {}

void Dispatcher::add(std::string name, Method method)
{
  methods_.emplace_back(std::move(name), std::move(method));
}

std::vector<std::string> Dispatcher::methods() const
{
  std::vector<std::string> names;
  for (const auto& [name, method] : methods_) {
    names.push_back(name);
  }
  return names;
}

std::vector<std::string> Dispatcher::notifications()
{
  return {"aria2.onDownloadStart",    "aria2.onDownloadPause",
          "aria2.onDownloadStop",     "aria2.onDownloadComplete",
          "aria2.onDownloadError",    "aria2.onBtDownloadComplete"};
}

json::value Dispatcher::invoke(const std::string& name, json::array params)
{
  const bool open = name == "system.listMethods" || name == "system.listNotifications";
  const auto secret = engine_.global().get("rpc-secret");
  std::string token;
  if (!params.empty() && params.front().is_string() &&
      params.front().as_string().starts_with("token:")) {
    token = std::string(params.front().as_string()).substr(6);
    params.erase(params.begin());
  }
  if (!open && name != "system.multicall" && !secret.empty() &&
      !sameSecret(token, secret)) {
    throw Error(ErrorCode::Unknown, "Unauthorized");
  }
  if (name == "system.multicall") {
    if (!secret.empty() && !token.empty() && !sameSecret(token, secret)) {
      throw Error(ErrorCode::Unknown, "Unauthorized");
    }
    if (params.empty() || !params[0].is_array()) {
      throw Error(ErrorCode::Unknown, "system.multicall expects an array");
    }
    json::array results;
    for (const auto& item : params[0].as_array()) {
      const auto* call = item.if_object();
      const auto* method = call ? call->if_contains("methodName") : nullptr;
      if (!method || !method->is_string() || *method == "system.multicall") {
        results.push_back(json::object{{"code", 1}, {"message", "Invalid call"}});
        continue;
      }
      json::array nested;
      if (const auto* list = call->if_contains("params"); list && list->is_array()) {
        nested = list->as_array();
      }
      try {
        results.push_back(json::array{invoke(std::string(method->as_string()),
                                             std::move(nested))});
      }
      catch (const std::exception& error) {
        results.push_back(json::object{{"code", 1}, {"message", error.what()}});
      }
    }
    return results;
  }
  const auto found = std::find_if(methods_.begin(), methods_.end(),
                                  [&name](const auto& entry) { return entry.first == name; });
  if (found == methods_.end()) {
    throw Error(ErrorCode::Unknown, "No such method: " + name);
  }
  return found->second(params);
}

json::value Dispatcher::call(const json::object& request)
{
  const auto* id = request.if_contains("id");
  const json::value requestId = id ? *id : json::value(nullptr);
  const auto* method = request.if_contains("method");
  if (!method || !method->is_string()) {
    return failure(requestId, -32600, "Invalid Request.");
  }
  json::array params;
  if (const auto* list = request.if_contains("params")) {
    if (!list->is_array()) {
      return failure(requestId, -32602, "Invalid params.");
    }
    params = list->as_array();
  }
  const std::string name(method->as_string());
  try {
    return reply(requestId, invoke(name, std::move(params)));
  }
  catch (const std::exception& error) {
    log::info("RPC {} failed: {}", name, error.what());
    return failure(requestId, 1, error.what());
  }
}

json::value Dispatcher::handle(const json::value& request)
{
  if (const auto* batch = request.if_array()) {
    json::array responses;
    for (const auto& item : *batch) {
      responses.push_back(item.is_object() ? call(item.as_object())
                                           : json::value(failure(nullptr, -32600, "Invalid Request.")));
    }
    return responses;
  }
  if (const auto* object = request.if_object()) {
    return call(*object);
  }
  return failure(nullptr, -32600, "Invalid Request.");
}

std::string Dispatcher::handle(std::string_view body)
{
  boost::system::error_code error;
  const auto request = json::parse(body, error);
  if (error) {
    return json::serialize(failure(nullptr, -32700, "Parse error."));
  }
  return json::serialize(handle(request));
}

} // namespace aria2::rpc
