#include "MetalinkEntry.h"

#include "a2doctest.h"

#include "MetalinkResource.h"
#include "a2functional.h"

namespace aria2 {

class MetalinkEntryTest {


private:
public:
  void setUp() {}
  void tearDown() {}

  void testDropUnsupportedResource();
  void testReorderResourcesByPriority();
  void testSetLocationPriority();
  void testSetProtocolPriority();
};

A2_TEST(MetalinkEntryTest, testDropUnsupportedResource)
A2_TEST(MetalinkEntryTest, testReorderResourcesByPriority)
A2_TEST(MetalinkEntryTest, testSetLocationPriority)
A2_TEST(MetalinkEntryTest, testSetProtocolPriority)

std::unique_ptr<MetalinkEntry> createTestEntry()
{
  auto entry = make_unique<MetalinkEntry>();
  auto res1 = make_unique<MetalinkResource>();
  res1->url = "ftp://myhost/aria2.tar.bz2";
  res1->type = MetalinkResource::TYPE_FTP;
  res1->location = "ro";
  res1->priority = 50;
  auto res2 = make_unique<MetalinkResource>();
  res2->url = "http://myhost/aria2.tar.bz2";
  res2->type = MetalinkResource::TYPE_HTTP;
  res2->location = "at";
  res2->priority = 1;
  auto res3 = make_unique<MetalinkResource>();
  res3->url = "http://myhost/aria2.torrent";
  res3->type = MetalinkResource::TYPE_BITTORRENT;
  res3->location = "al";
  res3->priority = 40;
  auto res4 = make_unique<MetalinkResource>();
  res4->url = "http://myhost/aria2.ext";
  res4->type = MetalinkResource::TYPE_NOT_SUPPORTED;
  res4->location = "ad";
  res4->priority = 90;
  auto res5 = make_unique<MetalinkResource>();
  res5->url = "https://myhost/aria2.tar.bz2";
  res5->type = MetalinkResource::TYPE_HTTPS;
  res5->location = "jp";
  res5->priority = 10;

  entry->resources.push_back(std::move(res1));
  entry->resources.push_back(std::move(res2));
  entry->resources.push_back(std::move(res3));
  entry->resources.push_back(std::move(res4));
  entry->resources.push_back(std::move(res5));
  return entry;
}

void MetalinkEntryTest::testDropUnsupportedResource()
{
  auto entry = createTestEntry();

  entry->dropUnsupportedResource();
#if defined(ENABLE_SSL) && defined(ENABLE_BITTORRENT)
  REQUIRE_EQ((size_t)4, entry->resources.size());
#elif defined(ENABLE_SSL) || defined(ENABLE_BITTORRENT)
  REQUIRE_EQ((size_t)3, entry->resources.size());
#else  // defined(ENABLE_SSL) || defined(ENABLE_BITTORRENT)
  REQUIRE_EQ((size_t)2, entry->resources.size());
#endif // defined(ENABLE_SSL) || defined(ENABLE_BITTORRENT)

  auto itr = std::begin(entry->resources);
  REQUIRE_EQ(MetalinkResource::TYPE_FTP, (*itr++)->type);
  REQUIRE_EQ(MetalinkResource::TYPE_HTTP, (*itr++)->type);
#ifdef ENABLE_BITTORRENT
  REQUIRE_EQ(MetalinkResource::TYPE_BITTORRENT, (*itr++)->type);
#endif // ENABLE_BITTORRENT
#ifdef ENABLE_SSL
  REQUIRE_EQ(MetalinkResource::TYPE_HTTPS, (*itr++)->type);
#endif // ENABLE_SSL
}

void MetalinkEntryTest::testReorderResourcesByPriority()
{
  auto entry = createTestEntry();

  entry->reorderResourcesByPriority();

  REQUIRE_EQ(1, entry->resources.at(0)->priority);
  REQUIRE_EQ(10, entry->resources.at(1)->priority);
  REQUIRE_EQ(40, entry->resources.at(2)->priority);
  REQUIRE_EQ(50, entry->resources.at(3)->priority);
  REQUIRE_EQ(90, entry->resources.at(4)->priority);
}

void MetalinkEntryTest::testSetLocationPriority()
{
  auto entry = createTestEntry();

  auto locations = std::vector<std::string>{"jp", "al", "ro"};

  entry->setLocationPriority(locations, -100);

  REQUIRE_EQ(std::string("ro"), entry->resources[0]->location);
  REQUIRE_EQ(-50, entry->resources[0]->priority);
  REQUIRE_EQ(std::string("at"), entry->resources[1]->location);
  REQUIRE_EQ(1, entry->resources[1]->priority);
  REQUIRE_EQ(std::string("al"), entry->resources[2]->location);
  REQUIRE_EQ(-60, entry->resources[2]->priority);
  REQUIRE_EQ(std::string("ad"), entry->resources[3]->location);
  REQUIRE_EQ(90, entry->resources[3]->priority);
  REQUIRE_EQ(std::string("jp"), entry->resources[4]->location);
  REQUIRE_EQ(-90, entry->resources[4]->priority);
}

void MetalinkEntryTest::testSetProtocolPriority()
{
  auto entry = createTestEntry();
  entry->setProtocolPriority("http", -1);
  REQUIRE_EQ(50, entry->resources[0]->priority); // ftp
  REQUIRE_EQ(0, entry->resources[1]->priority);  // http, -1
  REQUIRE_EQ(40, entry->resources[2]->priority); // bittorrent
  REQUIRE_EQ(90, entry->resources[3]->priority); // not supported
  REQUIRE_EQ(10, entry->resources[4]->priority); // https
}

} // namespace aria2
