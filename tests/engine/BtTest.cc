/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "bt/Blocklist.h"
#include "bt/Metainfo.h"

#include <doctest/doctest.h>
#include <libtorrent/address.hpp>

using namespace aria2;

namespace {
bool blocked(const bt::Blocklist& list, const char* address)
{
  return list.filter.access(libtorrent::make_address(address)) &
         libtorrent::ip_filter::blocked;
}
} // namespace

TEST_CASE("file selections")
{
  CHECK(bt::parseSelection("1-3,5", 6) == std::vector<int>{1, 2, 3, 5});
  CHECK_THROWS_AS(bt::parseSelection("0", 6), Error);
  CHECK_THROWS_AS(bt::parseSelection("4-9", 6), Error);
  CHECK_THROWS_AS(bt::parseSelection("x", 6), Error);
}

TEST_CASE("blocklists accept CIDR, eMule and PeerGuardian lines")
{
  const auto list =
      bt::parseBlocklist("# comment\n"
                         "203.0.113.0/24\n"
                         "2001:db8::/32\n"
                         "198.051.100.000 - 198.051.100.255 , 100 , emule\n"
                         "192.0.2.0 - 192.0.2.255 , 200 , allowed in eMule\n"
                         "Some range:100.64.0.0-100.64.0.10\n"
                         "not an address\n");
  // The allowed eMule range is not a rule.
  CHECK(list.rules == 4);
  CHECK(list.skipped == 2);
  CHECK(blocked(list, "203.0.113.9"));
  CHECK(blocked(list, "2001:db8::1"));
  CHECK(blocked(list, "198.51.100.7"));
  CHECK_FALSE(blocked(list, "192.0.2.1"));
  CHECK(blocked(list, "100.64.0.10"));
  CHECK_FALSE(blocked(list, "100.64.0.11"));
}

TEST_CASE("magnet links round-trip their identity")
{
  const auto params = bt::parseMagnet(
      "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&dn=Name&tr="
      "http%3A%2F%2Ft%2Fannounce");
  CHECK(bt::hashText(params.info_hashes) ==
        "0123456789abcdef0123456789abcdef01234567");
  const auto link = bt::magnetLink(params);
  CHECK(link.find("xt=urn:btih:0123456789abcdef0123456789abcdef01234567") !=
        std::string::npos);
  CHECK(link.find("dn=Name") != std::string::npos);
  CHECK(bt::isMagnet(
      "MAGNET:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567"));
  CHECK_THROWS_AS(bt::parseMagnet("magnet:?dn=nothing"), Error);
}
