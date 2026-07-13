#include "Exception.h"

#include <iostream>
#include "a2doctest.h"

#include "DownloadFailureException.h"
#include "util.h"

namespace aria2 {

class ExceptionTest {


public:
  void setUp() {}

  void tearDown() {}

  void testStackTrace();
};

A2_TEST(ExceptionTest, testStackTrace)

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

  REQUIRE(stackTrace.find(
                     "Exception: [ExceptionTest.cc:") != std::string::npos);
  REQUIRE(stackTrace.find("] errorCode=2 exception thrown\n") !=
                 std::string::npos);
  REQUIRE(stackTrace.find(
                     "  -> [ExceptionTest.cc:") != std::string::npos);
  REQUIRE(stackTrace.find("] errorCode=2 cause2\n") !=
                 std::string::npos);
  REQUIRE(stackTrace.find("] errorCode=2 cause1\n") !=
                 std::string::npos);
  REQUIRE(stackTrace.find("exception thrown\n  ->") <
                 stackTrace.find("cause2\n  ->"));
  REQUIRE(stackTrace.find("cause2\n  ->") < stackTrace.find("cause1\n"));
}

} // namespace aria2
