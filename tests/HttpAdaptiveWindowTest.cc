#include "HttpAdaptiveWindow.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class HttpAdaptiveWindowTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HttpAdaptiveWindowTest);
  CPPUNIT_TEST(testStartsConservatively);
  CPPUNIT_TEST(testSlowStartGrowsQuicklyThenLinear);
  CPPUNIT_TEST(testFailureHalvesAndCooldownBlocksImmediateGrowth);
  CPPUNIT_TEST(testRateLimitLocksToSingleStreamThenProbes);
  CPPUNIT_TEST(testRateLimitStrikeExtendsRecovery);
  CPPUNIT_TEST(testRateLimitLearnsSoftCeilingAndProbesSlowly);
  CPPUNIT_TEST(testRepeatedRateLimitLowersSoftCeiling);
  CPPUNIT_TEST(testRangeUnsupportedLocksToSingleStream);
  CPPUNIT_TEST_SUITE_END();

public:
  void testStartsConservatively();
  void testSlowStartGrowsQuicklyThenLinear();
  void testFailureHalvesAndCooldownBlocksImmediateGrowth();
  void testRateLimitLocksToSingleStreamThenProbes();
  void testRateLimitStrikeExtendsRecovery();
  void testRateLimitLearnsSoftCeilingAndProbesSlowly();
  void testRepeatedRateLimitLowersSoftCeiling();
  void testRangeUnsupportedLocksToSingleStream();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpAdaptiveWindowTest);

void HttpAdaptiveWindowTest::testStartsConservatively()
{
  HttpAdaptiveWindow window;

  CPPUNIT_ASSERT_EQUAL(4, window.limit(64));
  CPPUNIT_ASSERT_EQUAL(2, window.limit(2));
}

void HttpAdaptiveWindowTest::testSlowStartGrowsQuicklyThenLinear()
{
  HttpAdaptiveWindow window;

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(8, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(16, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(17, window.limit(64));
}

void HttpAdaptiveWindowTest::testFailureHalvesAndCooldownBlocksImmediateGrowth()
{
  HttpAdaptiveWindow window;
  window.onSuccess(64);
  window.onSuccess(64);

  window.onTransientFailure();
  CPPUNIT_ASSERT_EQUAL(8, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(8, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(8, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(9, window.limit(64));
}

void HttpAdaptiveWindowTest::testRateLimitLocksToSingleStreamThenProbes()
{
  HttpAdaptiveWindow window;
  window.onSuccess(64);
  window.onSuccess(64);

  window.onRateLimited();
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(2, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(3, window.limit(64));
}

void HttpAdaptiveWindowTest::testRateLimitStrikeExtendsRecovery()
{
  HttpAdaptiveWindow window;

  window.onRateLimited();
  window.onRateLimited();

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));
}

void HttpAdaptiveWindowTest::testRateLimitLearnsSoftCeilingAndProbesSlowly()
{
  HttpAdaptiveWindow window;
  window.onSuccess(64);
  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(16, window.limit(64));

  window.onRateLimited();
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(2, window.limit(64));

  for (int i = 0; i < 6; ++i) {
    window.onSuccess(64);
  }
  CPPUNIT_ASSERT_EQUAL(8, window.limit(64));

  for (int i = 0; i < 7; ++i) {
    window.onSuccess(64);
    CPPUNIT_ASSERT_EQUAL(8, window.limit(64));
  }

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(9, window.limit(64));
}

void HttpAdaptiveWindowTest::testRepeatedRateLimitLowersSoftCeiling()
{
  HttpAdaptiveWindow window;
  window.onSuccess(64);
  window.onRateLimited();
  window.onSuccess(64);
  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(2, window.limit(64));

  window.onRateLimited();
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  for (int i = 0; i < 2; ++i) {
    window.onSuccess(64);
    CPPUNIT_ASSERT_EQUAL(1, window.limit(64));
  }

  for (int i = 0; i < 7; ++i) {
    window.onSuccess(64);
    CPPUNIT_ASSERT_EQUAL(1, window.limit(64));
  }

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(2, window.limit(64));
}

void HttpAdaptiveWindowTest::testRangeUnsupportedLocksToSingleStream()
{
  HttpAdaptiveWindow window;
  window.onSuccess(64);
  window.onRangeUnsupported();

  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));

  window.onSuccess(64);
  CPPUNIT_ASSERT_EQUAL(1, window.limit(64));
}

} // namespace aria2
