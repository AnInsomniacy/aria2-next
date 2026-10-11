/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Text.h"
#include "ed2k/Endpoint.h"
#include <spdlog/fmt/fmt.h>
#include <cstddef>
#include <cstdint>
#include <string>

#include <sstream>

#include "ed2k/Link.h"
#include "ed2k/Packet.h"

namespace aria2::ed2k {

std::string ipv4FromEndpoint(uint32_t ip)
{
  return fmt::format("{}.{}.{}.{}", ip & 0xff, (ip >> 8) & 0xff,
                     (ip >> 16) & 0xff, (ip >> 24) & 0xff);
}

uint32_t ipv4ToEndpointValue(const std::string& host)
{
  uint32_t value = 0;
  std::stringstream ss(host);
  std::string part;
  for (size_t i = 0; i < 4; ++i) {
    if (!std::getline(ss, part, '.')) {
      throw ProtocolError(fmt::format("Bad ED2K IPv4 address: {}", host));
    }
    const auto octet = text::toInt(part);
    if (!octet || *octet < 0 || *octet > 255) {
      throw ProtocolError(fmt::format("Bad ED2K IPv4 address: {}", host));
    }
    value |= static_cast<uint32_t>(*octet) << (i * 8);
  }
  if (std::getline(ss, part, '.')) {
    throw ProtocolError(fmt::format("Bad ED2K IPv4 address: {}", host));
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

} // namespace aria2::ed2k
