#include "ByteArrayDiskWriter.h"
#include <string>
#include "a2doctest.h"

namespace aria2 {

class ByteArrayDiskWriterTest {


private:
public:
  void setUp() {}

  void testWriteAndRead();
  void testWriteAndRead2();
};

A2_TEST(ByteArrayDiskWriterTest, testWriteAndRead)
A2_TEST(ByteArrayDiskWriterTest, testWriteAndRead2)

void ByteArrayDiskWriterTest::testWriteAndRead()
{
  ByteArrayDiskWriter bw;

  std::string msg1 = "Hello";
  bw.writeData((const unsigned char*)msg1.c_str(), msg1.size(), 0);
  // write at the end of stream
  std::string msg2 = " World";
  bw.writeData((const unsigned char*)msg2.c_str(), msg2.size(), 5);
  // write at the end of stream +1
  std::string msg3 = "!!";
  bw.writeData((const unsigned char*)msg3.c_str(), msg3.size(), 12);
  // write space at the 'hole'
  std::string msg4 = " ";
  bw.writeData((const unsigned char*)msg4.c_str(), msg4.size(), 11);

  char buf[100];
  int32_t c = bw.readData((unsigned char*)buf, sizeof(buf), 1);
  buf[c] = '\0';

  REQUIRE_EQ(std::string("ello World !!"), std::string(buf));
  REQUIRE_EQ((int64_t)14, bw.size());
}

void ByteArrayDiskWriterTest::testWriteAndRead2()
{
  ByteArrayDiskWriter bw;

  std::string msg1 = "Hello World";
  bw.writeData((const unsigned char*)msg1.c_str(), msg1.size(), 0);
  std::string msg2 = "From Mars";
  bw.writeData((const unsigned char*)msg2.c_str(), msg2.size(), 6);

  char buf[100];
  int32_t c = bw.readData((unsigned char*)buf, sizeof(buf), 0);
  buf[c] = '\0';

  REQUIRE_EQ(std::string("Hello From Mars"), std::string(buf));
  REQUIRE_EQ((int64_t)15, bw.size());
}

} // namespace aria2
