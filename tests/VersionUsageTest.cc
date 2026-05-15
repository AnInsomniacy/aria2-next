#include "common.h"

#include <cppunit/extensions/HelperMacros.h>

#include <iostream>
#include <sstream>
#include <string>

namespace aria2 {

void showVersion();

class VersionUsageTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(VersionUsageTest);
  CPPUNIT_TEST(testShowVersionDisplaysMaintainedForkIdentity);
  CPPUNIT_TEST_SUITE_END();

public:
  void testShowVersionDisplaysMaintainedForkIdentity();
};

CPPUNIT_TEST_SUITE_REGISTRATION(VersionUsageTest);

void VersionUsageTest::testShowVersionDisplaysMaintainedForkIdentity()
{
  std::ostringstream out;
  auto* old = std::cout.rdbuf(out.rdbuf());
  showVersion();
  std::cout.rdbuf(old);

  const auto version = out.str();
  CPPUNIT_ASSERT(version.find("aria2-next version " PACKAGE_VERSION) !=
                 std::string::npos);
  CPPUNIT_ASSERT(version.find("Maintained since 2026 by AnInsomniacy") !=
                 std::string::npos);
  CPPUNIT_ASSERT(version.find(
                     "Original aria2 copyright: 2006, 2019 Tatsuhiro "
                     "Tsujikawa.") != std::string::npos);
  CPPUNIT_ASSERT(version.find("Report bugs to "
                              "https://github.com/AnInsomniacy/aria2-next/"
                              "issues") != std::string::npos);
}

} // namespace aria2
