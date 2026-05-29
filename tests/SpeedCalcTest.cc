#include "SpeedCalc.h"
#include <string>
#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class SpeedCalcTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SpeedCalcTest);
  CPPUNIT_TEST(testUpdate);
  CPPUNIT_TEST(testResetClearsLiveSpeed);
  CPPUNIT_TEST_SUITE_END();

private:
public:
  void setUp() {}

  void testUpdate();
  void testResetClearsLiveSpeed();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SpeedCalcTest);

void SpeedCalcTest::testUpdate()
{
  SpeedCalc calc;
  calc.update(1000);
}

void SpeedCalcTest::testResetClearsLiveSpeed()
{
  SpeedCalc calc;
  calc.update(1000);

  CPPUNIT_ASSERT(calc.calculateSpeed() > 0);

  calc.reset();

  CPPUNIT_ASSERT_EQUAL(0, calc.calculateSpeed());
}

} // namespace aria2
