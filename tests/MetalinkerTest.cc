#include "Metalinker.h"

#include "a2doctest.h"

#include "MetalinkEntry.h"
#include "a2functional.h"

namespace aria2 {

class MetalinkerTest {


private:
public:
  void setUp() {}
  void tearDown() {}

  void testQueryEntry();
};

A2_TEST(MetalinkerTest, testQueryEntry)

void MetalinkerTest::testQueryEntry()
{
  Metalinker metalinker;
  auto entry1 = make_unique<MetalinkEntry>();
  entry1->version = "0.5.2";
  entry1->languages.push_back("en-US");
  entry1->oses.push_back("Linux-x86");
  auto entry2 = make_unique<MetalinkEntry>();
  entry2->version = "0.5.1";
  entry2->languages.push_back("ja-JP");
  entry2->oses.push_back("Linux-m68k");
  metalinker.addEntry(std::move(entry1));
  metalinker.addEntry(std::move(entry2));

  std::string version;
  std::string language;
  std::string os;

  version = "0.5.1";
  language = "ja-JP";
  os = "Linux-m68k";
  {
    auto result = metalinker.queryEntry(version, language, os);
    REQUIRE_EQ((size_t)1, result.size());
    REQUIRE_EQ(std::string("0.5.1"), result.at(0)->version);
    REQUIRE_EQ(std::string("ja-JP"), result.at(0)->languages[0]);
    REQUIRE_EQ(std::string("Linux-m68k"), result.at(0)->oses[0]);
  }
  version = "0.6.0";
  language = "";
  os = "";
  {
    auto result = metalinker.queryEntry(version, language, os);
    REQUIRE_EQ((size_t)0, result.size());
  }

  version = "0.5.2";
  language = "";
  os = "";
  {
    auto result = metalinker.queryEntry(version, language, os);
    REQUIRE_EQ((size_t)1, result.size());
    REQUIRE_EQ(std::string("0.5.2"), result.at(0)->version);
    REQUIRE_EQ(std::string("en-US"), result.at(0)->languages[0]);
    REQUIRE_EQ(std::string("Linux-x86"), result.at(0)->oses[0]);
  }
}

} // namespace aria2
