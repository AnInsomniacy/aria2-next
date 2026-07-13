#include "GZipEncoder.h"

#include "a2doctest.h"

#include "GZipDecoder.h"
#include "util.h"

namespace aria2 {

class GZipEncoderTest {


public:
  void testEncode();
  void testEncodeBinaryChunk();
};

A2_TEST(GZipEncoderTest, testEncode)
A2_TEST(GZipEncoderTest, testEncodeBinaryChunk)

void GZipEncoderTest::testEncode()
{
  GZipEncoder encoder;
  encoder.init();

  std::vector<std::string> inputs;
  inputs.push_back("Hello World");
  inputs.push_back("9223372036854775807");
  inputs.push_back("Fox");

  encoder << inputs[0];
  encoder << (int64_t)9223372036854775807LL;
  encoder << inputs[2].c_str();

  std::string gzippedData = encoder.str();

  GZipDecoder decoder;
  decoder.init();
  std::string gunzippedData =
      decoder.decode(reinterpret_cast<const unsigned char*>(gzippedData.data()),
                     gzippedData.size());
  REQUIRE(decoder.finished());
  REQUIRE_EQ(strjoin(inputs.begin(), inputs.end(), ""),
                       gunzippedData);
}

void GZipEncoderTest::testEncodeBinaryChunk()
{
  GZipEncoder encoder;
  encoder.init();

  std::string input;
  for (int i = 0; i < 16_k; ++i) {
    input.push_back(static_cast<char>(i & 0xff));
  }

  encoder.write(input.data(), input.size());
  std::string gzippedData = encoder.str();

  GZipDecoder decoder;
  decoder.init();
  std::string gunzippedData =
      decoder.decode(reinterpret_cast<const unsigned char*>(gzippedData.data()),
                     gzippedData.size());
  REQUIRE(decoder.finished());
  REQUIRE_EQ(input, gunzippedData);
}

} // namespace aria2
