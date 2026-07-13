#include "DefaultDiskWriter.h"
#include "a2doctest.h"

#include "a2functional.h"

namespace aria2 {

class DefaultDiskWriterTest {


private:
public:
  void setUp() {}

  void testSize();
};

A2_TEST(DefaultDiskWriterTest, testSize)

void DefaultDiskWriterTest::testSize()
{
  DefaultDiskWriter dw(A2_TEST_DIR "/4096chunk.txt");
  dw.enableReadOnly();
  dw.openExistingFile();
  REQUIRE_EQ((int64_t)4_k, dw.size());
}

} // namespace aria2
