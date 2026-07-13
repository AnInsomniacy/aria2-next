#include "AsyncNameResolverMan.h"

#include "a2doctest.h"

#include "Option.h"
#include "prefs.h"

namespace aria2 {

class AsyncNameResolverManTest {


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
  void testFallbackAllowedForResolverInfrastructureErrors();
  void testFallbackRejectedForAuthoritativeResolverErrors();
  void testFallbackRejectedForExplicitAsyncDnsServers();
};

A2_TEST(AsyncNameResolverManTest, testConfigureDefaults)
A2_TEST(AsyncNameResolverManTest, testConfigureWithExplicitServers)
A2_TEST(AsyncNameResolverManTest, testIPv4SuccessCompletesWithoutWaitingForIPv6)
A2_TEST(AsyncNameResolverManTest, testIPv6OnlySuccessCompletesAfterIPv4Error)
A2_TEST(AsyncNameResolverManTest, testIPv6SuccessWaitsWhileIPv4IsPending)
A2_TEST(AsyncNameResolverManTest, testAllResolversFailed)
A2_TEST(AsyncNameResolverManTest, testNoResolversKeepsHistoricalFailureStatus)
A2_TEST(AsyncNameResolverManTest, testFallbackAllowedForResolverInfrastructureErrors)
A2_TEST(AsyncNameResolverManTest, testFallbackRejectedForAuthoritativeResolverErrors)
A2_TEST(AsyncNameResolverManTest, testFallbackRejectedForExplicitAsyncDnsServers)

void AsyncNameResolverManTest::testConfigureDefaults()
{
  AsyncNameResolverMan man;
  Option option;

  configureAsyncNameResolverMan(&man, &option);

  REQUIRE(!man.started());
}

void AsyncNameResolverManTest::testConfigureWithExplicitServers()
{
  AsyncNameResolverMan man;
  Option option;
  option.put(PREF_ASYNC_DNS_SERVER, "8.8.8.8,8.8.4.4");

  configureAsyncNameResolverMan(&man, &option);

  REQUIRE(!man.started());
}

void AsyncNameResolverManTest::testIPv4SuccessCompletesWithoutWaitingForIPv6()
{
  REQUIRE_EQ(1, evaluateAsyncNameResolverStatus(2, 1, 0, true));
}

void AsyncNameResolverManTest::testIPv6OnlySuccessCompletesAfterIPv4Error()
{
  REQUIRE_EQ(1, evaluateAsyncNameResolverStatus(2, 1, 1, false));
}

void AsyncNameResolverManTest::testIPv6SuccessWaitsWhileIPv4IsPending()
{
  REQUIRE_EQ(0, evaluateAsyncNameResolverStatus(2, 1, 0, false));
}

void AsyncNameResolverManTest::testAllResolversFailed()
{
  REQUIRE_EQ(-1, evaluateAsyncNameResolverStatus(2, 0, 2, false));
}

void AsyncNameResolverManTest::testNoResolversKeepsHistoricalFailureStatus()
{
  REQUIRE_EQ(-1, evaluateAsyncNameResolverStatus(0, 0, 0, false));
}

void AsyncNameResolverManTest::testFallbackAllowedForResolverInfrastructureErrors()
{
  REQUIRE(shouldFallbackToSystemResolver(ARES_ENOSERVER, false));
  REQUIRE(shouldFallbackToSystemResolver(ARES_ECONNREFUSED, false));
  REQUIRE(shouldFallbackToSystemResolver(ARES_ETIMEOUT, false));
  REQUIRE(shouldFallbackToSystemResolver(ARES_EFILE, false));
  REQUIRE(shouldFallbackToSystemResolver(ARES_ELOADIPHLPAPI, false));
  REQUIRE(shouldFallbackToSystemResolver(ARES_EADDRGETNETWORKPARAMS,
                                                false));
}

void AsyncNameResolverManTest::testFallbackRejectedForAuthoritativeResolverErrors()
{
  REQUIRE(!shouldFallbackToSystemResolver(ARES_ENOTFOUND, false));
  REQUIRE(!shouldFallbackToSystemResolver(ARES_ENONAME, false));
  REQUIRE(!shouldFallbackToSystemResolver(ARES_ENODATA, false));
  REQUIRE(!shouldFallbackToSystemResolver(ARES_EREFUSED, false));
}

void AsyncNameResolverManTest::testFallbackRejectedForExplicitAsyncDnsServers()
{
  REQUIRE(!shouldFallbackToSystemResolver(ARES_ENOSERVER, true));
  REQUIRE(!shouldFallbackToSystemResolver(ARES_ECONNREFUSED, true));
  REQUIRE(!shouldFallbackToSystemResolver(ARES_ETIMEOUT, true));
}

} // namespace aria2
