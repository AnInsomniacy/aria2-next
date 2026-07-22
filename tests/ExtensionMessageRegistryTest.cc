#include "ExtensionMessageRegistry.h"

#include <string>

#include "a2doctest.h"

namespace aria2 {

class ExtensionMessageRegistryTest {


public:
  void testStrBtExtension();
  void testKeyBtExtension();
  void testGetExtensionMessageID();
};

A2_TEST(ExtensionMessageRegistryTest, testStrBtExtension)
A2_TEST(ExtensionMessageRegistryTest, testKeyBtExtension)
A2_TEST(ExtensionMessageRegistryTest, testGetExtensionMessageID)

void ExtensionMessageRegistryTest::testStrBtExtension()
{
  REQUIRE_EQ(
      std::string("ut_pex"),
      std::string(strBtExtension(ExtensionMessageRegistry::UT_PEX)));
  REQUIRE_EQ(
      std::string("ut_metadata"),
      std::string(strBtExtension(ExtensionMessageRegistry::UT_METADATA)));
  REQUIRE(!strBtExtension(100));
}

void ExtensionMessageRegistryTest::testKeyBtExtension()
{
  REQUIRE_EQ((int)ExtensionMessageRegistry::UT_PEX,
                       keyBtExtension("ut_pex"));
  REQUIRE_EQ((int)ExtensionMessageRegistry::UT_METADATA,
                       keyBtExtension("ut_metadata"));
  REQUIRE_EQ((int)ExtensionMessageRegistry::MAX_EXTENSION,
                       keyBtExtension("unknown"));
}

void ExtensionMessageRegistryTest::testGetExtensionMessageID()
{
  ExtensionMessageRegistry reg;
  REQUIRE_EQ(
      (uint8_t)0, reg.getExtensionMessageID(ExtensionMessageRegistry::UT_PEX));
  REQUIRE(!reg.getExtensionName(0));
  REQUIRE(!reg.getExtensionName(1));
  REQUIRE(!reg.getExtensionName(100));

  reg.setExtensionMessageID(ExtensionMessageRegistry::UT_PEX, 1);

  REQUIRE_EQ(std::string("ut_pex"),
                       std::string(reg.getExtensionName(1)));
  REQUIRE_EQ(
      (uint8_t)1, reg.getExtensionMessageID(ExtensionMessageRegistry::UT_PEX));

  reg.setExtensionMessageID(ExtensionMessageRegistry::UT_METADATA, 127);

  REQUIRE_EQ(
      (uint8_t)127,
      reg.getExtensionMessageID(ExtensionMessageRegistry::UT_METADATA));
  REQUIRE_EQ(
      (uint8_t)1, reg.getExtensionMessageID(ExtensionMessageRegistry::UT_PEX));

  reg.removeExtension(ExtensionMessageRegistry::UT_PEX);

  REQUIRE_EQ(
      (uint8_t)127,
      reg.getExtensionMessageID(ExtensionMessageRegistry::UT_METADATA));
  REQUIRE_EQ(
      (uint8_t)0, reg.getExtensionMessageID(ExtensionMessageRegistry::UT_PEX));
  REQUIRE(!reg.getExtensionName(1));
}

} // namespace aria2
