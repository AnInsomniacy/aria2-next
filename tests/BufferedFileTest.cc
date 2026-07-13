#include "BufferedFile.h"

#include <iostream>
#include <sstream>

#include "a2doctest.h"

#include "File.h"

namespace aria2 {

class BufferedFileTest {


public:
  void testOpen();
  void testTransferWithClosedFile();
};

A2_TEST(BufferedFileTest, testOpen)
A2_TEST(BufferedFileTest, testTransferWithClosedFile)

void BufferedFileTest::testOpen()
{
  File f(A2_TEST_OUT_DIR "/aria2_BufferedFileTest_testOpen");
  f.remove();
  BufferedFile fail(f.getPath().c_str(), IOFile::READ);
  REQUIRE(!fail);

  BufferedFile wr(f.getPath().c_str(), IOFile::WRITE);
  REQUIRE(wr);
  std::string msg = "aria2 rules\nalpha\nbravo\ncharlie";
  wr.write(msg.data(), msg.size());
  wr.close();

  BufferedFile rd(f.getPath().c_str(), IOFile::READ);
  char buf[256];
  size_t len = rd.read(buf, 11);
  REQUIRE_EQ((size_t)11, len);
  buf[len] = '\0';
  REQUIRE_EQ(std::string("aria2 rules"), std::string(buf));

  REQUIRE(rd.gets(buf, sizeof(buf)));
  REQUIRE_EQ(std::string("\n"), std::string(buf));

  REQUIRE(rd.gets(buf, sizeof(buf)));
  REQUIRE_EQ(std::string("alpha\n"), std::string(buf));

  REQUIRE(rd.getsn(buf, sizeof(buf)));
  REQUIRE_EQ(std::string("bravo"), std::string(buf));

  REQUIRE(rd.getsn(buf, sizeof(buf)));
  REQUIRE_EQ(std::string("charlie"), std::string(buf));

  REQUIRE(rd.eof());
}

void BufferedFileTest::testTransferWithClosedFile()
{
  File f(A2_TEST_OUT_DIR "/aria2_BufferedFileTest_testTransferWithClosedFile");
  f.remove();
  BufferedFile fail(f.getPath().c_str(), IOFile::READ);
  REQUIRE(!fail);

  std::stringstream out;
  REQUIRE_EQ((size_t)0, fail.transfer(out));
  REQUIRE(out.str().empty());
}

} // namespace aria2
