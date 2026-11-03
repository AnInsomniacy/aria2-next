/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "bt/Blocklist.h"
#include "bt/Metainfo.h"

#include <doctest/doctest.h>
#include <libtorrent/address.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>

#include <algorithm>
#include <string>
#include <vector>

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

TEST_CASE("additional trackers follow retained tiers without duplicate entries")
{
  libtorrent::add_torrent_params params;
  params.trackers = {"http://a.test/announce", "http://removed.test/announce"};
  params.tracker_tiers = {4, 200};
  Options options;
  options.set("bt-exclude-tracker", "http://removed.test/announce");
  options.set("bt-tracker", " http://a.test/announce, https://b.test/announce, "
                            "https://b.test/announce,,udp://c.test/announce ");

  bt::applyTrackerOptions(params, options);

  const std::vector<std::string> expected{"http://a.test/announce",
                                          "https://b.test/announce",
                                          "udp://c.test/announce"};
  CHECK(params.trackers == expected);
  CHECK(params.tracker_tiers == std::vector<int>{4, 5, 6});
}

TEST_CASE("additional trackers share the last tier when its range is exhausted")
{
  libtorrent::add_torrent_params params;
  params.trackers = {"http://existing.test/announce"};
  params.tracker_tiers = {254};
  SUBCASE("one tier remains") {}
  SUBCASE("the final tier is already occupied")
  {
    params.tracker_tiers = {255};
  }
  const auto originalTier = params.tracker_tiers.front();
  Options options;
  options.set("bt-tracker", "http://a.test/announce,udp://b.test/announce");

  bt::applyTrackerOptions(params, options);

  const std::vector<std::string> expected{"http://existing.test/announce",
                                          "http://a.test/announce",
                                          "udp://b.test/announce"};
  CHECK(params.trackers == expected);
  CHECK(params.tracker_tiers == std::vector<int>{originalTier, 255, 255});
}

TEST_CASE(
    "large additional tracker lists preserve order without tier wraparound")
{
  libtorrent::add_torrent_params params;
  Options options;
  std::string list;
  std::vector<std::string> expected;
  for (int i = 0; i < 270; ++i) {
    const auto url = "http://tracker" + std::to_string(i) + ".test/announce";
    if (!list.empty()) {
      list += ',';
    }
    list += url;
    expected.push_back(url);
  }
  options.set("bt-tracker", list);

  bt::applyTrackerOptions(params, options);

  CHECK(params.trackers == expected);
  REQUIRE(params.tracker_tiers.size() == expected.size());
  CHECK(
      std::is_sorted(params.tracker_tiers.begin(), params.tracker_tiers.end()));
  CHECK(params.tracker_tiers.front() == 0);
  CHECK(params.tracker_tiers[254] == 254);
  CHECK(std::all_of(params.tracker_tiers.begin() + 255,
                    params.tracker_tiers.end(),
                    [](int tier) { return tier == 255; }));
  const auto tiers = params.tracker_tiers;
  bt::applyTrackerOptions(params, options);
  CHECK(params.trackers == expected);
  CHECK(params.tracker_tiers == tiers);
}

TEST_CASE("excluding all trackers resets the tier for configured replacements")
{
  libtorrent::add_torrent_params params;
  params.trackers = {"http://old.test/announce"};
  params.tracker_tiers = {255};
  Options options;
  options.set("bt-exclude-tracker", "*");
  options.set("bt-tracker", "http://new.test/announce");

  bt::applyTrackerOptions(params, options);

  CHECK(params.trackers ==
        std::vector<std::string>{"http://new.test/announce"});
  CHECK(params.tracker_tiers == std::vector<int>{0});
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
