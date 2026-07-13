#include "NsCookieParser.h"

#include <iostream>
#include <limits>

#include "a2doctest.h"

#include "RecoverableException.h"
#include "util.h"
#include "Cookie.h"

namespace aria2 {

class NsCookieParserTest {


public:
  void setUp() {}

  void tearDown() {}

  void testParse();
  void testParse_fileNotFound();
};

A2_TEST(NsCookieParserTest, testParse)
A2_TEST(NsCookieParserTest, testParse_fileNotFound)

void NsCookieParserTest::testParse()
{
  NsCookieParser parser;
  time_t now = 0;
  auto cookies = parser.parse(A2_TEST_DIR "/nscookietest.txt", now);
  REQUIRE_EQ((size_t)5, cookies.size());

  auto c = cookies[0].get();
  REQUIRE_EQ(std::string("JSESSIONID"), c->getName());
  REQUIRE_EQ(std::string("123456789"), c->getValue());
  REQUIRE_EQ((time_t)INT32_MAX, c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("localhost"), c->getDomain());
  REQUIRE(c->getHostOnly());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE(c->getSecure());

  c = cookies[1].get();
  REQUIRE_EQ(std::string("user"), c->getName());
  REQUIRE_EQ(std::string("me"), c->getValue());
  REQUIRE_EQ((time_t)1000, c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("expired"), c->getDomain());
  REQUIRE(c->getHostOnly());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE(!c->getSecure());

  c = cookies[2].get();
  REQUIRE_EQ(std::string("passwd"), c->getName());
  REQUIRE_EQ(std::string("secret"), c->getValue());
  REQUIRE_EQ(std::numeric_limits<time_t>::max(), c->getExpiryTime());
  REQUIRE(!c->getPersistent());
  REQUIRE_EQ(std::string("192.168.0.1"), c->getDomain());
  REQUIRE(c->getHostOnly());
  REQUIRE_EQ(std::string("/cgi-bin"), c->getPath());
  REQUIRE(!c->getSecure());

  c = cookies[3].get();
  REQUIRE_EQ(std::string("TAX"), c->getName());
  REQUIRE_EQ(std::string("1000"), c->getValue());
  REQUIRE_EQ((time_t)1463304912, c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("something"), c->getDomain());
  REQUIRE(c->getHostOnly());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE(!c->getSecure());

  c = cookies[4].get();
  REQUIRE_EQ(std::string("novalue"), c->getName());
  REQUIRE_EQ(std::string(""), c->getValue());
  REQUIRE_EQ((time_t)INT32_MAX, c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("example.org"), c->getDomain());
  REQUIRE(!c->getHostOnly());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE(!c->getSecure());
}

void NsCookieParserTest::testParse_fileNotFound()
{
  NsCookieParser parser;
  try {
    time_t now = 0;
    parser.parse("fileNotFound", now);
    FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // SUCCESS
  }
}

} // namespace aria2
