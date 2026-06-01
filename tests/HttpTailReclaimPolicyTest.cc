#include "HttpTailReclaimPolicy.h"

#include <cppunit/extensions/HelperMacros.h>

#include "a2functional.h"

namespace aria2 {

class HttpTailReclaimPolicyTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HttpTailReclaimPolicyTest);
  CPPUNIT_TEST(testDetectTailBlocked);
  CPPUNIT_TEST(testRejectNonTailBlockedStates);
  CPPUNIT_TEST(testReclaimOnlyHardStalledTail);
  CPPUNIT_TEST_SUITE_END();

public:
  void testDetectTailBlocked();
  void testRejectNonTailBlockedStates();
  void testReclaimOnlyHardStalledTail();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpTailReclaimPolicyTest);

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
  CPPUNIT_ASSERT(isHttpTailBlocked(baseState()));
}

void HttpTailReclaimPolicyTest::testRejectNonTailBlockedStates()
{
  auto state = baseState();
  state.hasMissingUnusedPiece = true;
  CPPUNIT_ASSERT(!isHttpTailBlocked(state));

  state = baseState();
  state.numConcurrentCommand = 1;
  CPPUNIT_ASSERT(!isHttpTailBlocked(state));

  state = baseState();
  state.numStreamCommand = 64;
  CPPUNIT_ASSERT(!isHttpTailBlocked(state));

  state = baseState();
  state.protocol = "ftp";
  CPPUNIT_ASSERT(!isHttpTailBlocked(state));

  state = baseState();
  state.p2pInvolved = true;
  CPPUNIT_ASSERT(!isHttpTailBlocked(state));

  state = baseState();
  state.totalLength = 0;
  CPPUNIT_ASSERT(!isHttpTailBlocked(state));

  state = baseState();
  state.pendingLength = 0;
  CPPUNIT_ASSERT(!isHttpTailBlocked(state));
}

void HttpTailReclaimPolicyTest::testReclaimOnlyHardStalledTail()
{
  CPPUNIT_ASSERT(shouldReclaimHttpTailSegment(baseState()));

  auto state = baseState();
  state.currentSessionDownloadLength += 1;
  CPPUNIT_ASSERT(!shouldReclaimHttpTailSegment(state));

  state = baseState();
  state.noProgressTime = 9_s;
  CPPUNIT_ASSERT(!shouldReclaimHttpTailSegment(state));
}

} // namespace aria2
