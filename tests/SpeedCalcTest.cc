#include "SpeedCalc.h"
#include "wallclock.h"
#include <string>
#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class SpeedCalcTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SpeedCalcTest);
  CPPUNIT_TEST(testColdStartDoesNotPublishSubSecondSpike);
  CPPUNIT_TEST(testPublishesAfterMinimumLiveSample);
  CPPUNIT_TEST(testActiveIdleGapKeepsSmoothedSpeed);
  CPPUNIT_TEST(testTenSecondWindowExpiresNaturally);
  CPPUNIT_TEST(testResetClearsLiveSpeed);
  CPPUNIT_TEST_SUITE_END();

private:
public:
  void setUp() { global::wallclock().reset(); }

  void testColdStartDoesNotPublishSubSecondSpike();
  void testPublishesAfterMinimumLiveSample();
  void testActiveIdleGapKeepsSmoothedSpeed();
  void testTenSecondWindowExpiresNaturally();
  void testResetClearsLiveSpeed();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SpeedCalcTest);

void SpeedCalcTest::testColdStartDoesNotPublishSubSecondSpike()
{
  SpeedCalc calc;
  calc.update(256_k);

  CPPUNIT_ASSERT_EQUAL(0, calc.calculateSpeed());

  global::wallclock().advance(250_ms);

  CPPUNIT_ASSERT_EQUAL(0, calc.calculateSpeed());
  CPPUNIT_ASSERT_EQUAL(0, calc.getMaxSpeed());
}

void SpeedCalcTest::testPublishesAfterMinimumLiveSample()
{
  SpeedCalc calc;
  calc.update(1024);

  global::wallclock().advance(500_ms);

  CPPUNIT_ASSERT_EQUAL(2048, calc.calculateSpeed());
  CPPUNIT_ASSERT_EQUAL(2048, calc.getMaxSpeed());
}

void SpeedCalcTest::testActiveIdleGapKeepsSmoothedSpeed()
{
  SpeedCalc calc;
  calc.update(1024);

  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(2048, calc.calculateSpeed());

  global::wallclock().advance(500_ms);

  CPPUNIT_ASSERT_EQUAL(1024, calc.calculateSpeed());
}

void SpeedCalcTest::testTenSecondWindowExpiresNaturally()
{
  SpeedCalc calc;
  calc.update(1000);

  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(2000, calc.calculateSpeed());

  global::wallclock().advance(9_s);
  CPPUNIT_ASSERT(calc.calculateSpeed() > 0);

  global::wallclock().advance(1_s);
  CPPUNIT_ASSERT_EQUAL(0, calc.calculateSpeed());
}

void SpeedCalcTest::testResetClearsLiveSpeed()
{
  SpeedCalc calc;
  calc.update(1000);

  global::wallclock().advance(500_ms);

  CPPUNIT_ASSERT(calc.calculateSpeed() > 0);

  calc.reset();

  CPPUNIT_ASSERT_EQUAL(0, calc.calculateSpeed());
}

} // namespace aria2
