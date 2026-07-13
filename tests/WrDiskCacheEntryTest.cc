#include "WrDiskCacheEntry.h"

#include <cstring>

#include "a2doctest.h"

#include "TestUtil.h"
#include "DirectDiskAdaptor.h"
#include "ByteArrayDiskWriter.h"

namespace aria2 {

class WrDiskCacheEntryTest {


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

  void testWriteToDisk();
  void testAppend();
  void testClear();
};

A2_TEST(WrDiskCacheEntryTest, testWriteToDisk)
A2_TEST(WrDiskCacheEntryTest, testAppend)
A2_TEST(WrDiskCacheEntryTest, testClear)

void WrDiskCacheEntryTest::testWriteToDisk()
{
  WrDiskCacheEntry e(adaptor_);
  e.cacheData(createDataCell(0, "??01234567", 2));
  e.cacheData(createDataCell(8, "890"));
  e.writeToDisk();
  REQUIRE_EQ((size_t)0, e.getSize());
  REQUIRE_EQ(std::string("01234567890"), writer_->getString());
}

void WrDiskCacheEntryTest::testAppend()
{
  WrDiskCacheEntry e(adaptor_);
  auto cell = new WrDiskCacheEntry::DataCell{};
  cell->goff = 0;
  size_t capacity = 6;
  size_t offset = 2;
  cell->data = new unsigned char[offset + capacity];
  memcpy(cell->data, "??foo", 3);
  cell->offset = offset;
  cell->len = 3;
  cell->capacity = capacity;
  e.cacheData(cell);
  REQUIRE_EQ((size_t)3,
                       e.append(3, (const unsigned char*)"barbaz", 6));
  REQUIRE_EQ((size_t)6, cell->len);
  REQUIRE_EQ((size_t)6, e.getSize());

  REQUIRE_EQ((size_t)0, e.append(7, (const unsigned char*)"FOO", 3));
}

void WrDiskCacheEntryTest::testClear()
{
  WrDiskCacheEntry e(adaptor_);
  e.cacheData(createDataCell(0, "foo"));
  e.clear();
  REQUIRE_EQ((size_t)0, e.getSize());
  REQUIRE_EQ(std::string(), writer_->getString());
}

} // namespace aria2
