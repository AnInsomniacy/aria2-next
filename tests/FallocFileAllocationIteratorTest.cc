#include "FallocFileAllocationIterator.h"

#include <fstream>
#include "a2doctest.h"

#include "a2functional.h"
#include "File.h"
#include "DefaultDiskWriter.h"

namespace aria2 {

class FallocFileAllocationIteratorTest {


private:
public:
  void setUp() {}

  void testAllocate();
};

A2_TEST(FallocFileAllocationIteratorTest, testAllocate)

void FallocFileAllocationIteratorTest::testAllocate()
{
// When fallocate is used, test fails if file system does not
// support it. So skip it.
#ifndef HAVE_FALLOCATE
  std::string fn =
      A2_TEST_OUT_DIR "/aria2_FallocFileAllocationIteratorTest_testAllocate";
  std::ofstream of(fn.c_str(), std::ios::binary);
  of << "0123456789";
  of.close();

  File f(fn);
  REQUIRE_EQ((int64_t)10, f.size());

  DefaultDiskWriter writer(fn);
  int64_t offset = 10;
  int64_t totalLength = 40_k;

  // we have to open file first.
  writer.openExistingFile();
  FallocFileAllocationIterator itr(&writer, offset, totalLength);

  itr.allocateChunk();
  REQUIRE(itr.finished());

  REQUIRE_EQ((int64_t)40_k, f.size());
#endif // !HAVE_FALLOCATE
}

} // namespace aria2
