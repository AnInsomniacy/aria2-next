#include "SingleFileAllocationIterator.h"
#include <fstream>
#include "a2doctest.h"

#include "File.h"
#include "DefaultDiskWriter.h"
#include "a2functional.h"

namespace aria2 {

class SingleFileAllocationIteratorTest {


private:
public:
  void setUp() {}

  void testAllocate();
};

A2_TEST(SingleFileAllocationIteratorTest, testAllocate)

void SingleFileAllocationIteratorTest::testAllocate()
{
  std::string dir = A2_TEST_OUT_DIR;
  std::string fname = "aria2_SingleFileAllocationIteratorTest_testAllocate";
  std::string fn = dir + "/" + fname;
  std::ofstream of(fn.c_str(), std::ios::binary);
  of << "0123456789";
  of.close();

  File x(fn);
  REQUIRE_EQ((int64_t)10, x.size());

  DefaultDiskWriter writer(fn);
  int64_t offset = 10;
  int64_t totalLength = 32_k + 8_k;

  // we have to open file first.
  writer.openExistingFile();
  SingleFileAllocationIterator itr(&writer, offset, totalLength);
  itr.init();

  while (!itr.finished()) {
    itr.allocateChunk();
  }
  File f(fn);
  REQUIRE_EQ((int64_t)40_k, f.size());
}

} // namespace aria2
