#include "SpeedCalc.h"
#include <string>
#include "a2doctest.h"

namespace aria2 {

class SpeedCalcTest {


private:
public:
  void setUp() {}

  void testUpdate();
};

A2_TEST(SpeedCalcTest, testUpdate)

void SpeedCalcTest::testUpdate()
{
  SpeedCalc calc;
  calc.update(1000);
}

} // namespace aria2
