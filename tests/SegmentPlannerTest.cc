/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/SegmentPlanner.h"

#include <doctest/doctest.h>

using aria2::http::Range;
using aria2::http::SegmentPlanner;

namespace {
constexpr int64_t MiB = 1 << 20;
}

TEST_CASE("ranges coalesce and report gaps")
{
  aria2::http::RangeSet set;
  set.add(0, 10);
  set.add(20, 30);
  set.add(10, 20);
  CHECK(set.ranges() == std::vector<Range>{{0, 30}});
  set.add(40, 50);
  CHECK(set.total() == 40);
  CHECK(set.covered(25, 45) == 10);
  CHECK(set.firstGap(0, 60) == Range{30, 40});
  CHECK(set.contains(40, 50));
  CHECK_FALSE(set.contains(29, 41));
}

TEST_CASE("nothing is assigned before the length is known")
{
  SegmentPlanner planner(MiB);
  CHECK_FALSE(planner.assign(1));
  planner.place(1, 0, aria2::http::unbounded);
  planner.setLength(10 * MiB);
  CHECK(planner.worker(1)->end == 10 * MiB);
}

TEST_CASE("an idle connection splits the slowest remaining range")
{
  SegmentPlanner planner(MiB);
  planner.setLength(16 * MiB);
  planner.place(1, 0, 16 * MiB);
  const auto range = planner.assign(2);
  REQUIRE(range);
  // Two equal unknown speeds share the remainder evenly.
  CHECK(range->first == 8 * MiB);
  CHECK(range->second == 16 * MiB);
  CHECK(planner.worker(1)->end == 8 * MiB);
  CHECK(range->first % 4096 == 0);
}

TEST_CASE("the split point follows measured speeds")
{
  SegmentPlanner planner(MiB);
  planner.setLength(64 * MiB);
  planner.place(1, 0, 63 * MiB);
  planner.receive(1, 1);
  planner.updateSpeed(1, 3e6, 1);
  planner.place(2, 63 * MiB, 64 * MiB);
  planner.receive(2, 1);
  planner.updateSpeed(2, 1e6, 1);
  const auto range = planner.assign(3);
  REQUIRE(range);
  // A new connection is expected to reach the typical speed (2e6), so the
  // faster connection keeps 3/5 of its remainder.
  const auto kept = planner.worker(1)->end - planner.worker(1)->position;
  const auto given = range->second - range->first;
  CHECK(kept * 2 == doctest::Approx(given * 3).epsilon(0.01));
}

TEST_CASE("small remainders are not split")
{
  SegmentPlanner planner(MiB);
  planner.setLength(MiB + MiB / 2);
  planner.place(1, 0, MiB + MiB / 2);
  CHECK_FALSE(planner.assign(2));
}

TEST_CASE("received bytes beyond the assignment are rejected")
{
  SegmentPlanner planner(MiB);
  planner.setLength(4 * MiB);
  planner.place(1, 0, MiB);
  CHECK(planner.receive(1, 2 * MiB) == MiB);
  CHECK(planner.received().total() == MiB);
}

TEST_CASE("a finished connection takes over a neighbour that has no data")
{
  SegmentPlanner planner(MiB);
  planner.setLength(8 * MiB);
  planner.place(1, 0, 8 * MiB);
  REQUIRE(planner.assign(2));
  planner.receive(1, planner.worker(1)->end);
  std::optional<int> cancelled;
  CHECK(planner.extend(1, cancelled));
  CHECK(cancelled == 2);
  CHECK(planner.worker(1)->end == 8 * MiB);
  CHECK(planner.worker(2) == nullptr);
}

TEST_CASE(
    "a stalled connection is raced only after it had time to show its speed")
{
  SegmentPlanner planner(MiB);
  planner.setLength(4 * MiB);
  planner.place(1, 0, 2 * MiB);
  planner.place(2, 2 * MiB, 4 * MiB);
  planner.receive(1, 2 * MiB);
  planner.updateSpeed(1, 8e6, 1);
  planner.release(1);
  planner.receive(2, 1);
  planner.updateSpeed(2, 1000, 1);
  CHECK_FALSE(planner.assign(3)); // younger than the race threshold
  planner.updateSpeed(2, 1000, 2);
  const auto range = planner.assign(3);
  REQUIRE(range);
  CHECK(range->first == planner.worker(2)->position);
  CHECK(range->second == 4 * MiB);
  // The duplicate is not raced again.
  CHECK_FALSE(planner.assign(4));
  planner.receive(3, range->second - range->first);
  CHECK(planner.redundant() == std::vector<int>{2});
  CHECK(planner.complete());
}

TEST_CASE("copies that never deliver do not block another copy")
{
  SegmentPlanner planner(MiB);
  planner.setLength(2 * MiB);
  planner.place(1, 0, MiB);
  planner.place(2, MiB, 2 * MiB);
  planner.receive(1, MiB);
  planner.updateSpeed(1, 8e6, 1);
  planner.release(1);
  int next = 3;
  // The original and two copies accept their requests but send nothing.
  for (; next < 5; ++next) {
    for (const auto& [id, worker] : planner.workers()) {
      planner.updateSpeed(id, 0, 2);
    }
    REQUIRE(planner.assign(next));
  }
  for (const auto& [id, worker] : planner.workers()) {
    planner.updateSpeed(id, 0, 2);
  }
  const auto range = planner.assign(next);
  REQUIRE(range);
  CHECK(*range == Range{MiB, 2 * MiB});
  // Silent copies are bounded too.
  for (++next; next < 8; ++next) {
    for (const auto& [id, worker] : planner.workers()) {
      planner.updateSpeed(id, 0, 2);
    }
    planner.assign(next);
  }
  for (const auto& [id, worker] : planner.workers()) {
    planner.updateSpeed(id, 0, 2);
  }
  CHECK_FALSE(planner.assign(next));
}

TEST_CASE("restored ranges are not assigned again")
{
  SegmentPlanner planner(MiB);
  planner.setLength(4 * MiB);
  planner.restore({{0, 3 * MiB}});
  const auto range = planner.assign(1);
  REQUIRE(range);
  CHECK(*range == Range{3 * MiB, 4 * MiB});
}

TEST_CASE("uniformly slow connections are raced against a faster finished one")
{
  SegmentPlanner planner(MiB);
  planner.setLength(3 * MiB);
  planner.place(1, 0, MiB);
  planner.place(2, MiB, 2 * MiB);
  planner.place(3, 2 * MiB, 3 * MiB);
  planner.receive(1, MiB);
  planner.noteSpeed(400e3);
  planner.release(1);
  for (const int id : {2, 3}) {
    planner.receive(id, 100 << 10);
    for (int tick = 0; tick < 10; ++tick) {
      planner.updateSpeed(id, 32e3, 0.5);
    }
  }
  const auto range = planner.assign(4);
  REQUIRE(range);
  CHECK(range->second % MiB == 0);
}
