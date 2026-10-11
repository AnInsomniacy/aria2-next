/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <map>
#include <string>
#include <vector>

namespace aria2::media {
bool isMediaHttpUrl(const std::string& value);

struct RequestContext {
  std::string url;
  std::map<std::string, std::string> headers;
};

// Contexts are immutable task input. Every HTTP hop selects its own origin.
std::vector<RequestContext> parseRequestContexts(const std::string& payload);

} // namespace aria2::media
