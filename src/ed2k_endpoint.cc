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
#include "ed2k_endpoint.h"

#include <sstream>

#include "DlAbortEx.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"
#include "fmt.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

std::string ipv4FromEndpoint(uint32_t ip)
{
  return fmt("%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff,
             (ip >> 24) & 0xff);
}

uint32_t ipv4ToEndpointValue(const std::string& host)
{
  uint32_t value = 0;
  std::stringstream ss(host);
  std::string part;
  for (size_t i = 0; i < 4; ++i) {
    if (!std::getline(ss, part, '.')) {
      throw DL_ABORT_EX(fmt("Bad ED2K IPv4 address: %s", host.c_str()));
    }
    uint32_t octet = 0;
    if (!util::parseUIntNoThrow(octet, part) || octet > 255) {
      throw DL_ABORT_EX(fmt("Bad ED2K IPv4 address: %s", host.c_str()));
    }
    value |= octet << (i * 8);
  }
  if (std::getline(ss, part, '.')) {
    throw DL_ABORT_EX(fmt("Bad ED2K IPv4 address: %s", host.c_str()));
  }
  return value;
}

Endpoint readEndpoint(const std::string& data, size_t& offset)
{
  auto ip = readUInt32(readBytes(data, offset, 4).data());
  auto port = readUInt16(readBytes(data, offset, 2).data());
  Endpoint endpoint;
  endpoint.host = ipv4FromEndpoint(ip);
  endpoint.port = port;
  return endpoint;
}

std::string packEndpoint(const Endpoint& endpoint)
{
  return packUInt32(ipv4ToEndpointValue(endpoint.host)) +
         packUInt16(endpoint.port);
}

} // namespace ed2k

} // namespace aria2
