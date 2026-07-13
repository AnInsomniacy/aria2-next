#include "magnet.h"

#include <iostream>

#include "a2doctest.h"

namespace aria2 {

namespace magnet {

class MagnetTest {


public:
  void testParse();
  void testParseNumberedTracker();
};

A2_TEST(MagnetTest, testParse)
A2_TEST(MagnetTest, testParseNumberedTracker)

namespace {
const std::string& nthStr(const ValueBase* v, size_t index)
{
  return downcast<String>(downcast<List>(v)->get(index))->s();
}
} // namespace

void MagnetTest::testParse()
{
  auto r = parse(
      "magnet:?xt=urn:btih:248d0a1cd08284299de78d5c1ed359bb46717d8c&dn=aria2"
      "&tr=http%3A%2F%2Ftracker1&tr=http://tracker2");
  REQUIRE_EQ(
      std::string("urn:btih:248d0a1cd08284299de78d5c1ed359bb46717d8c"),
      nthStr(r->get("xt"), 0));
  REQUIRE_EQ(std::string("aria2"), nthStr(r->get("dn"), 0));
  REQUIRE_EQ(std::string("http://tracker1"), nthStr(r->get("tr"), 0));
  REQUIRE_EQ(std::string("http://tracker2"), nthStr(r->get("tr"), 1));
  REQUIRE(!parse("http://localhost"));
}

void MagnetTest::testParseNumberedTracker()
{
  auto r = parse("magnet:?xt=urn:btih:248d0a1cd08284299de78d5c1ed359bb46717d8c"
                 "&tr.1=http://tracker1&tr.2=http://tracker2");

  REQUIRE_EQ(std::string("http://tracker1"), nthStr(r->get("tr"), 0));
  REQUIRE_EQ(std::string("http://tracker2"), nthStr(r->get("tr"), 1));
}

} // namespace magnet

} // namespace aria2
