/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <libtorrent/ip_filter.hpp>

#include <string_view>

namespace aria2::bt {

// Parses a peer blocklist. Each line is one of:
//   203.0.113.7, 2001:db8::/32                        address or CIDR block
//   203.0.113.0 - 203.0.113.255 , 100 , comment       eMule ipfilter.dat
//   Some name:203.0.113.0-203.0.113.255               PeerGuardian P2P
// eMule lines with an access level above 127 are allowed, as in eMule. Blank
// lines, comments (# or //) and unreadable lines are skipped.
struct Blocklist {
  libtorrent::ip_filter filter;
  size_t rules = 0;
  size_t skipped = 0;
};

Blocklist parseBlocklist(std::string_view content);

} // namespace aria2::bt
