#include "GZipDecoder.h"

#include <iostream>
#include <fstream>

#include "a2doctest.h"

#include "TestUtil.h"
#include "Exception.h"
#include "util.h"
#include "MessageDigest.h"

namespace aria2 {

class GZipDecoderTest {


public:
  void setUp() {}

  void tearDown() {}

  void testDecode();
};

A2_TEST(GZipDecoderTest, testDecode)

void GZipDecoderTest::testDecode()
{
  GZipDecoder decoder;
  decoder.init();

  std::string outfile(A2_TEST_OUT_DIR "/aria2_GZipDecoderTest_testDecode");

  char buf[4_k];
  std::ifstream in(A2_TEST_DIR "/gzip_decode_test.gz", std::ios::binary);
  std::ofstream out(outfile.c_str(), std::ios::binary);
  while (in) {
    in.read(buf, sizeof(buf));

    std::string r = decoder.decode(reinterpret_cast<const unsigned char*>(buf),
                                   in.gcount());

    out.write(r.data(), r.size());
  }
  REQUIRE(decoder.finished());
  decoder.release();

  out.close();

  REQUIRE_EQ(std::string("8b577b33c0411b2be9d4fa74c7402d54a8d21f96"),
                       fileHexDigest(MessageDigest::sha1().get(), outfile));
}

} // namespace aria2
