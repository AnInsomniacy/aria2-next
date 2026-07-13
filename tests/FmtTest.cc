#include "fmt.h"

#include "a2doctest.h"

namespace aria2 {

class FmtTest {


public:
  void testFmt();
};

A2_TEST(FmtTest, testFmt)

void FmtTest::testFmt()
{
  int major = 1;
  int minor = 0;
  int release = 7;
  REQUIRE_EQ(std::string("aria2-1.0.7-beta"),
                       fmt("aria2-%d.%d.%d-%s", major, minor, release, "beta"));
}

} // namespace aria2
