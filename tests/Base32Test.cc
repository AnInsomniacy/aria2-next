#include "base32.h"

#include "a2doctest.h"

#include "util.h"

namespace aria2 {

class Base32Test {


public:
  void testEncode();
  void testDecode();
};

A2_TEST(Base32Test, testEncode)
A2_TEST(Base32Test, testDecode)

void Base32Test::testEncode()
{
  REQUIRE_EQ(std::string(), base32::encode(""));
  REQUIRE_EQ(std::string("GE======"), base32::encode("1"));
  REQUIRE_EQ(std::string("GEZA===="), base32::encode("12"));
  REQUIRE_EQ(std::string("GEZDG==="), base32::encode("123"));
  REQUIRE_EQ(std::string("GEZDGNA="), base32::encode("1234"));
  REQUIRE_EQ(std::string("GEZDGNBV"), base32::encode("12345"));
  std::string s = "248d0a1cd08284";
  REQUIRE_EQ(std::string("ESGQUHGQQKCA===="),
                       base32::encode(util::fromHex(s.begin(), s.end())));
}

void Base32Test::testDecode()
{
  std::string s = "";
  REQUIRE_EQ(std::string(), base32::decode(s.begin(), s.end()));
  s = "GE======";
  REQUIRE_EQ(std::string("1"), base32::decode(s.begin(), s.end()));
  s = "GEZA====";
  REQUIRE_EQ(std::string("12"), base32::decode(s.begin(), s.end()));
  s = "GEZDG===";
  REQUIRE_EQ(std::string("123"), base32::decode(s.begin(), s.end()));
  s = "GEZDGNA=";
  REQUIRE_EQ(std::string("1234"), base32::decode(s.begin(), s.end()));
  s = "GEZDGNBV";
  REQUIRE_EQ(std::string("12345"),
                       base32::decode(s.begin(), s.end()));
  s = "gezdgnbv";
  REQUIRE_EQ(std::string("12345"),
                       base32::decode(s.begin(), s.end()));
}

} // namespace aria2
