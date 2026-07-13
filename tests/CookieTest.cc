#include "Cookie.h"

#include <iostream>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "TimeA2.h"
#include "TestUtil.h"

namespace aria2 {

class CookieTest {


public:
  void setUp() {}

  void tearDown() {}

  void testOperatorEqual();
  void testMatch();
  void testIsExpired();
  void testToNsCookieFormat();
};

A2_TEST(CookieTest, testOperatorEqual)
A2_TEST(CookieTest, testMatch)
A2_TEST(CookieTest, testIsExpired)
A2_TEST(CookieTest, testToNsCookieFormat)

void CookieTest::testOperatorEqual()
{
  auto a = createCookie("k", "v", "localhost", true, "/", false);
  auto b = createCookie("k", "v", "localhost", true, "/", true);
  auto wrongPath = createCookie("k", "v", "localhost", true, "/a", false);
  auto wrongDomain = createCookie("k", "v", "mydomain", true, "/", false);
  auto wrongName = createCookie("h", "v", "localhost", true, "/a", false);
  auto caseSensitiveName =
      createCookie("K", "v", "localhost", true, "/a", false);
  REQUIRE(*a == *b);
  REQUIRE(!(*a == *wrongPath));
  REQUIRE(!(*a == *wrongDomain));
  REQUIRE(!(*a == *wrongName));
  REQUIRE(!(*a == *caseSensitiveName));
}

void CookieTest::testMatch()
{
  auto c = createCookie("k", "v", "aria2.org", false, "/downloads", false);
  auto c2 = createCookie("k", "v", "aria2.org", false, "/downloads", false);
  auto c3 = createCookie("k", "v", "aria2.org", true, "/downloads", false);
  auto c4 = createCookie("k", "v", "localhost", true, "/downloads", false);
  REQUIRE(c->match("www.aria2.org", "/downloads", 0, false));
  REQUIRE(c2->match("www.aria2.org", "/downloads", 0, false));
  REQUIRE(!c->match("www.aria.org", "/downloads", 0, false));
  REQUIRE(!c->match("www.aria2.org", "/examples", 0, false));
  REQUIRE(c->match("www.aria2.org", "/downloads", 0, true));
  REQUIRE(c->match("www.aria2.org", "/downloads/latest", 0, false));
  REQUIRE(!c->match("www.aria2.org", "/downloadss/latest", 0, false));
  REQUIRE(!c->match("www.aria2.org", "/DOWNLOADS", 0, false));
  REQUIRE(!c3->match("www.aria2.org", "/downloads", 0, false));
  REQUIRE(c4->match("localhost", "/downloads", 0, false));

  auto secureCookie =
      createCookie("k", "v", "secure.aria2.org", false, "/", true);
  REQUIRE(secureCookie->match("secure.aria2.org", "/", 0, true));
  REQUIRE(!secureCookie->match("secure.aria2.org", "/", 0, false));
  REQUIRE(!secureCookie->match("ssecure.aria2.org", "/", 0, true));
  REQUIRE(secureCookie->match("www.secure.aria2.org", "/", 0, true));

  auto expireTest =
      createCookie("k", "v", 1000, "aria2.org", false, "/", false);
  REQUIRE(expireTest->match("www.aria2.org", "/", 999, false));
  REQUIRE(expireTest->match("www.aria2.org", "/", 1000, false));
  REQUIRE(!expireTest->match("www.aria2.org", "/", 1001, false));

  auto fromNumericHost =
      createCookie("k", "v", "192.168.1.1", true, "/foo", false);
  REQUIRE(fromNumericHost->match("192.168.1.1", "/foo", 0, false));
  REQUIRE(!fromNumericHost->match("www.aria2.org", "/foo", 0, false));
  REQUIRE(!fromNumericHost->match("1.192.168.1.1", "/foo", 0, false));
  REQUIRE(!fromNumericHost->match("192.168.1.1", "/", 0, false));
}

void CookieTest::testIsExpired()
{
  auto cookie = createCookie("k", "v", 1000, "localhost", true, "/", false);
  REQUIRE(cookie->isExpired(1001));
  REQUIRE(!cookie->isExpired(1000));
  REQUIRE(!cookie->isExpired(999));
  auto sessionCookie = createCookie("k", "v", "localhost", true, "/", false);
  REQUIRE(!sessionCookie->isExpired(INT32_MAX));
}

void CookieTest::testToNsCookieFormat()
{
  REQUIRE_EQ(
      std::string(".domain.org\tTRUE\t/\tFALSE\t12345678\thello\tworld"),
      createCookie("hello", "world", 12345678, "domain.org", false, "/", false)
          ->toNsCookieFormat());
  // Session cookie
  REQUIRE_EQ(
      std::string("domain.org\tFALSE\t/\tTRUE\t0\thello\tworld"),
      createCookie("hello", "world", "domain.org", true, "/", true)
          ->toNsCookieFormat());
}

} // namespace aria2
