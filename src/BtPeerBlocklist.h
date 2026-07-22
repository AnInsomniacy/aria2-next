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
#ifndef D_BT_PEER_BLOCKLIST_H
#define D_BT_PEER_BLOCKLIST_H

#include "common.h"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace aria2 {

class BtPeerBlocklist {
public:
  struct Range {
    std::array<unsigned char, 16> first;
    std::array<unsigned char, 16> last;
  };

private:
  std::vector<Range> ipv4Ranges_;
  std::vector<Range> ipv6Ranges_;
  size_t ruleCount_;
  uint64_t revision_;

public:
  BtPeerBlocklist();

  void clear();
  void load(const std::string& path);
  void load(std::istream& input, const std::string& source);
  void replace(const std::vector<std::string>& rules,
               const std::string& source);

  bool contains(const std::string& ipaddr) const;
  size_t count() const { return ruleCount_; }
  uint64_t revision() const { return revision_; }
};

} // namespace aria2

#endif // D_BT_PEER_BLOCKLIST_H
