#include "GZipFile.h"

#include <iostream>

#include "a2doctest.h"

#include "File.h"

namespace aria2 {

class GZipFileTest {


public:
  void testOpen();
};

A2_TEST(GZipFileTest, testOpen)

void GZipFileTest::testOpen()
{
  File f(A2_TEST_OUT_DIR "/aria2_GZipFileTest_testOpen");
  f.remove();
  GZipFile fail(f.getPath().c_str(), GZipFile::READ);
  REQUIRE(!fail);

  GZipFile wr(f.getPath().c_str(), GZipFile::WRITE);
  REQUIRE(wr);
  std::string msg = "aria2 rules\nalpha\nbravo\ncharlie";
  wr.write(msg.data(), msg.size());
  wr.close();

  GZipFile rd(f.getPath().c_str(), GZipFile::READ);
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

} // namespace aria2
