/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef ARIA2_TRANSPORT_CURL_OPTIONS_H
#define ARIA2_TRANSPORT_CURL_OPTIONS_H

#include "common.h"

#include <curl/curl.h>
#include <string>

namespace aria2 {
class Option;
namespace http {

// Uses libcurl's canonical scheme, host and effective port. Invalid URLs never
// grant access to credentials belonging to another request.
bool sameOrigin(const std::string& first, const std::string& second);
// Expand host globs only for this request; curl retains native domain/IP/CIDR
// matching.
std::string noProxyFor(const std::string& rules, const std::string& url);

// Applies the existing target-specific trust policy to an unshared easy handle.
// Returns the first native option failure without changing caller error policy.
CURLcode configureTls(CURL* handle, const Option* option);

} // namespace http
} // namespace aria2
#endif
