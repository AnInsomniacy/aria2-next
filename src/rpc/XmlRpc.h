/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include <string>
#include <string_view>

// aria2's XML-RPC interface (POST /rpc), as used by Sonarr, Radarr and other
// clients that never moved to JSON-RPC. Calls are translated to JSON-RPC
// requests and answered by the same dispatcher.
namespace aria2::rpc::xml {

// The JSON-RPC request for an XML-RPC methodCall document. Values without a
// type are strings, as the specification says; base64 values stay encoded
// because the methods taking binary data decode base64 themselves. Throws
// Error when the document is not a well-formed methodCall.
boost::json::object parseCall(std::string_view document);

// The methodResponse document for a JSON-RPC response object: its result as
// the single parameter, or its error as a fault. In a system.multicall result
// the failed calls become faultCode/faultString structs, as aria2 sent them.
std::string encodeResponse(const boost::json::value& response, bool multicall);

} // namespace aria2::rpc::xml
