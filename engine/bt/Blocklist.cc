/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "bt/Blocklist.h"

#include "base/Text.h"

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <optional>

namespace aria2::bt {

namespace ip = boost::asio::ip;

namespace {

// Blocklists write IPv4 octets with leading zeros ("001.002.003.004"), which
// inet_pton rejects.
std::optional<ip::address> parseAddress(std::string_view value)
{
  value = text::trim(value);
  std::string normalized;
  if (value.find(':') == std::string_view::npos) {
    for (const auto& octet : text::split(value, '.')) {
      const auto number = text::toInt(octet);
      if (!number || *number > 255) {
        return std::nullopt;
      }
      normalized += (normalized.empty() ? "" : ".") + std::to_string(*number);
    }
  }
  else {
    normalized = value;
  }
  boost::system::error_code error;
  const auto address = ip::make_address(normalized, error);
  if (error) {
    return std::nullopt;
  }
  if (address.is_v6() && address.to_v6().is_v4_mapped()) {
    return ip::make_address_v4(ip::v4_mapped, address.to_v6());
  }
  return address;
}

template <typename Bytes> Bytes withHostBits(Bytes bytes, int prefix, bool set)
{
  for (size_t i = 0; i < bytes.size(); ++i) {
    const int bits = std::clamp(prefix - static_cast<int>(i) * 8, 0, 8);
    const auto mask = static_cast<unsigned char>(0xff << (8 - bits));
    bytes[i] = set ? static_cast<unsigned char>(bytes[i] | ~mask)
                   : static_cast<unsigned char>(bytes[i] & mask);
  }
  return bytes;
}

std::optional<std::pair<ip::address, ip::address>> cidr(std::string_view value)
{
  const auto slash = value.find('/');
  const auto address = parseAddress(value.substr(0, slash));
  if (!address) {
    return std::nullopt;
  }
  const int width = address->is_v4() ? 32 : 128;
  int prefix = width;
  if (slash != std::string_view::npos) {
    const auto parsed = text::toInt(text::trim(value.substr(slash + 1)));
    if (!parsed || *parsed > width) {
      return std::nullopt;
    }
    prefix = static_cast<int>(*parsed);
  }
  if (address->is_v4()) {
    const auto bytes = address->to_v4().to_bytes();
    return std::pair{ip::address(ip::address_v4(withHostBits(bytes, prefix, false))),
                     ip::address(ip::address_v4(withHostBits(bytes, prefix, true)))};
  }
  const auto bytes = address->to_v6().to_bytes();
  return std::pair{ip::address(ip::address_v6(withHostBits(bytes, prefix, false))),
                   ip::address(ip::address_v6(withHostBits(bytes, prefix, true)))};
}

std::optional<std::pair<ip::address, ip::address>> span(std::string_view value)
{
  const auto dash = value.find('-');
  const auto first = parseAddress(value.substr(0, dash));
  const auto last = parseAddress(value.substr(dash + 1));
  if (!first || !last || first->is_v4() != last->is_v4() || *last < *first) {
    return std::nullopt;
  }
  return std::pair{*first, *last};
}

std::optional<std::pair<ip::address, ip::address>> parseLine(std::string_view line)
{
  // eMule: "first - last , level , description"
  if (const auto comma = line.find(','); comma != std::string_view::npos) {
    const auto rest = line.substr(comma + 1);
    const auto level = text::toInt(text::trim(rest.substr(0, rest.find(','))));
    if (level && *level > 127) {
      return std::nullopt;
    }
    return span(line.substr(0, comma));
  }
  // P2P: "name:first-last"; the name may itself contain colons.
  if (const auto colon = line.rfind(':'); colon != std::string_view::npos &&
                                          line.find('-', colon) != std::string_view::npos &&
                                          line.find('.', colon) != std::string_view::npos) {
    return span(line.substr(colon + 1));
  }
  if (line.find('-') != std::string_view::npos) {
    return span(line);
  }
  return cidr(line);
}

} // namespace

Blocklist parseBlocklist(std::string_view content)
{
  Blocklist result;
  for (const auto& raw : text::split(content, '\n')) {
    const auto line = text::trim(raw);
    if (line.empty() || line.front() == '#' || line.starts_with("//")) {
      continue;
    }
    if (const auto range = parseLine(line)) {
      result.filter.add_rule(range->first, range->second, libtorrent::ip_filter::blocked);
      ++result.rules;
    }
    else {
      ++result.skipped;
    }
  }
  return result;
}

} // namespace aria2::bt
