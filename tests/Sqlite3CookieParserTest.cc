#include "Sqlite3CookieParserImpl.h"

#include <cstring>
#include <iostream>

#include "a2doctest.h"

#include "RecoverableException.h"
#include "util.h"
#include "array_fun.h"

namespace aria2 {

class Sqlite3CookieParserTest {


public:
  void setUp() {}

  void tearDown() {}

  void testMozParse();
  void testMozParse_readOnlyWalSnapshot();
  void testMozParse_fileNotFound();
  void testMozParse_badfile();
  void testChromumParse();
};

A2_TEST(Sqlite3CookieParserTest, testMozParse)
A2_TEST(Sqlite3CookieParserTest, testMozParse_readOnlyWalSnapshot)
A2_TEST(Sqlite3CookieParserTest, testMozParse_fileNotFound)
A2_TEST(Sqlite3CookieParserTest, testMozParse_badfile)
A2_TEST(Sqlite3CookieParserTest, testChromumParse)

void Sqlite3CookieParserTest::testMozParse()
{
  auto parser = Sqlite3MozCookieParser{A2_TEST_DIR "/cookies.sqlite"};
  auto cookies = parser.parse();
  REQUIRE_EQ((size_t)3, cookies.size());

  auto& localhost = cookies[0];
  REQUIRE_EQ(std::string("JSESSIONID"), localhost->getName());
  REQUIRE_EQ(std::string("123456789"), localhost->getValue());
  REQUIRE_EQ((time_t)INT32_MAX, localhost->getExpiryTime());
  REQUIRE(localhost->getPersistent());
  REQUIRE_EQ(std::string("localhost"), localhost->getDomain());
  REQUIRE(localhost->getHostOnly());
  REQUIRE_EQ(std::string("/"), localhost->getPath());
  REQUIRE(localhost->getSecure());
  REQUIRE_EQ((time_t)3000, localhost->getLastAccessTime());
  REQUIRE_EQ((time_t)3000, localhost->getCreationTime());

  auto& nullValue = cookies[1];
  REQUIRE_EQ(std::string("uid"), nullValue->getName());
  REQUIRE_EQ(std::string(""), nullValue->getValue());
  REQUIRE_EQ((time_t)0, nullValue->getExpiryTime());
  REQUIRE(nullValue->getPersistent());
  REQUIRE_EQ(std::string("null_value.com"), nullValue->getDomain());
  REQUIRE(!nullValue->getHostOnly());
  REQUIRE_EQ(std::string("/path/to"), nullValue->getPath());
  REQUIRE(!nullValue->getSecure());

  // See row id=3 has no name, so it is skipped.

  auto& overflowTime = cookies[2];
  REQUIRE_EQ(std::string("foo"), overflowTime->getName());
  REQUIRE_EQ(std::string("bar"), overflowTime->getValue());
  REQUIRE((time_t)INT32_MAX <= overflowTime->getExpiryTime());
  REQUIRE(overflowTime->getPersistent());
  REQUIRE_EQ(std::string("overflow.time_t.org"),
                       overflowTime->getDomain());
  REQUIRE(!overflowTime->getHostOnly());
  REQUIRE_EQ(std::string("/path/to"), overflowTime->getPath());
  REQUIRE(!overflowTime->getSecure());

  // See row id=5 has bad path, so it is skipped.
}

void Sqlite3CookieParserTest::testMozParse_readOnlyWalSnapshot()
{
  auto parser =
      Sqlite3MozCookieParser{A2_TEST_DIR "/cookies-readonly-wal.sqlite"};
  auto cookies = parser.parse();
  REQUIRE_EQ((size_t)1, cookies.size());

  auto& cookie = cookies[0];
  REQUIRE_EQ(std::string("sid"), cookie->getName());
  REQUIRE_EQ(std::string("value"), cookie->getValue());
  REQUIRE_EQ(std::string("example.org"), cookie->getDomain());
}

void Sqlite3CookieParserTest::testMozParse_fileNotFound()
{
  auto parser = Sqlite3MozCookieParser{"fileNotFound"};
  try {
    parser.parse();
    FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // SUCCESS
    const char A2_SQLITE_ERR[] = "SQLite3 database is not opened";
    REQUIRE(util::startsWith(e.what(), e.what() + strlen(e.what()),
                                    A2_SQLITE_ERR,
                                    std::end(A2_SQLITE_ERR) - 1));
  }
}

void Sqlite3CookieParserTest::testMozParse_badfile()
{
  auto parser = Sqlite3MozCookieParser{A2_TEST_DIR "/badcookies.sqlite"};
  try {
    parser.parse();
    FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // SUCCESS
  }
}

void Sqlite3CookieParserTest::testChromumParse()
{
  auto parser =
      Sqlite3ChromiumCookieParser{A2_TEST_DIR "/chromium_cookies.sqlite"};
  auto cookies = parser.parse();
  REQUIRE_EQ((size_t)3, cookies.size());

  auto& sfnet = cookies[0];
  REQUIRE_EQ(std::string("mykey"), sfnet->getName());
  REQUIRE_EQ(std::string("pass"), sfnet->getValue());
  REQUIRE_EQ((time_t)12345679, sfnet->getExpiryTime());
  REQUIRE(sfnet->getPersistent());
  REQUIRE_EQ(std::string("aria2.sourceforge.net"),
                       sfnet->getDomain());
  REQUIRE(!sfnet->getHostOnly());
  REQUIRE_EQ(std::string("/"), sfnet->getPath());
  REQUIRE(!sfnet->getSecure());

  auto& sfjp = cookies[1];
  REQUIRE_EQ(std::string("myseckey"), sfjp->getName());
  REQUIRE_EQ(std::string("pass2"), sfjp->getValue());
  REQUIRE_EQ((time_t)0, sfjp->getExpiryTime());
  REQUIRE(sfjp->getPersistent());
  REQUIRE_EQ(std::string("aria2.sourceforge.jp"), sfjp->getDomain());
  REQUIRE(sfjp->getHostOnly());
  REQUIRE_EQ(std::string("/profile"), sfjp->getPath());
  REQUIRE(sfjp->getSecure());

  auto& localnet = cookies[2];
  REQUIRE_EQ(std::string("192.168.0.1"), localnet->getDomain());
  REQUIRE(localnet->getHostOnly());
  REQUIRE_EQ((time_t)3000, localnet->getLastAccessTime());
  REQUIRE_EQ((time_t)3000, localnet->getCreationTime());
}

} // namespace aria2
