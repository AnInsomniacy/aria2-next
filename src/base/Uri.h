/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace aria2::uri {

// Components as libcurl's URL parser reports them; path and query stay
// percent-encoded.
struct Parts {
  std::string scheme; // lower case
  std::string user;
  std::string password;
  std::string host;
  std::string port; // effective port, including the scheme default
  std::string path;
  std::string query;
};

std::optional<Parts> parse(std::string_view url);
// Resolves a possibly relative reference against a base URL.
std::optional<std::string> resolve(std::string_view base,
                                   std::string_view reference);
bool sameOrigin(std::string_view a, std::string_view b);
// Removes user information and the query for logs and notifications.
std::string redact(std::string_view url);

// The decoded file name a URL suggests: a `filename=` query parameter, else the
// last path segment. Empty when nothing usable exists.
std::string fileName(std::string_view url);

// Thunder links (thunder://) carry a URI as Base64 of "AA" + URI + "ZZ".
bool isThunder(std::string_view link);
// The URI a Thunder link carries; nullopt when it carries none. Bytes outside
// printable ASCII come back percent-encoded, as sent, whatever charset made
// them.
std::optional<std::string> unwrapThunder(std::string_view link);

} // namespace aria2::uri
