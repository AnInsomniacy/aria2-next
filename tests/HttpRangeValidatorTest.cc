#include "HttpRangeValidator.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class HttpRangeValidatorTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HttpRangeValidatorTest);
  CPPUNIT_TEST(testAcceptsMatchingPartialContent);
  CPPUNIT_TEST(testRejectsIgnoredRangeResponse);
  CPPUNIT_TEST(testRejectsMismatchedContentRange);
  CPPUNIT_TEST(testRejectsEncodedRangeResponse);
  CPPUNIT_TEST_SUITE_END();

public:
  void testAcceptsMatchingPartialContent();
  void testRejectsIgnoredRangeResponse();
  void testRejectsMismatchedContentRange();
  void testRejectsEncodedRangeResponse();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpRangeValidatorTest);

void HttpRangeValidatorTest::testAcceptsMatchingPartialContent()
{
  auto result = validateHttpRangeResponse(
      206, Range(1024, 2047, 4096), Range(1024, 2047, 4096), 4096, "");

  CPPUNIT_ASSERT(result.ok);
}

void HttpRangeValidatorTest::testRejectsIgnoredRangeResponse()
{
  auto result = validateHttpRangeResponse(
      200, Range(0, 1023, 4096), Range(0, 4095, 4096), 4096, "");

  CPPUNIT_ASSERT(!result.ok);
  CPPUNIT_ASSERT(!result.retryable);
}

void HttpRangeValidatorTest::testRejectsMismatchedContentRange()
{
  auto result = validateHttpRangeResponse(
      206, Range(1024, 2047, 4096), Range(0, 1023, 4096), 4096, "");

  CPPUNIT_ASSERT(!result.ok);
  CPPUNIT_ASSERT(result.retryable);
}

void HttpRangeValidatorTest::testRejectsEncodedRangeResponse()
{
  auto result = validateHttpRangeResponse(
      206, Range(0, 1023, 4096), Range(0, 1023, 4096), 4096, "gzip");

  CPPUNIT_ASSERT(!result.ok);
}

} // namespace aria2
