/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "bt/Blocklist.h"
#include "bt/Metainfo.h"
#include "options/Options.h"

#include <doctest/doctest.h>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/address.hpp>
#include <libtorrent/announce_entry.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <utility>
#include <vector>

using namespace aria2;

namespace {
bool blocked(const bt::Blocklist& list, const char* address)
{
  return list.filter.access(libtorrent::make_address(address)) &
         libtorrent::ip_filter::blocked;
}

libtorrent::session quietSession()
{
  libtorrent::settings_pack pack;
  pack.set_str(libtorrent::settings_pack::listen_interfaces, "127.0.0.1:0");
  pack.set_bool(libtorrent::settings_pack::enable_dht, false);
  pack.set_bool(libtorrent::settings_pack::enable_lsd, false);
  pack.set_bool(libtorrent::settings_pack::enable_upnp, false);
  pack.set_bool(libtorrent::settings_pack::enable_natpmp, false);
  pack.set_bool(libtorrent::settings_pack::prefer_udp_trackers, true);
  return libtorrent::session(libtorrent::session_params(std::move(pack)));
}

using Trackers = std::vector<std::pair<std::string, int>>;

// The trackers of a paused torrent added with `trackers`, in list order.
Trackers trackerOrder(const Trackers& trackers)
{
  auto session = quietSession();
  libtorrent::add_torrent_params params = bt::parseMagnet(
      "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567");
  for (const auto& [url, tier] : trackers) {
    params.trackers.push_back(url);
    params.tracker_tiers.push_back(tier);
  }
  params.save_path = "."; // a paused magnet writes nothing
  params.flags |= libtorrent::torrent_flags::paused;
  params.flags &= ~libtorrent::torrent_flags::auto_managed;
  Trackers result;
  for (const auto& entry : session.add_torrent(std::move(params)).trackers()) {
    result.emplace_back(entry.url, entry.tier);
  }
  return result;
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

TEST_CASE("UDP trackers move ahead of their host's other trackers by tier")
{
  // Announcing and the tracker timer both walk the list in tier order; a
  // tracker left behind a higher tier is skipped by one and scheduled by
  // the other, which spins the network thread.
  SUBCASE("a pair across tiers trades tiers too")
  {
    CHECK(trackerOrder({{"http://a.test/announce", 0},
                        {"http://b.test/announce", 1},
                        {"udp://a.test:80/announce", 2}}) ==
          Trackers{{"udp://a.test:80/announce", 0},
                   {"http://b.test/announce", 1},
                   {"http://a.test/announce", 2}});
  }
  SUBCASE("adjacent entries")
  {
    CHECK(trackerOrder({{"http://a.test/announce", 0},
                        {"udp://a.test:80/announce", 1}}) ==
          Trackers{{"udp://a.test:80/announce", 0},
                   {"http://a.test/announce", 1}});
  }
  SUBCASE("consecutive UDP trackers are each considered")
  {
    CHECK(trackerOrder({{"http://a.test/announce", 0},
                        {"http://c.test/announce", 1},
                        {"udp://a.test:80/announce", 2},
                        {"udp://c.test:80/announce", 3}}) ==
          Trackers{{"udp://a.test:80/announce", 0},
                   {"udp://c.test:80/announce", 1},
                   {"http://a.test/announce", 2},
                   {"http://c.test/announce", 3}});
  }
  SUBCASE("a tracker of the same tier keeps its place")
  {
    CHECK(trackerOrder({{"http://a.test/announce", 0},
                        {"udp://a.test:80/announce", 0}}) ==
          Trackers{{"udp://a.test:80/announce", 0},
                   {"http://a.test/announce", 0}});
  }
}

TEST_CASE("added trackers past the last tier share it")
{
  libtorrent::add_torrent_params params;
  params.trackers = {"http://own.test/announce"};
  Options options;
  std::string list;
  for (int i = 0; i < 300; ++i) {
    list += (i ? "," : "") + std::string("http://t") + std::to_string(i) +
            ".test/announce";
  }
  options.set("bt-tracker", list);
  bt::applyTrackerOptions(params, options);
  REQUIRE(params.tracker_tiers.size() == 301);
  CHECK(params.tracker_tiers[0] == 0);
  CHECK(params.tracker_tiers[1] == 1);
  CHECK(params.tracker_tiers[255] == 255);
  CHECK(params.tracker_tiers[300] == 255);
}
