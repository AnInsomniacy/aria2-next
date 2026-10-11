/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>

namespace aria2::ed2k {

struct Endpoint;

std::string ipv4FromEndpoint(uint32_t ip);
uint32_t ipv4ToEndpointValue(const std::string& host);
Endpoint readEndpoint(const std::string& data, size_t& offset);
std::string packEndpoint(const Endpoint& endpoint);

} // namespace aria2::ed2k
