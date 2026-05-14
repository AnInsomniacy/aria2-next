#include "Exception.h"

#include <iostream>
#include <cppunit/extensions/HelperMacros.h>

#include "DownloadFailureException.h"
#include "util.h"
#include "A2STR.h"

namespace aria2 {

class ExceptionTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(ExceptionTest);
  CPPUNIT_TEST(testStackTrace);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testStackTrace();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ExceptionTest);

void ExceptionTest::testStackTrace()
{
  DownloadFailureException c1 =
      DOWNLOAD_FAILURE_EXCEPTION2("cause1", error_code::TIME_OUT);
  DownloadFailureException c2 = DOWNLOAD_FAILURE_EXCEPTION2("cause2", c1);
  DownloadFailureException e =
      DOWNLOAD_FAILURE_EXCEPTION2("exception thrown", c2);

  auto stackTrace = util::replace(e.stackTrace(), std::string(A2_TEST_DIR) + "/",
                                  "");
  stackTrace =
      util::replace(stackTrace, std::string(__FILE__), "ExceptionTest.cc");

  CPPUNIT_ASSERT(stackTrace.find(
                     "Exception: [ExceptionTest.cc:") != std::string::npos);
  CPPUNIT_ASSERT(stackTrace.find("] errorCode=2 exception thrown\n") !=
                 std::string::npos);
  CPPUNIT_ASSERT(stackTrace.find(
                     "  -> [ExceptionTest.cc:") != std::string::npos);
  CPPUNIT_ASSERT(stackTrace.find("] errorCode=2 cause2\n") !=
                 std::string::npos);
  CPPUNIT_ASSERT(stackTrace.find("] errorCode=2 cause1\n") !=
                 std::string::npos);
  CPPUNIT_ASSERT(stackTrace.find("exception thrown\n  ->") <
                 stackTrace.find("cause2\n  ->"));
  CPPUNIT_ASSERT(stackTrace.find("cause2\n  ->") < stackTrace.find("cause1\n"));
}

} // namespace aria2
