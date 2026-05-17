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
#ifndef D_ED2K_ENDPOINT_H
#define D_ED2K_ENDPOINT_H

#include "common.h"

#include <cstdint>
#include <string>

namespace aria2 {

namespace ed2k {

struct Endpoint;

std::string ipv4FromEndpoint(uint32_t ip);
uint32_t ipv4ToEndpointValue(const std::string& host);
Endpoint readEndpoint(const std::string& data, size_t& offset);
std::string packEndpoint(const Endpoint& endpoint);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_ENDPOINT_H
