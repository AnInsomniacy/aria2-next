#include "HttpTailReclaimPolicy.h"

#include "a2doctest.h"

#include "a2functional.h"

namespace aria2 {

class HttpTailReclaimPolicyTest {

public:
  void testDetectTailBlocked();
  void testRejectNonTailBlockedStates();
  void testReclaimOnlyHardStalledTail();
};

A2_TEST(HttpTailReclaimPolicyTest, testDetectTailBlocked)
A2_TEST(HttpTailReclaimPolicyTest, testRejectNonTailBlockedStates)
A2_TEST(HttpTailReclaimPolicyTest, testReclaimOnlyHardStalledTail)

namespace {
HttpTailReclaimState baseState()
{
  HttpTailReclaimState state;
  state.protocol = "https";
  state.p2pInvolved = false;
  state.totalLength = 128_m;
  state.pendingLength = 2_m;
  state.hasMissingUnusedPiece = false;
  state.numConcurrentCommand = 64;
  state.numStreamCommand = 2;
  state.currentSessionDownloadLength = 512_k;
  state.lastSessionDownloadLength = 512_k;
  state.noProgressTime = 10_s;
  state.stallTime = 10_s;
  return state;
}
} // namespace

void HttpTailReclaimPolicyTest::testDetectTailBlocked()
{
  REQUIRE(isHttpTailBlocked(baseState()));
}

void HttpTailReclaimPolicyTest::testRejectNonTailBlockedStates()
{
  auto state = baseState();
  state.hasMissingUnusedPiece = true;
  REQUIRE(!isHttpTailBlocked(state));

  state = baseState();
  state.numConcurrentCommand = 1;
  REQUIRE(!isHttpTailBlocked(state));

  state = baseState();
  state.numStreamCommand = 64;
  REQUIRE(!isHttpTailBlocked(state));

  state = baseState();
  state.protocol = "ftp";
  REQUIRE(!isHttpTailBlocked(state));

  state = baseState();
  state.p2pInvolved = true;
  REQUIRE(!isHttpTailBlocked(state));

  state = baseState();
  state.totalLength = 0;
  REQUIRE(!isHttpTailBlocked(state));

  state = baseState();
  state.pendingLength = 0;
  REQUIRE(!isHttpTailBlocked(state));
}

void HttpTailReclaimPolicyTest::testReclaimOnlyHardStalledTail()
{
  REQUIRE(shouldReclaimHttpTailSegment(baseState()));

  auto state = baseState();
  state.currentSessionDownloadLength += 1;
  REQUIRE(!shouldReclaimHttpTailSegment(state));

  state = baseState();
  state.noProgressTime = 9_s;
  REQUIRE(!shouldReclaimHttpTailSegment(state));
}

} // namespace aria2
