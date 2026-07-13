#include "TimeSeedCriteria.h"

#include "a2doctest.h"

#include "util.h"
#include "wallclock.h"

namespace aria2 {

class TimeSeedCriteriaTest {


public:
  void testEvaluate();
};

A2_TEST(TimeSeedCriteriaTest, testEvaluate)

void TimeSeedCriteriaTest::testEvaluate()
{
  TimeSeedCriteria cri(1_s);
  global::wallclock().reset();
  global::wallclock().advance(2_s);
  REQUIRE(cri.evaluate());
  cri.reset();
  cri.setDuration(10_s);
  REQUIRE(!cri.evaluate());
}

} // namespace aria2
