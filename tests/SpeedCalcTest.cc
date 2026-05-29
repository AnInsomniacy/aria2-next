#include "SpeedCalc.h"
#include "wallclock.h"
#include <string>
#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class SpeedCalcTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SpeedCalcTest);
  CPPUNIT_TEST(testColdStartDoesNotPublishSubSecondSpike);
  CPPUNIT_TEST(testPublishesOneSecondRate);
  CPPUNIT_TEST(testIdleSecondClearsLiveSpeed);
  CPPUNIT_TEST(testFiveSecondRollingWindow);
  CPPUNIT_TEST(testResetClearsLiveSpeed);
  CPPUNIT_TEST_SUITE_END();

private:
public:
  void setUp() { global::wallclock().reset(); }

  void testColdStartDoesNotPublishSubSecondSpike();
  void testPublishesOneSecondRate();
  void testIdleSecondClearsLiveSpeed();
  void testFiveSecondRollingWindow();
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

void SpeedCalcTest::testPublishesOneSecondRate()
{
  SpeedCalc calc;
  calc.update(1024);

  global::wallclock().advance(500_ms);

  CPPUNIT_ASSERT_EQUAL(2048, calc.calculateSpeed());
  CPPUNIT_ASSERT_EQUAL(2048, calc.getMaxSpeed());
}

void SpeedCalcTest::testIdleSecondClearsLiveSpeed()
{
  SpeedCalc calc;
  calc.update(1024);

  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(2048, calc.calculateSpeed());

  global::wallclock().advance(500_ms);

  CPPUNIT_ASSERT_EQUAL(0, calc.calculateSpeed());
}

void SpeedCalcTest::testFiveSecondRollingWindow()
{
  SpeedCalc calc;
  calc.update(1000);

  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(2000, calc.calculateSpeed());

  calc.update(2000);
  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(3000, calc.calculateSpeed());

  calc.update(3000);
  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(4000, calc.calculateSpeed());

  calc.update(4000);
  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(5000, calc.calculateSpeed());

  calc.update(5000);
  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(6000, calc.calculateSpeed());

  calc.update(6000);
  global::wallclock().advance(500_ms);
  CPPUNIT_ASSERT_EQUAL(7000, calc.calculateSpeed());
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
