#include "OptionHandlerImpl.h"

#include "a2doctest.h"

#include "Option.h"
#include "prefs.h"
#include "Exception.h"
#include "help_tags.h"

namespace aria2 {

class OptionHandlerTest {


public:
  void testBooleanOptionHandler();
  void testNumberOptionHandler();
  void testNumberOptionHandler_min();
  void testNumberOptionHandler_max();
  void testNumberOptionHandler_min_max();
  void testUnitNumberOptionHandler();
  void testParameterOptionHandler();
  void testDefaultOptionHandler();
  void testFloatNumberOptionHandler();
  void testFloatNumberOptionHandler_min();
  void testFloatNumberOptionHandler_max();
  void testFloatNumberOptionHandler_min_max();
  void testHttpProxyOptionHandler();
  void testDeprecatedOptionHandler();
};

A2_TEST(OptionHandlerTest, testBooleanOptionHandler)
A2_TEST(OptionHandlerTest, testNumberOptionHandler)
A2_TEST(OptionHandlerTest, testNumberOptionHandler_min)
A2_TEST(OptionHandlerTest, testNumberOptionHandler_max)
A2_TEST(OptionHandlerTest, testNumberOptionHandler_min_max)
A2_TEST(OptionHandlerTest, testUnitNumberOptionHandler)
A2_TEST(OptionHandlerTest, testParameterOptionHandler)
A2_TEST(OptionHandlerTest, testDefaultOptionHandler)
A2_TEST(OptionHandlerTest, testFloatNumberOptionHandler)
A2_TEST(OptionHandlerTest, testFloatNumberOptionHandler_min)
A2_TEST(OptionHandlerTest, testFloatNumberOptionHandler_max)
A2_TEST(OptionHandlerTest, testFloatNumberOptionHandler_min_max)
A2_TEST(OptionHandlerTest, testHttpProxyOptionHandler)
A2_TEST(OptionHandlerTest, testDeprecatedOptionHandler)

void OptionHandlerTest::testBooleanOptionHandler()
{
  BooleanOptionHandler handler(PREF_DAEMON);
  Option option;
  handler.parse(option, A2_V_TRUE);
  REQUIRE_EQ(std::string(A2_V_TRUE), option.get(PREF_DAEMON));
  handler.parse(option, A2_V_FALSE);
  REQUIRE_EQ(std::string(A2_V_FALSE), option.get(PREF_DAEMON));
  try {
    handler.parse(option, "hello");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("true, false"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler()
{
  NumberOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "0");
  REQUIRE_EQ(std::string("0"), option.get(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("*-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler_min()
{
  NumberOptionHandler handler(PREF_TIMEOUT, "", "", 1);
  Option option;
  handler.parse(option, "1");
  REQUIRE_EQ(std::string("1"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "0");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("1-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler_max()
{
  NumberOptionHandler handler(PREF_TIMEOUT, "", "", -1, 100);
  Option option;
  handler.parse(option, "100");
  REQUIRE_EQ(std::string("100"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "101");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("*-100"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler_min_max()
{
  NumberOptionHandler handler(PREF_TIMEOUT, "", "", 1, 100);
  Option option;
  handler.parse(option, "1");
  REQUIRE_EQ(std::string("1"), option.get(PREF_TIMEOUT));
  handler.parse(option, "100");
  REQUIRE_EQ(std::string("100"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "0");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "101");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("1-100"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testUnitNumberOptionHandler()
{
  UnitNumberOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "4294967296");
  REQUIRE_EQ(std::string("4294967296"), option.get(PREF_TIMEOUT));
  handler.parse(option, "4096M");
  REQUIRE_EQ(std::string("4294967296"), option.get(PREF_TIMEOUT));
  handler.parse(option, "4096K");
  REQUIRE_EQ(std::string("4194304"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "K");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "M");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
}

void OptionHandlerTest::testParameterOptionHandler()
{
  ParameterOptionHandler handler(PREF_TIMEOUT, "", "", {"value1", "value2"});
  Option option;
  handler.parse(option, "value1");
  REQUIRE_EQ(std::string("value1"), option.get(PREF_TIMEOUT));
  handler.parse(option, "value2");
  REQUIRE_EQ(std::string("value2"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "value3");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("value1, value2"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testDefaultOptionHandler()
{
  DefaultOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "bar");
  REQUIRE_EQ(std::string("bar"), option.get(PREF_TIMEOUT));
  handler.parse(option, "");
  REQUIRE_EQ(std::string(""), option.get(PREF_TIMEOUT));
  REQUIRE_EQ(std::string(""), handler.createPossibleValuesString());

  handler.addTag(TAG_ADVANCED);
  REQUIRE_EQ(std::string("#advanced"), handler.toTagString());
  handler.addTag(TAG_BASIC);
  REQUIRE_EQ(std::string("#basic, #advanced"), handler.toTagString());
  REQUIRE(handler.hasTag(TAG_ADVANCED));
  REQUIRE(handler.hasTag(TAG_BASIC));
  REQUIRE(!handler.hasTag(TAG_HTTP));
}

void OptionHandlerTest::testFloatNumberOptionHandler()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "1.0");
  REQUIRE_EQ(std::string("1.0"), option.get(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("*-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testFloatNumberOptionHandler_min()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT, "", "", 0.0);
  Option option;
  handler.parse(option, "0.0");
  REQUIRE_EQ(std::string("0.0"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "-0.1");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("0.0-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testFloatNumberOptionHandler_max()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT, "", "", -1, 10.0);
  Option option;
  handler.parse(option, "10.0");
  REQUIRE_EQ(std::string("10.0"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "10.1");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("*-10.0"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testFloatNumberOptionHandler_min_max()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT, "", "", 0.0, 10.0);
  Option option;
  handler.parse(option, "0.0");
  REQUIRE_EQ(std::string("0.0"), option.get(PREF_TIMEOUT));
  handler.parse(option, "10.0");
  REQUIRE_EQ(std::string("10.0"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "-0.1");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "10.1");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("0.0-10.0"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testHttpProxyOptionHandler()
{
  HttpProxyOptionHandler handler(PREF_HTTP_PROXY, "", "");
  Option option;
  handler.parse(option, "proxy:65535");
  REQUIRE_EQ(std::string("http://proxy:65535/"),
                       option.get(PREF_HTTP_PROXY));

  handler.parse(option, "http://proxy:8080");
  REQUIRE_EQ(std::string("http://proxy:8080/"),
                       option.get(PREF_HTTP_PROXY));

  handler.parse(option, "");
  REQUIRE(option.defined(PREF_HTTP_PROXY));
  REQUIRE(option.blank(PREF_HTTP_PROXY));

  try {
    handler.parse(option, "http://bar:65536");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  REQUIRE_EQ(std::string("[http://][USER:PASSWORD@]HOST[:PORT]"),
                       handler.createPossibleValuesString());

  handler.parse(option, "http://user%40:passwd%40@proxy:8080");
  REQUIRE_EQ(std::string("http://user%40:passwd%40@proxy:8080/"),
                       option.get(PREF_HTTP_PROXY));

  handler.parse(option, "http://[::1]:8080");
  REQUIRE_EQ(std::string("http://[::1]:8080/"),
                       option.get(PREF_HTTP_PROXY));
}

void OptionHandlerTest::testDeprecatedOptionHandler()
{
  {
    DeprecatedOptionHandler handler(new DefaultOptionHandler(PREF_TIMEOUT));
    Option option;
    handler.parse(option, "foo");
    REQUIRE(!option.defined(PREF_TIMEOUT));
  }
  {
    DefaultOptionHandler dir(PREF_DIR);
    DeprecatedOptionHandler handler(new DefaultOptionHandler(PREF_TIMEOUT),
                                    &dir);
    Option option;
    handler.parse(option, "foo");
    REQUIRE(!option.defined(PREF_TIMEOUT));
    REQUIRE_EQ(std::string("foo"), option.get(PREF_DIR));
  }
}

} // namespace aria2
