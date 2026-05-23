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
#include "RpcHttpHandler.h"

#include <algorithm>

#include "A2STR.h"
#include "BoostJsonValue.h"
#include "DownloadEngine.h"
#include "Option.h"
#include "RpcResponse.h"
#include "json.h"
#include "prefs.h"
#include "rpc_helper.h"

namespace aria2 {

namespace {
std::string getPath(const std::string& target)
{
  auto end = target.find_first_of("?#");
  auto path = target.substr(0, end);
  return path.empty() ? "/" : path;
}

std::string getQuery(const std::string& target)
{
  auto question = target.find('?');
  if (question == std::string::npos) {
    return {};
  }
  auto fragment = target.find('#', question);
  return target.substr(question, fragment - question);
}

const std::string* findHeader(const RpcHttpRequest& req,
                              const std::string& name)
{
  auto itr = req.headers.find(name);
  return itr == req.headers.end() ? nullptr : &itr->second;
}

std::string contentType(bool script)
{
  return script ? "text/javascript" : "application/json-rpc";
}

int statusForJsonRpc(const rpc::RpcResponse& res)
{
  switch (res.code) {
  case 0:
    return 200;
  case 1:
  case -32600:
    return 400;
  case -32601:
    return 404;
  default:
    return 500;
  }
}

RpcHttpResponse jsonResponse(const rpc::RpcResponse& res,
                             const std::string& callback, bool gzip)
{
  RpcHttpResponse http;
  http.status = statusForJsonRpc(res);
  http.contentType = contentType(!callback.empty());
  http.gzip = gzip;
  http.closeConnection = res.code != 0;
  http.delayAfterWrite = rpc::not_authorized(res);
  http.body = rpc::toJson(res, callback, gzip);
  return http;
}

RpcHttpResponse jsonBatchResponse(const std::vector<rpc::RpcResponse>& results,
                                  const std::string& callback, bool gzip)
{
  RpcHttpResponse http;
  http.status = 200;
  http.contentType = contentType(!callback.empty());
  http.gzip = gzip;
  http.delayAfterWrite =
      rpc::any_not_authorized(results.begin(), results.end());
  http.body = rpc::toJsonBatch(results, callback, gzip);
  return http;
}

std::unique_ptr<ValueBase> parseJson(const std::string& body, bool& ok)
{
  return json::parseValue(body, ok);
}

RpcHttpResponse processJson(std::unique_ptr<ValueBase> json,
                            const std::string& callback, bool gzip,
                            DownloadEngine* engine)
{
  if (auto jsondict = downcast<Dict>(json)) {
    return jsonResponse(rpc::processJsonRpcRequest(jsondict, engine), callback,
                        gzip);
  }

  if (auto jsonlist = downcast<List>(json)) {
    std::vector<rpc::RpcResponse> results;
    for (const auto& entry : *jsonlist) {
      if (auto jsondict = downcast<Dict>(entry)) {
        results.push_back(rpc::processJsonRpcRequest(jsondict, engine));
      }
    }
    return jsonBatchResponse(results, callback, gzip);
  }

  return jsonResponse(rpc::createJsonRpcErrorResponse(
                          -32600, "Invalid Request.", Null::g()),
                      callback, gzip);
}
} // namespace

RpcHttpHandler::RpcHttpHandler(DownloadEngine* engine) : engine_(engine) {}

RpcHttpResponse RpcHttpHandler::handle(const RpcHttpRequest& req) const
{
  RpcHttpResponse res;
  auto option = engine_->getOption();
  if (option->getAsBool(PREF_RPC_ALLOW_ORIGIN_ALL)) {
    res.headers.emplace("access-control-allow-origin", "*");
  }

  if (req.method == "OPTIONS") {
    if (findHeader(req, "origin") &&
        findHeader(req, "access-control-request-method") &&
        option->getAsBool(PREF_RPC_ALLOW_ORIGIN_ALL)) {
      res.headers.emplace("access-control-allow-methods",
                          "POST, GET, OPTIONS");
      res.headers.emplace("access-control-max-age", "1728000");
      if (auto requestHeaders =
              findHeader(req, "access-control-request-headers")) {
        res.headers.emplace("access-control-allow-headers", *requestHeaders);
      }
    }
    return res;
  }

  const auto path = getPath(req.target);
  const auto gzip = req.acceptsGzip;
  if (req.method == "POST" && path == "/jsonrpc") {
    if (option->getAsInt(PREF_RPC_MAX_REQUEST_SIZE) > 0 &&
        req.body.size() >
            static_cast<size_t>(option->getAsInt(PREF_RPC_MAX_REQUEST_SIZE))) {
      res.status = 413;
      res.closeConnection = true;
      return res;
    }

    bool ok = false;
    auto json = parseJson(req.body, ok);
    if (!ok) {
      return jsonResponse(rpc::createJsonRpcErrorResponse(
                              -32700, "Parse error.", Null::g()),
                          A2STR::NIL, gzip);
    }
    return processJson(std::move(json), A2STR::NIL, gzip, engine_);
  }

  if (req.method == "GET" && path == "/jsonrpc") {
    auto param = json::decodeGetParams(getQuery(req.target));
    bool ok = false;
    auto json = parseJson(param.request, ok);
    if (!ok) {
      return jsonResponse(rpc::createJsonRpcErrorResponse(
                              -32700, "Parse error.", Null::g()),
                          param.callback, gzip);
    }
    return processJson(std::move(json), param.callback, gzip, engine_);
  }

  res.status = 404;
  res.closeConnection = true;
  return res;
}

} // namespace aria2
