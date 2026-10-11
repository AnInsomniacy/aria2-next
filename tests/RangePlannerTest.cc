#include <chrono>
#include <cstdint>
#include "RangePlanner.h"

#include "a2doctest.h"

namespace aria2 {

TEST_CASE("RangePlannerTest.testRestoreAndScheduleGaps")
{
  RangePlanner planner;
  planner.restore({{0, 10}, {20, 30}}, 50);
  planner.configure(50, 10, {{30, 40}});

  REQUIRE_EQ(20, planner.completedLength());
  auto first = planner.takeReady({});
  REQUIRE(first);
  CHECK_EQ(10, first->begin);
  CHECK_EQ(20, first->end);
  auto second = planner.takeReady({});
  REQUIRE(second);
  CHECK_EQ(40, second->begin);
  CHECK_EQ(50, second->end);
  CHECK(!planner.takeReady({}));
}

TEST_CASE("RangePlannerTest.testRestoredCompletionNeedsNoResponse")
{
  // A download stopped after its last byte was committed resumes complete;
  // otherwise the next request would ask for an empty range.
  RangePlanner planner;
  planner.restore({{0, 50}}, 50);
  CHECK(planner.complete());
  planner.restore({{0, 40}}, 50);
  CHECK_FALSE(planner.complete());
  CHECK_EQ(40, planner.contiguousLength());
  CHECK_EQ(50, planner.gapEnd(40, 50));
}

TEST_CASE("RangePlannerTest.testLocalizedRetry")
{
  RangePlanner planner;
  planner.configure(100, 100, {});
  auto lease = planner.takeReady({});
  REQUIRE(lease);
  planner.commit(lease->begin, 35);
  lease->attempts = 1;
  lease->uriIndex = 2;
  const auto deadline = RangePlanner::TimePoint{} + std::chrono::seconds(1);
  planner.defer(lease->remainder(35), deadline);

  CHECK_EQ(35, planner.completedLength());
  CHECK(!planner.takeReady(deadline - std::chrono::milliseconds(1)));
  auto retry = planner.takeReady(deadline);
  REQUIRE(retry);
  CHECK_EQ(35, retry->begin);
  CHECK_EQ(100, retry->end);
  CHECK_EQ(1, retry->attempts);
  CHECK_EQ(2, retry->uriIndex);
  CHECK(!planner.takeReady(deadline));
  planner.commit(retry->begin, retry->end);
  CHECK(planner.complete());
  CHECK_EQ(100, planner.completedLength());
}

TEST_CASE("RangePlannerTest.testReadyRefill")
{
  RangePlanner planner;
  planner.enqueue({0, 80, 0, 1});
  planner.enqueue({80, 120, 1, 2});

  CHECK_EQ(4, planner.refillReady(4, 20, 10));
  int64_t cursor = 0;
  while (auto lease = planner.takeReady({})) {
    CHECK_EQ(cursor, lease->begin);
    CHECK(lease->end > lease->begin);
    if (lease->begin < 80) {
      CHECK_EQ(0, lease->attempts);
      CHECK_EQ(1, lease->uriIndex);
    }
    else {
      CHECK_EQ(1, lease->attempts);
      CHECK_EQ(2, lease->uriIndex);
    }
    cursor = lease->end;
  }
  CHECK_EQ(120, cursor);
}

} // namespace aria2
