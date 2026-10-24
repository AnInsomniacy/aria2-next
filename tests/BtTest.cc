/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "bt/Blocklist.h"
#include "bt/Metainfo.h"

#include <doctest/doctest.h>
#include <libtorrent/address.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <algorithm>

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

TEST_CASE("a BitTorrent session starts on its own threads")
{
  // Starting registers sockets with Asio from libtorrent's own code, which
  // crashed when libtorrent and the engine were built as different C++
  // standards and so disagreed on the layout of Asio's reactor.
  libtorrent::settings_pack pack;
  pack.set_str(libtorrent::settings_pack::listen_interfaces, "127.0.0.1:0");
  pack.set_bool(libtorrent::settings_pack::enable_dht, false);
  pack.set_bool(libtorrent::settings_pack::enable_lsd, false);
  pack.set_bool(libtorrent::settings_pack::enable_upnp, false);
  pack.set_bool(libtorrent::settings_pack::enable_natpmp, false);
  libtorrent::session session(libtorrent::session_params(std::move(pack)));
  // A synchronous call runs after the session's own start-up on its thread.
  CHECK_FALSE(session.get_settings().get_bool(libtorrent::settings_pack::enable_dht));
  CHECK(session.is_listening());
}

TEST_CASE("preferring UDP trackers preserves tier order")
{
  libtorrent::settings_pack pack;
  pack.set_str(libtorrent::settings_pack::listen_interfaces, "127.0.0.1:0");
  pack.set_bool(libtorrent::settings_pack::enable_dht, false);
  pack.set_bool(libtorrent::settings_pack::enable_lsd, false);
  pack.set_bool(libtorrent::settings_pack::enable_upnp, false);
  pack.set_bool(libtorrent::settings_pack::enable_natpmp, false);
  pack.set_bool(libtorrent::settings_pack::prefer_udp_trackers, true);
  libtorrent::session session(libtorrent::session_params(std::move(pack)));

  auto params = bt::parseMagnet(
      "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567");
  params.save_path = ".";
  params.flags = libtorrent::torrent_flags::paused;
  params.trackers = {"http://tracker.test/announce",
                     "http://other.test/announce",
                     "udp://tracker.test:80/announce"};
  SUBCASE("different tiers") { params.tracker_tiers = {0, 1, 2}; }
  SUBCASE("same tier") { params.tracker_tiers = {0, 0, 0}; }

  const auto trackers = session.add_torrent(params).trackers();
  REQUIRE(trackers.size() == 3);
  CHECK(trackers[0].url == params.trackers[2]);
  CHECK(trackers[1].url == params.trackers[1]);
  CHECK(trackers[2].url == params.trackers[0]);
  CHECK(std::is_sorted(trackers.begin(), trackers.end(),
                       [](const auto& left, const auto& right) {
                         return left.tier < right.tier;
                       }));
  for (size_t i = 0; i < trackers.size(); ++i) {
    CHECK(trackers[i].tier == params.tracker_tiers[i]);
  }
}
