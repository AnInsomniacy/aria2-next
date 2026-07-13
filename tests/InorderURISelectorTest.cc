#include "InorderURISelector.h"

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "array_fun.h"
#include "FileEntry.h"

namespace aria2 {

class InorderURISelectorTest {


private:
  FileEntry fileEntry_;

  std::shared_ptr<InorderURISelector> sel;

public:
  void setUp()
  {
    fileEntry_.setUris(
        {"http://alpha/file", "ftp://alpha/file", "http://bravo/file"});

    sel.reset(new InorderURISelector());
  }

  void tearDown() {}

  void testSelect();
};

A2_TEST(InorderURISelectorTest, testSelect)

void InorderURISelectorTest::testSelect()
{
  std::vector<std::pair<size_t, std::string>> usedHosts;
  REQUIRE_EQ(std::string("http://alpha/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ(std::string("ftp://alpha/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ(std::string("http://bravo/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ(std::string(""), sel->select(&fileEntry_, usedHosts));
}

} // namespace aria2
