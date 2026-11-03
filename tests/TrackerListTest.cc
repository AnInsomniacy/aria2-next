/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include <doctest/doctest.h>
#include <libtorrent/announce_entry.hpp>
#include <libtorrent/aux_/tracker_list.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace lt = libtorrent;
using TrackerOrder = std::vector<std::pair<std::string, int>>;

struct TrackerListFixture {
  lt::aux::tracker_list trackers;

  void assign(std::initializer_list<std::pair<std::string, int>> entries)
  {
    std::vector<lt::announce_entry> values;
    for (const auto& [url, tier] : entries) {
      lt::announce_entry entry(url);
      entry.tier = static_cast<std::uint8_t>(tier);
      values.push_back(std::move(entry));
    }
    trackers.replace(values);
  }

  TrackerOrder order() const
  {
    TrackerOrder result;
    for (const auto& entry : trackers) {
      result.emplace_back(entry.url, entry.tier);
    }
    return result;
  }
};

} // namespace

TEST_CASE_FIXTURE(
    TrackerListFixture,
    "UDP tracker preference keeps tier order and tracker identity")
{
  assign({{"http://a.test/announce", 0},
          {"https://h.test:443/announce", 0},
          {"http://b.test/announce", 1},
          {"udp://h.test:6969/announce", 2}});
  auto* http = trackers.find_tracker("https://h.test:443/announce");
  auto* udp = trackers.find_tracker("udp://h.test:6969/announce");
  REQUIRE(http != nullptr);
  REQUIRE(udp != nullptr);
  http->trackerid = "http-session";
  http->verified = true;
  udp->trackerid = "udp-session";
  udp->fail_limit = 7;
  trackers.record_working(http);

  trackers.prioritize_udp_trackers();

  const TrackerOrder expected{{"http://a.test/announce", 0},
                              {"udp://h.test:6969/announce", 0},
                              {"http://b.test/announce", 1},
                              {"https://h.test:443/announce", 2}};
  CHECK(order() == expected);
  CHECK(trackers.find_tracker("https://h.test:443/announce") == http);
  CHECK(trackers.find_tracker("udp://h.test:6969/announce") == udp);
  CHECK(http->trackerid == "http-session");
  CHECK(http->verified);
  CHECK(udp->trackerid == "udp-session");
  CHECK(udp->fail_limit == 7);
  CHECK(trackers.last_working() == http);
  for (const auto& entry : trackers) {
    CHECK(trackers.find_tracker(entry.url) == &entry);
  }
  trackers.prioritize_udp_trackers();
  CHECK(order() == expected);
}

TEST_CASE_FIXTURE(
    TrackerListFixture,
    "UDP tracker preference visits consecutive entries including the last")
{
  assign({{"http://a.test/announce", 0},
          {"http://b.test/announce", 1},
          {"udp://a.test/announce", 2},
          {"udp://b.test/announce", 255}});

  trackers.prioritize_udp_trackers();

  const TrackerOrder expected{{"udp://a.test/announce", 0},
                              {"udp://b.test/announce", 1},
                              {"http://a.test/announce", 2},
                              {"http://b.test/announce", 255}};
  CHECK(order() == expected);
}

TEST_CASE_FIXTURE(
    TrackerListFixture,
    "UDP tracker preference handles adjacent entries sharing a tier")
{
  assign({{"http://a.test/announce", 255}, {"udp://a.test/announce", 255}});

  trackers.prioritize_udp_trackers();

  const TrackerOrder expected{{"udp://a.test/announce", 255},
                              {"http://a.test/announce", 255}};
  CHECK(order() == expected);
}

TEST_CASE_FIXTURE(
    TrackerListFixture,
    "UDP tracker preference preserves unrelated hosts and existing order")
{
  SUBCASE("empty list") {}
  SUBCASE("only UDP") { assign({{"udp://a.test/announce", 0}}); }
  SUBCASE("HTTP alternatives and unrelated UDP")
  {
    assign({{"udp://a.test/announce", 0},
            {"http://a.test/announce", 1},
            {"https://a.test/announce", 1},
            {"udp://b.test/announce", 2}});
  }
  const auto before = order();

  trackers.prioritize_udp_trackers();

  CHECK(order() == before);
}
