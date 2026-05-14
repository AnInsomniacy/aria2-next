#include "AsyncNameResolverMan.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Option.h"
#include "prefs.h"

namespace aria2 {

class AsyncNameResolverManTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(AsyncNameResolverManTest);
  CPPUNIT_TEST(testConfigureDefaults);
  CPPUNIT_TEST(testConfigureWithExplicitServers);
  CPPUNIT_TEST(testIPv4SuccessCompletesWithoutWaitingForIPv6);
  CPPUNIT_TEST(testIPv6OnlySuccessCompletesAfterIPv4Error);
  CPPUNIT_TEST(testIPv6SuccessWaitsWhileIPv4IsPending);
  CPPUNIT_TEST(testAllResolversFailed);
  CPPUNIT_TEST(testNoResolversKeepsHistoricalFailureStatus);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testConfigureDefaults();
  void testConfigureWithExplicitServers();
  void testIPv4SuccessCompletesWithoutWaitingForIPv6();
  void testIPv6OnlySuccessCompletesAfterIPv4Error();
  void testIPv6SuccessWaitsWhileIPv4IsPending();
  void testAllResolversFailed();
  void testNoResolversKeepsHistoricalFailureStatus();
};

CPPUNIT_TEST_SUITE_REGISTRATION(AsyncNameResolverManTest);

void AsyncNameResolverManTest::testConfigureDefaults()
{
  AsyncNameResolverMan man;
  Option option;

  configureAsyncNameResolverMan(&man, &option);

  CPPUNIT_ASSERT(!man.started());
}

void AsyncNameResolverManTest::testConfigureWithExplicitServers()
{
  AsyncNameResolverMan man;
  Option option;
  option.put(PREF_ASYNC_DNS_SERVER, "8.8.8.8,8.8.4.4");

  configureAsyncNameResolverMan(&man, &option);

  CPPUNIT_ASSERT(!man.started());
}

void AsyncNameResolverManTest::testIPv4SuccessCompletesWithoutWaitingForIPv6()
{
  CPPUNIT_ASSERT_EQUAL(1, evaluateAsyncNameResolverStatus(2, 1, 0, true));
}

void AsyncNameResolverManTest::testIPv6OnlySuccessCompletesAfterIPv4Error()
{
  CPPUNIT_ASSERT_EQUAL(1, evaluateAsyncNameResolverStatus(2, 1, 1, false));
}

void AsyncNameResolverManTest::testIPv6SuccessWaitsWhileIPv4IsPending()
{
  CPPUNIT_ASSERT_EQUAL(0, evaluateAsyncNameResolverStatus(2, 1, 0, false));
}

void AsyncNameResolverManTest::testAllResolversFailed()
{
  CPPUNIT_ASSERT_EQUAL(-1, evaluateAsyncNameResolverStatus(2, 0, 2, false));
}

void AsyncNameResolverManTest::testNoResolversKeepsHistoricalFailureStatus()
{
  CPPUNIT_ASSERT_EQUAL(-1, evaluateAsyncNameResolverStatus(0, 0, 0, false));
}

} // namespace aria2
