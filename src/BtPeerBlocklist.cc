/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 The aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#include "BtPeerBlocklist.h"

#include <algorithm>
#include <sstream>

#include "BufferedFile.h"
#include "DlAbortEx.h"
#include "Log.h"
#include "SocketCore.h"
#include "fmt.h"
#include "util.h"

namespace aria2 {

namespace {

struct ParsedAddress {
  std::array<unsigned char, 16> bytes;
  size_t length;
};

bool lessAddress(const std::array<unsigned char, 16>& lhs,
                 const std::array<unsigned char, 16>& rhs, size_t length)
{
  return std::lexicographical_compare(lhs.begin(), lhs.begin() + length,
                                      rhs.begin(), rhs.begin() + length);
}

bool lessOrEqualAddress(const std::array<unsigned char, 16>& lhs,
                        const std::array<unsigned char, 16>& rhs,
                        size_t length)
{
  return !lessAddress(rhs, lhs, length);
}

ParsedAddress parseAddress(const std::string& value)
{
  ParsedAddress address{};
  address.length = net::getBinAddr(address.bytes.data(), value);
  if (address.length != 4 && address.length != 16) {
    throw DL_ABORT_EX(fmt("Invalid IP address: %s", value.c_str()));
  }

  if (address.length == 16 &&
      std::all_of(address.bytes.begin(), address.bytes.begin() + 10,
                  [](unsigned char byte) { return byte == 0; }) &&
      address.bytes[10] == 0xff && address.bytes[11] == 0xff) {
    std::copy(address.bytes.begin() + 12, address.bytes.end(),
              address.bytes.begin());
    std::fill(address.bytes.begin() + 4, address.bytes.end(), 0);
    address.length = 4;
  }
  return address;
}

unsigned int parsePrefixLength(const std::string& value, size_t addressLength)
{
  uint32_t prefixLength;
  if (!util::parseUIntNoThrow(prefixLength, value) ||
      prefixLength > addressLength * 8) {
    throw DL_ABORT_EX(fmt("Invalid CIDR prefix length: %s", value.c_str()));
  }
  return prefixLength;
}

BtPeerBlocklist::Range createRange(const std::string& rule,
                                   size_t& addressLength)
{
  auto slash = rule.find('/');
  auto addressText = slash == std::string::npos ? rule : rule.substr(0, slash);
  auto address = parseAddress(addressText);
  addressLength = address.length;
  auto prefixLength = slash == std::string::npos
                          ? static_cast<unsigned int>(addressLength * 8)
                          : parsePrefixLength(rule.substr(slash + 1),
                                              addressLength);

  BtPeerBlocklist::Range range{};
  for (size_t i = 0; i < addressLength; ++i) {
    auto remaining = prefixLength > i * 8 ? prefixLength - i * 8 : 0;
    unsigned char mask = remaining >= 8
                             ? 0xff
                             : static_cast<unsigned char>(0xff << (8 - remaining));
    range.first[i] = address.bytes[i] & mask;
    range.last[i] = address.bytes[i] | static_cast<unsigned char>(~mask);
  }
  return range;
}

void mergeRanges(std::vector<BtPeerBlocklist::Range>& ranges, size_t length)
{
  std::sort(ranges.begin(), ranges.end(),
            [length](const BtPeerBlocklist::Range& lhs,
                     const BtPeerBlocklist::Range& rhs) {
              return lessAddress(lhs.first, rhs.first, length);
            });
  if (ranges.empty()) {
    return;
  }

  size_t output = 0;
  for (size_t i = 1; i < ranges.size(); ++i) {
    if (lessOrEqualAddress(ranges[i].first, ranges[output].last, length)) {
      if (lessAddress(ranges[output].last, ranges[i].last, length)) {
        ranges[output].last = ranges[i].last;
      }
    }
    else {
      ranges[++output] = ranges[i];
    }
  }
  ranges.resize(output + 1);
}

bool containsAddress(const std::vector<BtPeerBlocklist::Range>& ranges,
                     const ParsedAddress& address)
{
  auto i = std::upper_bound(
      ranges.begin(), ranges.end(), address.bytes,
      [&address](const std::array<unsigned char, 16>& value,
                 const BtPeerBlocklist::Range& range) {
        return lessAddress(value, range.first, address.length);
      });
  if (i == ranges.begin()) {
    return false;
  }
  --i;
  return lessOrEqualAddress(i->first, address.bytes, address.length) &&
         lessOrEqualAddress(address.bytes, i->last, address.length);
}

} // namespace

BtPeerBlocklist::BtPeerBlocklist() : ruleCount_(0), revision_(1) {}

void BtPeerBlocklist::clear()
{
  replace({}, "clear");
}

void BtPeerBlocklist::load(const std::string& path)
{
  BufferedFile file(path.c_str(), BufferedFile::READ);
  if (!file) {
    throw DL_ABORT_EX(fmt("Cannot open BT peer blocklist: %s", path.c_str()));
  }
  std::stringstream input;
  file.transfer(input);
  load(input, path);
}

void BtPeerBlocklist::load(std::istream& input, const std::string& source)
{
  std::vector<std::string> rules;
  std::string line;
  while (std::getline(input, line)) {
    rules.push_back(std::move(line));
  }
  replace(rules, source);
}

void BtPeerBlocklist::replace(const std::vector<std::string>& rules,
                              const std::string& source)
{
  std::vector<Range> ipv4Ranges;
  std::vector<Range> ipv6Ranges;
  size_t ruleCount = 0;
  size_t lineNumber = 0;
  for (auto line : rules) {
    ++lineNumber;
    line = util::strip(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    try {
      size_t addressLength;
      auto range = createRange(line, addressLength);
      (addressLength == 4 ? ipv4Ranges : ipv6Ranges).push_back(range);
      ++ruleCount;
    }
    catch (RecoverableException& ex) {
      throw DL_ABORT_EX(fmt("Invalid BT peer blocklist rule at %s:%lu: %s",
                            source.c_str(),
                            static_cast<unsigned long>(lineNumber), ex.what()));
    }
  }

  mergeRanges(ipv4Ranges, 4);
  mergeRanges(ipv6Ranges, 16);
  ipv4Ranges_.swap(ipv4Ranges);
  ipv6Ranges_.swap(ipv6Ranges);
  ruleCount_ = ruleCount;
  ++revision_;
  A2_LOG_INFO(fmt("Loaded %lu BT peer blocklist rules from %s",
                  static_cast<unsigned long>(ruleCount_), source.c_str()));
}

bool BtPeerBlocklist::contains(const std::string& ipaddr) const
{
  if (ruleCount_ == 0) {
    return false;
  }
  ParsedAddress address;
  try {
    address = parseAddress(ipaddr);
  }
  catch (RecoverableException&) {
    return false;
  }
  return containsAddress(address.length == 4 ? ipv4Ranges_ : ipv6Ranges_,
                         address);
}

} // namespace aria2
