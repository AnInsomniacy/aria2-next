#include "CookieStorage.h"

#include <iostream>
#include <algorithm>
#include <limits>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "TimeA2.h"
#include "RecoverableException.h"
#include "File.h"
#include "TestUtil.h"
#include "TimerA2.h"

namespace aria2 {

class CookieStorageTest {


public:
  void setUp() {}

  void tearDown() {}

  void testStore();
  void testParseAndStore();
  void testCriteriaFind();
  void testCriteriaFind_cookieOrder();
  void testLoad();
  void testLoad_sqlite3();
  void testLoad_fileNotfound();
  void testSaveNsFormat();
  void testSaveNsFormat_fail();
  void testCookieIsFull();
  void testDomainIsFull();
  void testEviction();
};

A2_TEST(CookieStorageTest, testStore)
A2_TEST(CookieStorageTest, testParseAndStore)
A2_TEST(CookieStorageTest, testCriteriaFind)
A2_TEST(CookieStorageTest, testCriteriaFind_cookieOrder)
A2_TEST(CookieStorageTest, testLoad)
A2_TEST(CookieStorageTest, testLoad_sqlite3)
A2_TEST(CookieStorageTest, testLoad_fileNotfound)
A2_TEST(CookieStorageTest, testSaveNsFormat)
A2_TEST(CookieStorageTest, testSaveNsFormat_fail)
A2_TEST(CookieStorageTest, testCookieIsFull)
A2_TEST(CookieStorageTest, testDomainIsFull)
A2_TEST(CookieStorageTest, testEviction)

namespace {
std::vector<const Cookie*> dumpCookie(const CookieStorage& st)
{
  auto res = std::vector<const Cookie*>{};
  st.dumpCookie(std::back_inserter(res));
  std::sort(res.begin(), res.end(), CookieSorter());
  return res;
}
} // namespace

void CookieStorageTest::testStore()
{
  time_t now = 999;
  auto st = CookieStorage{};
  auto goodCookie = [&]() {
    auto c = createCookie("k", "v", "localhost", true, "/", false);
    c->setCreationTime(now);
    return c;
  };
  REQUIRE(st.store(goodCookie(), now));
  REQUIRE_EQ((size_t)1, st.size());
  REQUIRE(st.contains(*goodCookie()));

  auto anotherCookie = []() {
    return createCookie("k", "v", "mirror", true, "/", true);
  };
  REQUIRE(st.store(anotherCookie(), now));
  REQUIRE_EQ((size_t)2, st.size());
  REQUIRE(st.contains(*anotherCookie()));
  REQUIRE(st.contains(*goodCookie()));

  ++now;
  auto updateGoodCookie = [&]() {
    auto c = createCookie("k", "v2", "localhost", true, "/", false);
    c->setCreationTime(now);
    return c;
  };
  REQUIRE(st.store(updateGoodCookie(), now));
  REQUIRE_EQ((size_t)2, st.size());
  REQUIRE(st.contains(*updateGoodCookie()));
  REQUIRE(st.contains(*anotherCookie()));
  auto cookies = st.criteriaFind("localhost", "/", now, false);
  REQUIRE_EQ((size_t)1, cookies.size());
  REQUIRE_EQ(std::string("v2"), cookies[0]->getValue());
  // New cookie's creation time must match old cookie's creation time.
  REQUIRE_EQ((time_t)999, cookies[0]->getCreationTime());

  auto expireGoodCookie = []() {
    return createCookie("k", "v3", 0, "localhost", true, "/", false);
  };
  REQUIRE(!st.store(expireGoodCookie(), now));
  REQUIRE_EQ((size_t)1, st.size());
  REQUIRE(st.contains(*anotherCookie()));

  auto fromNumericHost = []() {
    return createCookie("k", "v", "192.168.1.1", true, "/", false);
  };
  REQUIRE(st.store(fromNumericHost(), now));
  REQUIRE_EQ((size_t)2, st.size());
  REQUIRE(st.contains(*fromNumericHost()));
}

void CookieStorageTest::testParseAndStore()
{
  auto st = CookieStorage{};
  time_t now = 1000;
  std::string localhostCookieStr =
      "k=v;"
      " expires=Fri, 01 Jan 2038 00:00:00 GMT; path=/; domain=localhost;";
  REQUIRE(
      st.parseAndStore(localhostCookieStr, "localhost", "/downloads", now));
  REQUIRE(
      !st.parseAndStore(localhostCookieStr, "mirror", "/downloads", now));
  REQUIRE(
      !st.parseAndStore(localhostCookieStr, "127.0.0.1", "/downloads", now));

  std::string numericHostCookieStr =
      "k=v;"
      " expires=Fri, 01 Jan 2038 00:00:00 GMT; path=/; domain=192.168.1.1;";
  REQUIRE(
      st.parseAndStore(numericHostCookieStr, "192.168.1.1", "/", now));

  // No domain and no path are specified.
  std::string noDomainPathCookieStr = "k=v";
  REQUIRE(st.parseAndStore(noDomainPathCookieStr, "aria2.sf.net",
                                  "/downloads", now));
}

void CookieStorageTest::testCriteriaFind()
{
  auto st = CookieStorage{};
  time_t now = 1000;

  auto alpha = []() {
    return createCookie("alpha", "ALPHA", "aria2.org", false, "/", false);
  };
  auto bravo = []() {
    return createCookie("bravo", "BRAVO", 1060, "aria2.org", false, "/foo",
                        false);
  };
  auto charlie = []() {
    return createCookie("charlie", "CHARLIE", "aria2.org", false, "/", true);
  };
  auto delta = []() {
    return createCookie("delta", "DELTA", "aria2.org", false, "/foo/bar",
                        false);
  };
  auto echo = []() {
    return createCookie("echo", "ECHO", "www.dl.aria2.org", false, "/", false);
  };
  auto foxtrot = []() {
    return createCookie("foxtrot", "FOXTROT", "sf.net", false, "/", false);
  };
  auto golf = []() {
    return createCookie("golf", "GOLF", "192.168.1.1", true, "/", false);
  };
  auto hotel1 = []() {
    return createCookie("hotel", "HOTEL1", "samename.x", false, "/", false);
  };
  auto hotel2 = []() {
    return createCookie("hotel", "HOTEL2", "samename.x", false, "/hotel",
                        false);
  };
  auto hotel3 = []() {
    return createCookie("hotel", "HOTEL3", "samename.x", false, "/bar/wine",
                        false);
  };
  auto hotel4 = []() {
    return createCookie("hotel", "HOTEL4", "samename.x", false, "/bar/", false);
  };
  auto india1 = []() {
    return createCookie("india", "INDIA1", "default.domain", true, "/foo",
                        false);
  };
  auto india2 = []() {
    return createCookie("india", "INDIA2", "default.domain", false, "/", false);
  };
  auto juliet1 = []() {
    return createCookie("juliet", "JULIET1", "localhost", true, "/foo", false);
  };

  REQUIRE(st.store(alpha(), now));
  REQUIRE(st.store(bravo(), now));
  REQUIRE(st.store(charlie(), now));
  REQUIRE(st.store(delta(), now));
  REQUIRE(st.store(echo(), now));
  REQUIRE(st.store(foxtrot(), now));
  REQUIRE(st.store(golf(), now));
  REQUIRE(st.store(hotel1(), now));
  REQUIRE(st.store(hotel2(), now));
  REQUIRE(st.store(hotel3(), now));
  REQUIRE(st.store(hotel4(), now));
  REQUIRE(st.store(india1(), now));
  REQUIRE(st.store(india2(), now));
  REQUIRE(st.store(juliet1(), now));

  auto aria2Slash = st.criteriaFind("www.dl.aria2.org", "/", 0, false);
  REQUIRE_EQ((size_t)2, aria2Slash.size());
  REQUIRE(derefFind(aria2Slash, alpha()));
  REQUIRE(derefFind(aria2Slash, echo()));

  auto aria2SlashFoo = st.criteriaFind("www.dl.aria2.org", "/foo", 0, false);
  REQUIRE_EQ((size_t)3, aria2SlashFoo.size());
  REQUIRE_EQ(std::string("bravo"), aria2SlashFoo[0]->getName());
  REQUIRE(derefFind(aria2SlashFoo, alpha()));
  REQUIRE(derefFind(aria2SlashFoo, echo()));

  auto aria2Expires = st.criteriaFind("www.dl.aria2.org", "/foo", 1120, false);
  REQUIRE_EQ((size_t)2, aria2Expires.size());
  REQUIRE(derefFind(aria2Expires, alpha()));
  REQUIRE(derefFind(aria2Expires, echo()));

  auto dlAria2 = st.criteriaFind("dl.aria2.org", "/", 0, false);
  REQUIRE_EQ((size_t)1, dlAria2.size());
  REQUIRE_EQ(std::string("alpha"), dlAria2[0]->getName());

  auto tailmatchAria2 = st.criteriaFind("myaria2.org", "/", 0, false);
  REQUIRE(tailmatchAria2.empty());

  auto numericHostCookies = st.criteriaFind("192.168.1.1", "/", 0, false);
  REQUIRE_EQ((size_t)1, numericHostCookies.size());
  REQUIRE_EQ(std::string("golf"), numericHostCookies[0]->getName());

  auto sameNameCookies = st.criteriaFind("samename.x", "/bar/wine", 0, false);
  REQUIRE_EQ((size_t)3, sameNameCookies.size());
  REQUIRE_EQ(std::string("HOTEL3"), sameNameCookies[0]->getValue());
  REQUIRE_EQ(std::string("HOTEL4"), sameNameCookies[1]->getValue());
  REQUIRE_EQ(std::string("HOTEL1"), sameNameCookies[2]->getValue());

  auto defaultDomainCookies =
      st.criteriaFind("default.domain", "/foo", 0, false);
  REQUIRE_EQ((size_t)2, defaultDomainCookies.size());
  REQUIRE_EQ(std::string("INDIA1"),
                       defaultDomainCookies[0]->getValue());
  REQUIRE_EQ(std::string("INDIA2"),
                       defaultDomainCookies[1]->getValue());
  defaultDomainCookies =
      st.criteriaFind("sub.default.domain", "/foo", 0, false);
  REQUIRE_EQ((size_t)1, defaultDomainCookies.size());
  REQUIRE_EQ(std::string("INDIA2"),
                       defaultDomainCookies[0]->getValue());

  // localhost.local case
  auto localDomainCookies = st.criteriaFind("localhost", "/foo", 0, false);
  REQUIRE_EQ((size_t)1, localDomainCookies.size());
  REQUIRE_EQ(std::string("JULIET1"),
                       localDomainCookies[0]->getValue());
}

void CookieStorageTest::testCriteriaFind_cookieOrder()
{
  auto st = CookieStorage{};
  auto a = createCookie("a", "0", "host", true, "/", false);
  a->setCreationTime(1000);
  auto b = createCookie("b", "0", "host", true, "/foo", false);
  b->setCreationTime(5000);
  auto c = createCookie("c", "0", "host", true, "/foo", false);
  c->setCreationTime(4000);
  auto d = createCookie("d", "0", "host", true, "/foo/bar", false);
  d->setCreationTime(6000);

  st.store(std::move(a), 0);
  st.store(std::move(b), 0);
  st.store(std::move(c), 0);
  st.store(std::move(d), 0);

  auto cookies = st.criteriaFind("host", "/foo/bar", 0, false);
  REQUIRE_EQ((size_t)4, cookies.size());
  REQUIRE_EQ(std::string("d"), cookies[0]->getName());
  REQUIRE_EQ(std::string("c"), cookies[1]->getName());
  REQUIRE_EQ(std::string("b"), cookies[2]->getName());
  REQUIRE_EQ(std::string("a"), cookies[3]->getName());
}

void CookieStorageTest::testLoad()
{
  auto st = CookieStorage{};

  st.load(A2_TEST_DIR "/nscookietest.txt", 1001);

  REQUIRE_EQ((size_t)4, st.size());

  auto cookies = dumpCookie(st);

  auto c = cookies[0];
  REQUIRE_EQ(std::string("passwd"), c->getName());
  REQUIRE_EQ(std::string("secret"), c->getValue());
  REQUIRE_EQ(std::numeric_limits<time_t>::max(), c->getExpiryTime());
  REQUIRE(!c->getPersistent());
  REQUIRE_EQ(std::string("/cgi-bin"), c->getPath());
  REQUIRE_EQ(std::string("192.168.0.1"), c->getDomain());
  REQUIRE(c->getHostOnly());
  REQUIRE(!c->getSecure());

  c = cookies[1];
  REQUIRE_EQ(std::string("novalue"), c->getName());
  REQUIRE_EQ(std::string(""), c->getValue());
  REQUIRE_EQ((time_t)2147483647, c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE_EQ(std::string("example.org"), c->getDomain());
  REQUIRE(!c->getHostOnly());
  REQUIRE(!c->getSecure());

  c = cookies[2];
  REQUIRE_EQ(std::string("JSESSIONID"), c->getName());
  REQUIRE_EQ(std::string("123456789"), c->getValue());
  REQUIRE_EQ((time_t)2147483647, c->getExpiryTime());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE_EQ(std::string("localhost"), c->getDomain());
  REQUIRE(c->getHostOnly());
  REQUIRE(c->getSecure());

  c = cookies[3];
  REQUIRE_EQ(std::string("TAX"), c->getName());
  REQUIRE_EQ(std::string("1000"), c->getValue());
  REQUIRE_EQ((time_t)1463304912, c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE_EQ(std::string("something"), c->getDomain());
  REQUIRE(!c->getSecure());
}

void CookieStorageTest::testLoad_sqlite3()
{
  auto st = CookieStorage{};
#ifdef HAVE_SQLITE3
  st.load(A2_TEST_DIR "/cookies.sqlite", 1000);
  REQUIRE_EQ((size_t)2, st.size());
  auto cookies = dumpCookie(st);
  auto c = cookies[0];
  REQUIRE_EQ(std::string("JSESSIONID"), c->getName());
  REQUIRE_EQ(std::string("123456789"), c->getValue());
  REQUIRE_EQ((time_t)INT32_MAX, c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("localhost"), c->getDomain());
  REQUIRE(c->getHostOnly());
  REQUIRE_EQ(std::string("/"), c->getPath());
  REQUIRE(c->getSecure());

  c = cookies[1];
  REQUIRE_EQ(std::string("foo"), c->getName());
  REQUIRE_EQ(std::string("bar"), c->getValue());
  REQUIRE((time_t)INT32_MAX <= c->getExpiryTime());
  REQUIRE(c->getPersistent());
  REQUIRE_EQ(std::string("overflow.time_t.org"), c->getDomain());
  REQUIRE(!c->getHostOnly());
  REQUIRE_EQ(std::string("/path/to"), c->getPath());
  REQUIRE(!c->getSecure());

#else  // !HAVE_SQLITE3
  REQUIRE(!st.load(A2_TEST_DIR "/cookies.sqlite", 1000));
#endif // !HAVE_SQLITE3
}

void CookieStorageTest::testLoad_fileNotfound()
{
  auto st = CookieStorage{};
  REQUIRE(
      !st.load("./aria2_CookieStorageTest_testLoad_fileNotfound", 0));
}

void CookieStorageTest::testSaveNsFormat()
{
  // TODO add cookie with default domain
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_CookieStorageTest_testSaveNsFormat";
  File(filename).remove();
  auto st = CookieStorage{};
  time_t now = 1000;
  st.store(
      createCookie("favorite", "classic", "domain.org", false, "/config", true),
      now);
  st.store(createCookie("uid", "tujikawa", now, "domain.org", true, "/", false),
           now);
  REQUIRE(st.saveNsFormat(filename));
  auto loadst = CookieStorage{};
  loadst.load(filename, now);
  REQUIRE_EQ((size_t)2, loadst.size());

  auto cookies = dumpCookie(loadst);

  REQUIRE_EQ((size_t)2, cookies.size());
  REQUIRE_EQ(std::string("favorite"), cookies[0]->getName());
  REQUIRE_EQ(std::string("uid"), cookies[1]->getName());
}

void CookieStorageTest::testSaveNsFormat_fail()
{
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_CookieStorageTest_testSaveNsFormat_fail";
  File f(filename);
  f.remove();
  f.mkdirs();
  REQUIRE(f.isDir());
  auto st = CookieStorage{};
  REQUIRE(!st.saveNsFormat(filename));
}

void CookieStorageTest::testCookieIsFull()
{
  auto st = CookieStorage{};
  for (size_t i = 0; i < CookieStorage::MAX_COOKIE_PER_DOMAIN + 1; ++i) {
    st.store(
        createCookie("k" + util::itos(i), "v", "aria2.org", false, "/", false),
        0);
  }
  REQUIRE_EQ((size_t)CookieStorage::MAX_COOKIE_PER_DOMAIN, st.size());
}

void CookieStorageTest::testDomainIsFull()
{
  // See DOMAIN_EVICTION_TRIGGER and DOMAIN_EVICTION_RATE in
  // CookieStorage.cc
  auto st = CookieStorage{};
  for (size_t i = 0; i < 2001; ++i) {
    st.store(createCookie("k", "v", "domain" + util::itos(i), true, "/", false),
             0);
  }
  REQUIRE_EQ((size_t)1801, st.size());
}

void CookieStorageTest::testEviction()
{
  auto st = CookieStorage{};
  auto alpha = []() {
    return createCookie("a", "alpha", "aria2.sf.net", false, "/", false);
  };
  auto bravo = []() {
    return createCookie("b", "bravo", "d.aria2.sf.net", false, "/", false);
  };
  auto charlie = []() {
    return createCookie("c", "charlie", "a2.github.com", false, "/", false);
  };
  auto delta = []() {
    return createCookie("d", "delta", "aria2.sf.net", false, "/", false);
  };
  st.store(alpha(), 0);
  REQUIRE_EQ((size_t)1, st.getLruTrackerSize());
  st.store(bravo(), 1);
  REQUIRE_EQ((size_t)2, st.getLruTrackerSize());
  st.store(charlie(), 2);
  REQUIRE_EQ((size_t)3, st.getLruTrackerSize());
  st.store(delta(), 0);
  REQUIRE_EQ((size_t)3, st.getLruTrackerSize());

  // aria2.sf.net will be evicted
  st.evictNode(1);
  REQUIRE_EQ((size_t)2, st.getLruTrackerSize());
  REQUIRE(!st.contains(*alpha()));
  REQUIRE(st.contains(*bravo()));
  REQUIRE(st.contains(*charlie()));
  REQUIRE(!st.contains(*delta()));

  // d.aria2.sf.net will be evicted
  st.evictNode(1);
  REQUIRE_EQ((size_t)1, st.getLruTrackerSize());
  REQUIRE(!st.contains(*bravo()));
  REQUIRE(st.contains(*charlie()));

  // a2.github.com will be evicted
  st.evictNode(1);
  REQUIRE_EQ((size_t)0, st.getLruTrackerSize());
  REQUIRE(!st.contains(*charlie()));
  REQUIRE_EQ((size_t)0, st.size());
  REQUIRE(!st.getRootNode()->hasNext());
}

} // namespace aria2
