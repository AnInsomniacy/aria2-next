#include "DirectDiskAdaptor.h"

#include <cppunit/extensions/HelperMacros.h>

#include "FileEntry.h"
#include "DefaultDiskWriter.h"
#include "DiskWriter.h"
#include "Exception.h"
#include "util.h"
#include "TestUtil.h"
#include "ByteArrayDiskWriter.h"
#include "WrDiskCacheEntry.h"

namespace aria2 {

class DirectDiskAdaptorTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DirectDiskAdaptorTest);
  CPPUNIT_TEST(testCutTrailingGarbage);
  CPPUNIT_TEST(testWriteCache);
  CPPUNIT_TEST(testWriteCache_mergesContiguousCells);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testCutTrailingGarbage();
  void testWriteCache();
  void testWriteCache_mergesContiguousCells();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DirectDiskAdaptorTest);

void DirectDiskAdaptorTest::testCutTrailingGarbage()
{
  std::string dir = A2_TEST_OUT_DIR;
  auto entry = std::make_shared<FileEntry>(
      dir + "/aria2_DirectDiskAdaptorTest_testCutTrailingGarbage", 256, 0);
  createFile(entry->getPath(), entry->getLength() + 100);
  auto fileEntries = std::vector<std::shared_ptr<FileEntry>>{entry};
  DirectDiskAdaptor adaptor;
  adaptor.setDiskWriter(make_unique<DefaultDiskWriter>(entry->getPath()));
  adaptor.setTotalLength(entry->getLength());
  adaptor.setFileEntries(fileEntries.begin(), fileEntries.end());
  adaptor.openFile();

  adaptor.cutTrailingGarbage();

  CPPUNIT_ASSERT_EQUAL((int64_t)entry->getLength(),
                       File(entry->getPath()).size());
}

void DirectDiskAdaptorTest::testWriteCache()
{
  auto adaptor = std::make_shared<DirectDiskAdaptor>();
  ByteArrayDiskWriter* dw;
  {
    auto sdw = make_unique<ByteArrayDiskWriter>();
    dw = sdw.get();
    adaptor->setDiskWriter(std::move(sdw));
  }
  WrDiskCacheEntry cache{adaptor};
  std::string data1(4_k, '1'), data2(4094, '2');
  cache.cacheData(createDataCell(5, data1.c_str()));
  cache.cacheData(createDataCell(5 + data1.size(), data2.c_str()));
  adaptor->writeCache(&cache);
  CPPUNIT_ASSERT_EQUAL(data1 + data2, dw->getString().substr(5));

  cache.clear();
  dw->setString("");
  cache.cacheData(createDataCell(4_k, data1.c_str()));
  adaptor->writeCache(&cache);
  CPPUNIT_ASSERT_EQUAL(data1, dw->getString().substr(4_k));

  cache.clear();
  dw->setString("???????");
  cache.cacheData(createDataCell(0, "abc"));
  cache.cacheData(createDataCell(4, "efg"));
  adaptor->writeCache(&cache);
  CPPUNIT_ASSERT_EQUAL(std::string("abc?efg"), dw->getString());
}

namespace {
class RecordingDiskWriter : public DiskWriter {
public:
  virtual void initAndOpenFile(int64_t totalLength = 0) CXX11_OVERRIDE {}
  virtual void openFile(int64_t totalLength = 0) CXX11_OVERRIDE {}
  virtual void closeFile() CXX11_OVERRIDE {}
  virtual void openExistingFile(int64_t totalLength = 0) CXX11_OVERRIDE {}

  virtual void writeData(const unsigned char* data, size_t len,
                         int64_t offset) CXX11_OVERRIDE
  {
    writes.emplace_back(offset,
                        std::string(reinterpret_cast<const char*>(data), len));
  }

  virtual ssize_t readData(unsigned char* data, size_t len,
                           int64_t offset) CXX11_OVERRIDE
  {
    return 0;
  }

  virtual int64_t size() CXX11_OVERRIDE { return 0; }

  std::vector<std::pair<int64_t, std::string>> writes;
};
} // namespace

void DirectDiskAdaptorTest::testWriteCache_mergesContiguousCells()
{
  auto adaptor = std::make_shared<DirectDiskAdaptor>();
  RecordingDiskWriter* dw;
  {
    auto sdw = make_unique<RecordingDiskWriter>();
    dw = sdw.get();
    adaptor->setDiskWriter(std::move(sdw));
  }
  WrDiskCacheEntry cache{adaptor};
  cache.cacheData(createDataCell(10, "abc"));
  cache.cacheData(createDataCell(13, "def"));
  cache.cacheData(createDataCell(20, "ghi"));

  adaptor->writeCache(&cache);

  CPPUNIT_ASSERT_EQUAL((size_t)2, dw->writes.size());
  CPPUNIT_ASSERT_EQUAL((int64_t)10, dw->writes[0].first);
  CPPUNIT_ASSERT_EQUAL(std::string("abcdef"), dw->writes[0].second);
  CPPUNIT_ASSERT_EQUAL((int64_t)20, dw->writes[1].first);
  CPPUNIT_ASSERT_EQUAL(std::string("ghi"), dw->writes[1].second);
}

} // namespace aria2
