#include "UriListParser.h"

#include <sstream>
#include <algorithm>
#include <iostream>
#include <iterator>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "prefs.h"
#include "OptionHandler.h"

namespace aria2 {

class UriListParserTest {


private:
  std::string list2String(const std::vector<std::string>& src);

public:
  void setUp() {}

  void testHasNext();
};

A2_TEST(UriListParserTest, testHasNext)

std::string UriListParserTest::list2String(const std::vector<std::string>& src)
{
  std::ostringstream strm;
  std::copy(src.begin(), src.end(),
            std::ostream_iterator<std::string>(strm, " "));
  return util::strip(strm.str());
}

void UriListParserTest::testHasNext()
{
  std::string filename = A2_TEST_DIR "/filelist1.txt";

  UriListParser flp(filename);

  std::vector<std::string> uris;
  Option reqOp;

  REQUIRE(flp.hasNext());

  flp.parseNext(uris, reqOp);
  REQUIRE_EQ(
      std::string("http://localhost/index.html http://localhost2/index.html"),
      list2String(uris));

  uris.clear();
  reqOp.clear();

  REQUIRE(flp.hasNext());

  flp.parseNext(uris, reqOp);
  REQUIRE_EQ(std::string("ftp://localhost/aria2.tar.bz2"),
                       list2String(uris));
  REQUIRE_EQ(std::string("/tmp"), reqOp.get(PREF_DIR));
  REQUIRE_EQ(std::string("chunky_chocolate"), reqOp.get(PREF_OUT));

  uris.clear();
  reqOp.clear();

  REQUIRE(!flp.hasNext());

  flp.parseNext(uris, reqOp);
  REQUIRE_EQ(std::string(""), list2String(uris));

  REQUIRE(!flp.hasNext());
}

} // namespace aria2
