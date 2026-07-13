#include "DirectDiskAdaptor.h"

#include "a2doctest.h"

#include "FileEntry.h"
#include "DefaultDiskWriter.h"
#include "DiskWriter.h"
#include "Exception.h"
#include "util.h"
#include "TestUtil.h"
#include "ByteArrayDiskWriter.h"
#include "WrDiskCacheEntry.h"
#include "FallocFileAllocationIterator.h"

namespace aria2 {

class DirectDiskAdaptorTest {


public:
  void setUp() {}

  void tearDown() {}

  void testCutTrailingGarbage();
  void testNoAllocationEnablesSparse();
  void testAdaptiveAllocationUsesPlatformAllocator();
  void testWriteCache();
  void testWriteCache_mergesContiguousCells();
};

A2_TEST(DirectDiskAdaptorTest, testCutTrailingGarbage)
A2_TEST(DirectDiskAdaptorTest, testNoAllocationEnablesSparse)
A2_TEST(DirectDiskAdaptorTest, testAdaptiveAllocationUsesPlatformAllocator)
A2_TEST(DirectDiskAdaptorTest, testWriteCache)
A2_TEST(DirectDiskAdaptorTest, testWriteCache_mergesContiguousCells)

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

  REQUIRE_EQ((int64_t)entry->getLength(),
                       File(entry->getPath()).size());
}

namespace {
class SparseRecordingDiskWriter : public DiskWriter {
public:
  virtual void initAndOpenFile(int64_t totalLength = 0) override
  {
    ++initAndOpenFileCount;
  }

  virtual void openFile(int64_t totalLength = 0) override
  {
    ++openFileCount;
  }

  virtual void closeFile() override {}

  virtual void openExistingFile(int64_t totalLength = 0) override
  {
    ++openExistingFileCount;
  }

  virtual void writeData(const unsigned char* data, size_t len,
                         int64_t offset) override
  {
  }

  virtual ssize_t readData(unsigned char* data, size_t len,
                           int64_t offset) override
  {
    return 0;
  }

  virtual void truncate(int64_t length) override
  {
    ++truncateCount;
  }

  virtual void enableSparse() override { ++enableSparseCount; }

  virtual int64_t size() override { return 0; }

  size_t initAndOpenFileCount = 0;
  size_t openFileCount = 0;
  size_t openExistingFileCount = 0;
  size_t truncateCount = 0;
  size_t enableSparseCount = 0;
};
} // namespace

void DirectDiskAdaptorTest::testNoAllocationEnablesSparse()
{
  auto entry = std::make_shared<FileEntry>(
      A2_TEST_OUT_DIR "/aria2_DirectDiskAdaptorTest_testNoAllocationEnablesSparse",
      256, 0);
  auto fileEntries = std::vector<std::shared_ptr<FileEntry>>{entry};
  DirectDiskAdaptor adaptor;
  auto writer = make_unique<SparseRecordingDiskWriter>();
  auto writerPtr = writer.get();
  adaptor.setDiskWriter(std::move(writer));
  adaptor.setTotalLength(entry->getLength());
  adaptor.setFileEntries(fileEntries.begin(), fileEntries.end());
  adaptor.setFileAllocationMethod(DiskAdaptor::FILE_ALLOC_NONE);

  adaptor.initAndOpenFile();

  REQUIRE_EQ((size_t)1, writerPtr->initAndOpenFileCount);
  REQUIRE_EQ((size_t)1, writerPtr->enableSparseCount);
  REQUIRE_EQ((size_t)0, writerPtr->truncateCount);
}

void DirectDiskAdaptorTest::testAdaptiveAllocationUsesPlatformAllocator()
{
  auto entry = std::make_shared<FileEntry>(
      A2_TEST_OUT_DIR
      "/aria2_DirectDiskAdaptorTest_testAdaptiveAllocationUsesPlatformAllocator",
      256, 0);
  auto fileEntries = std::vector<std::shared_ptr<FileEntry>>{entry};
  DirectDiskAdaptor adaptor;
  adaptor.setDiskWriter(make_unique<ByteArrayDiskWriter>());
  adaptor.setTotalLength(entry->getLength());
  adaptor.setFileEntries(fileEntries.begin(), fileEntries.end());
  adaptor.setFileAllocationMethod(DiskAdaptor::FILE_ALLOC_ADAPTIVE);

  auto itr = adaptor.fileAllocationIterator();

#ifdef HAVE_SOME_FALLOCATE
  REQUIRE(dynamic_cast<FallocFileAllocationIterator*>(itr.get()));
#endif // HAVE_SOME_FALLOCATE
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
  REQUIRE_EQ(data1 + data2, dw->getString().substr(5));

  cache.clear();
  dw->setString("");
  cache.cacheData(createDataCell(4_k, data1.c_str()));
  adaptor->writeCache(&cache);
  REQUIRE_EQ(data1, dw->getString().substr(4_k));

  cache.clear();
  dw->setString("???????");
  cache.cacheData(createDataCell(0, "abc"));
  cache.cacheData(createDataCell(4, "efg"));
  adaptor->writeCache(&cache);
  REQUIRE_EQ(std::string("abc?efg"), dw->getString());
}

namespace {
class RecordingDiskWriter : public DiskWriter {
public:
  virtual void initAndOpenFile(int64_t totalLength = 0) override {}
  virtual void openFile(int64_t totalLength = 0) override {}
  virtual void closeFile() override {}
  virtual void openExistingFile(int64_t totalLength = 0) override {}

  virtual void writeData(const unsigned char* data, size_t len,
                         int64_t offset) override
  {
    writes.emplace_back(offset,
                        std::string(reinterpret_cast<const char*>(data), len));
  }

  virtual ssize_t readData(unsigned char* data, size_t len,
                           int64_t offset) override
  {
    return 0;
  }

  virtual int64_t size() override { return 0; }

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

  REQUIRE_EQ((size_t)2, dw->writes.size());
  REQUIRE_EQ((int64_t)10, dw->writes[0].first);
  REQUIRE_EQ(std::string("abcdef"), dw->writes[0].second);
  REQUIRE_EQ((int64_t)20, dw->writes[1].first);
  REQUIRE_EQ(std::string("ghi"), dw->writes[1].second);
}

} // namespace aria2
