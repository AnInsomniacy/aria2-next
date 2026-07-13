#include "WrDiskCache.h"

#include <cstring>

#include "a2doctest.h"

#include "TestUtil.h"
#include "DirectDiskAdaptor.h"
#include "ByteArrayDiskWriter.h"

namespace aria2 {

class WrDiskCacheTest {


  std::shared_ptr<DirectDiskAdaptor> adaptor_;
  ByteArrayDiskWriter* writer_;

public:
  void setUp()
  {
    adaptor_ = std::make_shared<DirectDiskAdaptor>();
    auto dw = make_unique<ByteArrayDiskWriter>();
    writer_ = dw.get();
    adaptor_->setDiskWriter(std::move(dw));
  }

  void testAdd();
};

A2_TEST(WrDiskCacheTest, testAdd)

void WrDiskCacheTest::testAdd()
{
  WrDiskCache dc(20);
  REQUIRE_EQ((size_t)0, dc.getSize());
  WrDiskCacheEntry e1(adaptor_);
  e1.cacheData(createDataCell(0, "who knows?"));
  REQUIRE(dc.add(&e1));
  REQUIRE_EQ((size_t)10, dc.getSize());

  WrDiskCacheEntry e2(adaptor_);
  e2.cacheData(createDataCell(21, "seconddata"));
  REQUIRE(dc.add(&e2));
  REQUIRE_EQ((size_t)20, dc.getSize());

  WrDiskCacheEntry e3(adaptor_);
  e3.cacheData(createDataCell(10, "hello"));
  REQUIRE(dc.add(&e3));
  REQUIRE_EQ((size_t)15, dc.getSize());
  // e1 is flushed to the disk
  REQUIRE_EQ(std::string("who knows?"), writer_->getString());
  REQUIRE_EQ((size_t)0, e1.getSize());

  e3.cacheData(createDataCell(15, " world"));
  REQUIRE(dc.update(&e3, 6));

  // e3 is flushed to the disk
  REQUIRE_EQ(std::string("who knows?hello world"),
                       writer_->getString());
  REQUIRE_EQ((size_t)0, e3.getSize());
  REQUIRE_EQ((size_t)10, dc.getSize());

  e2.cacheData(createDataCell(31, "01234567890"));
  REQUIRE(dc.update(&e2, 11));
  // e2 is flushed to the disk
  REQUIRE_EQ(
      std::string("who knows?hello worldseconddata01234567890"),
      writer_->getString());
  REQUIRE_EQ((size_t)0, e2.getSize());
  REQUIRE_EQ((size_t)0, dc.getSize());
}

} // namespace aria2
