#include "DNSCache.h"

#include "a2doctest.h"

namespace aria2 {

class DNSCacheTest {


  DNSCache cache_;

public:
  void setUp()
  {
    cache_ = DNSCache();
    cache_.put("www", "192.168.0.1", 80);
    cache_.put("www", "::1", 80);
    cache_.put("ftp", "192.168.0.1", 21);
    cache_.put("proxy", "192.168.1.2", 8080);
  }

  void testFind();
  void testMarkBad();
  void testPutBadAddr();
  void testRemove();
};

A2_TEST(DNSCacheTest, testFind)
A2_TEST(DNSCacheTest, testMarkBad)
A2_TEST(DNSCacheTest, testPutBadAddr)
A2_TEST(DNSCacheTest, testRemove)

void DNSCacheTest::testFind()
{
  REQUIRE_EQ(std::string("192.168.0.1"), cache_.find("www", 80));
  REQUIRE_EQ(std::string("192.168.0.1"), cache_.find("ftp", 21));
  REQUIRE_EQ(std::string("192.168.1.2"), cache_.find("proxy", 8080));
  REQUIRE_EQ(std::string(""), cache_.find("www", 8080));
  REQUIRE_EQ(std::string(""), cache_.find("another", 80));
}

void DNSCacheTest::testMarkBad()
{
  cache_.markBad("www", "192.168.0.1", 80);
  REQUIRE_EQ(std::string("::1"), cache_.find("www", 80));
}

void DNSCacheTest::testPutBadAddr()
{
  cache_.markBad("www", "192.168.0.1", 80);
  cache_.put("www", "192.168.0.1", 80);
  REQUIRE_EQ(std::string("::1"), cache_.find("www", 80));
}

void DNSCacheTest::testRemove()
{
  cache_.remove("www", 80);
  REQUIRE_EQ(std::string(""), cache_.find("www", 80));
}

} // namespace aria2
