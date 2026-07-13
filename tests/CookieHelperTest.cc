#include "cookie_helper.h"

#include <limits>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "Cookie.h"

namespace aria2 {

class CookieHelperTest {


public:
  void testParseDate();
  void testDomainMatch();
  void testPathMatch();
  void testParse();
  void testReverseDomainLevel();
};

A2_TEST(CookieHelperTest, testParseDate)
A2_TEST(CookieHelperTest, testDomainMatch)
A2_TEST(CookieHelperTest, testPathMatch)
A2_TEST(CookieHelperTest, testParse)
A2_TEST(CookieHelperTest, testReverseDomainLevel)

void CookieHelperTest::testParseDate()
{
  // RFC1123
  time_t time = 0;
  std::string s = "Sat, 06 Sep 2008 15:26:33 GMT";
  REQUIRE(cookie::parseDate(time, s.begin(), s.end()));
  REQUIRE_EQ((time_t)1220714793, time);
  // RFC850
  s = "Saturday, 06-Sep-08 15:26:33 GMT";
  REQUIRE(cookie::parseDate(time, s.begin(), s.end()));
  REQUIRE_EQ((time_t)1220714793, time);
  // ANSI C's asctime()
  s = "Sun Sep  6 15:26:33 2008";
  REQUIRE(cookie::parseDate(time, s.begin(), s.end()));
  REQUIRE_EQ((time_t)1220714793, time);
  s = "Thu Jan 1 0:0:0 1970";
  REQUIRE(cookie::parseDate(time, s.begin(), s.end()));
  REQUIRE_EQ((time_t)0, time);

  s = "Thu Jan 1 1970 0:";
  REQUIRE(!cookie::parseDate(time, s.begin(), s.end()));
  s = "Thu Jan 1 1970 0:0";
  REQUIRE(!cookie::parseDate(time, s.begin(), s.end()));
  s = "Thu Jan 1 1970 0:0:";
  REQUIRE(!cookie::parseDate(time, s.begin(), s.end()));

  // Leap year
  s = "Tue, 29 Feb 2000 00:00:00 GMT";
  REQUIRE(cookie::parseDate(time, s.begin(), s.end()));
  s = "Thu, 29 Feb 2001 00:00:00 GMT";
  REQUIRE(!cookie::parseDate(time, s.begin(), s.end()));
}

void CookieHelperTest::testDomainMatch()
{
  REQUIRE(cookie::domainMatch("localhost", "localhost"));
  REQUIRE(cookie::domainMatch("192.168.0.1", "192.168.0.1"));
  REQUIRE(cookie::domainMatch("www.example.org", "example.org"));
  REQUIRE(!cookie::domainMatch("192.168.0.1", "0.1"));
  REQUIRE(!cookie::domainMatch("example.org", "example.com"));
  REQUIRE(!cookie::domainMatch("example.org", "www.example.org"));
}

void CookieHelperTest::testPathMatch()
{
  REQUIRE(cookie::pathMatch("/", "/"));
  REQUIRE(cookie::pathMatch("/foo/", "/foo"));
  REQUIRE(!cookie::pathMatch("/bar/", "/foo"));
  REQUIRE(!cookie::pathMatch("/foo", "/bar/foo"));
  REQUIRE(cookie::pathMatch("/foo/bar", "/foo/"));
}

void CookieHelperTest::testParse()
{
  time_t creationDate = 141;
  {
    std::string str = "ID=123456789; expires=Sun, 10-Jun-2007 11:00:00 GMT;"
                      "path=/foo; domain=localhost; secure;httpOnly   ";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("ID"), c->getName());
    REQUIRE_EQ(std::string("123456789"), c->getValue());
    REQUIRE_EQ((time_t)1181473200, c->getExpiryTime());
    REQUIRE(c->getPersistent());
    REQUIRE_EQ(std::string("localhost"), c->getDomain());
    REQUIRE(!c->getHostOnly());
    REQUIRE_EQ(std::string("/foo"), c->getPath());
    REQUIRE(c->getSecure());
    REQUIRE(c->getHttpOnly());
    REQUIRE_EQ((time_t)141, c->getCreationTime());
    REQUIRE_EQ((time_t)141, c->getLastAccessTime());
  }
  {
    std::string str = "id=; Max-Age=0;";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("id"), c->getName());
    REQUIRE_EQ((time_t)0, c->getExpiryTime());
    REQUIRE(c->getPersistent());
    REQUIRE_EQ(std::string("localhost"), c->getDomain());
    REQUIRE(c->getHostOnly());
    REQUIRE_EQ(std::string("/"), c->getPath());
    REQUIRE(!c->getSecure());
    REQUIRE(!c->getHttpOnly());
  }
  {
    std::string str = "id=; Max-Age=-100;";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ((time_t)0, c->getExpiryTime());
    REQUIRE(c->getPersistent());
  }
  {
    std::string str = "id=; Max-Age=100;";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ((time_t)creationDate + 100, c->getExpiryTime());
    REQUIRE(c->getPersistent());
  }
  {
    std::string str = "id=; Max-Age=9223372036854775807;";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::numeric_limits<time_t>::max(),
                         c->getExpiryTime());
    REQUIRE(c->getPersistent());
  }
  {
    std::string str = "id=; Max-Age=X;";
    REQUIRE(!cookie::parse(str, "localhost", "/", creationDate));
  }
  {
    std::string str = "id=; Max-Age=100garbage;";
    REQUIRE(!cookie::parse(str, "localhost", "/", creationDate));
  }
  {
    std::string str = "id=; Max-Age=100;expires=Sun, 10-Jun-2007 11:00:00 GMT;";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ((time_t)creationDate + 100, c->getExpiryTime());
    REQUIRE(c->getPersistent());
  }
  {
    // Cookie data cannot be parsed.
    std::string str = "id=; expires=2007-10-01 11:00:00 GMT;";
    REQUIRE(!cookie::parse(str, "localhost", "/", creationDate));
  }
  {
    std::string str = "id=;";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE(!c->getPersistent());
  }
  {
    std::string str = "id=; path=abc";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("/"), c->getPath());
  }
  {
    std::string str = "id=; domain=.example.org";
    REQUIRE(cookie::parse(str, "www.example.org", "/", creationDate));
  }
  {
    // Fails because request host does not domain-match with cookie
    // domain.
    std::string str = "id=; domain=www.example.org";
    REQUIRE(!cookie::parse(str, "example.org", "/", creationDate));
  }
  {
    std::string str = "id=; domain=.";
    REQUIRE(!cookie::parse(str, "localhost", "/", creationDate));
  }
  {
    std::string str = "";
    REQUIRE(!cookie::parse(str, "localhost", "/", creationDate));
  }
  {
    std::string str = "=";
    REQUIRE(!cookie::parse(str, "localhost", "/", creationDate));
  }
  {
    // Use domain last time seen.
    std::string str = "id=;domain=a.example.org;domain=.example.org";
    auto c = cookie::parse(str, "b.example.org", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("example.org"), c->getDomain());
  }
  {
    // numeric host
    std::string str = "id=;";
    auto c = cookie::parse(str, "192.168.0.1", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("192.168.0.1"), c->getDomain());
    REQUIRE(c->getHostOnly());
  }
  {
    // numeric host
    std::string str = "id=; domain=192.168.0.1";
    auto c = cookie::parse(str, "192.168.0.1", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("192.168.0.1"), c->getDomain());
    REQUIRE(c->getHostOnly());
  }
  {
    // DQUOTE around cookie-value
    std::string str = "id=\"foo\";";
    auto c = cookie::parse(str, "localhost", "/", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("foo"), c->getValue());
  }
  {
    // Default path
    std::string str = "id=;";
    auto c = cookie::parse(str, "localhost", "/foo", creationDate);
    REQUIRE(c);
    REQUIRE_EQ(std::string("/foo"), c->getPath());
  }
}

void CookieHelperTest::testReverseDomainLevel()
{
  REQUIRE_EQ(std::string("net.sourceforge.aria2"),
                       cookie::reverseDomainLevel("aria2.sourceforge.net"));
  REQUIRE_EQ(std::string("localhost"),
                       cookie::reverseDomainLevel("localhost"));
  // Behavior check
  REQUIRE_EQ(std::string(""), cookie::reverseDomainLevel(""));
  REQUIRE_EQ(std::string(""), cookie::reverseDomainLevel("."));
  REQUIRE_EQ(std::string("foo."), cookie::reverseDomainLevel(".foo"));
  REQUIRE_EQ(std::string("foo"), cookie::reverseDomainLevel("foo."));
}

} // namespace aria2
