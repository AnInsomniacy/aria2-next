#include "UTMetadataRequestTracker.h"

#include "a2doctest.h"

namespace aria2 {

class UTMetadataRequestTrackerTest {


public:
  void testAdd();
  void testRemove();
  void testGetAllTrackedIndex();
  void testCount();
  void testAvail();
};

A2_TEST(UTMetadataRequestTrackerTest, testAdd)
A2_TEST(UTMetadataRequestTrackerTest, testRemove)
A2_TEST(UTMetadataRequestTrackerTest, testGetAllTrackedIndex)
A2_TEST(UTMetadataRequestTrackerTest, testCount)
A2_TEST(UTMetadataRequestTrackerTest, testAvail)

void UTMetadataRequestTrackerTest::testAdd()
{
  UTMetadataRequestTracker tr;
  tr.add(1);
  REQUIRE(tr.tracks(1));
}

void UTMetadataRequestTrackerTest::testRemove()
{
  UTMetadataRequestTracker tr;
  tr.add(1);
  tr.remove(1);
  REQUIRE(!tr.tracks(1));
}

void UTMetadataRequestTrackerTest::testGetAllTrackedIndex()
{
  UTMetadataRequestTracker tr;
  tr.add(1);
  tr.add(2);

  std::vector<size_t> indexes = tr.getAllTrackedIndex();
  REQUIRE_EQ((size_t)2, indexes.size());
  REQUIRE_EQ((size_t)1, indexes[0]);
  REQUIRE_EQ((size_t)2, indexes[1]);
}

void UTMetadataRequestTrackerTest::testCount()
{
  UTMetadataRequestTracker tr;
  tr.add(1);
  tr.add(2);
  REQUIRE_EQ((size_t)2, tr.count());
}

void UTMetadataRequestTrackerTest::testAvail()
{
  UTMetadataRequestTracker tr;
  REQUIRE_EQ((size_t)1, tr.avail());
  tr.add(1);
  REQUIRE_EQ((size_t)0, tr.avail());
  tr.add(2);
  REQUIRE_EQ((size_t)0, tr.avail());
}

} // namespace aria2
