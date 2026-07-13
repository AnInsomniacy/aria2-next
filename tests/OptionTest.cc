#include "Option.h"

#include <string>

#include "a2doctest.h"

#include "prefs.h"

namespace aria2 {

class OptionTest {


private:
public:
  void setUp() {}

  void testPutAndGet();
  void testPutAndGetAsInt();
  void testPutAndGetAsDouble();
  void testDefined();
  void testBlank();
  void testMerge();
  void testParent();
  void testRemove();
};

A2_TEST(OptionTest, testPutAndGet)
A2_TEST(OptionTest, testPutAndGetAsInt)
A2_TEST(OptionTest, testPutAndGetAsDouble)
A2_TEST(OptionTest, testDefined)
A2_TEST(OptionTest, testBlank)
A2_TEST(OptionTest, testMerge)
A2_TEST(OptionTest, testParent)
A2_TEST(OptionTest, testRemove)

void OptionTest::testPutAndGet()
{
  Option op;
  op.put(PREF_TIMEOUT, "value");

  REQUIRE(op.defined(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("value"), op.get(PREF_TIMEOUT));
}

void OptionTest::testPutAndGetAsInt()
{
  Option op;
  op.put(PREF_TIMEOUT, "1000");

  REQUIRE(op.defined(PREF_TIMEOUT));
  REQUIRE_EQ((int32_t)1000, op.getAsInt(PREF_TIMEOUT));
}

void OptionTest::testPutAndGetAsDouble()
{
  Option op;
  op.put(PREF_TIMEOUT, "10.0");

  REQUIRE_EQ(10.0, op.getAsDouble(PREF_TIMEOUT));
}

void OptionTest::testDefined()
{
  Option op;
  op.put(PREF_TIMEOUT, "v");
  op.put(PREF_DIR, "");
  REQUIRE(op.defined(PREF_TIMEOUT));
  REQUIRE(op.defined(PREF_DIR));
  REQUIRE(!op.defined(PREF_DAEMON));
}

void OptionTest::testBlank()
{
  Option op;
  op.put(PREF_TIMEOUT, "v");
  op.put(PREF_DIR, "");
  REQUIRE(!op.blank(PREF_TIMEOUT));
  REQUIRE(op.blank(PREF_DIR));
  REQUIRE(op.blank(PREF_DAEMON));
}

void OptionTest::testMerge()
{
  Option src;
  src.put(PREF_TIMEOUT, "100");
  src.put(PREF_DAEMON, "true");
  Option dest;
  dest.put(PREF_DAEMON, "false");
  dest.put(PREF_DIR, "foo");
  dest.merge(src);
  REQUIRE_EQ(100, dest.getAsInt(PREF_TIMEOUT));
  REQUIRE(dest.getAsBool(PREF_DAEMON));
  REQUIRE_EQ(std::string("foo"), dest.get(PREF_DIR));
  REQUIRE(!dest.defined(PREF_OUT));
}

void OptionTest::testParent()
{
  Option child;
  std::shared_ptr<Option> parent(new Option());
  parent->put(PREF_TIMEOUT, "100");
  child.put(PREF_DIR, "foo");
  REQUIRE(!child.defined(PREF_TIMEOUT));
  REQUIRE(!child.definedLocal(PREF_TIMEOUT));
  child.setParent(parent);
  REQUIRE(child.defined(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("100"), child.get(PREF_TIMEOUT));
  REQUIRE_EQ((int32_t)100, child.getAsInt(PREF_TIMEOUT));
  REQUIRE(!child.definedLocal(PREF_TIMEOUT));
  // blank
  REQUIRE(!child.blank(PREF_DIR));
  child.put(PREF_DIR, "");
  REQUIRE(child.blank(PREF_DIR));
  REQUIRE(!child.blank(PREF_TIMEOUT));
  // override
  child.put(PREF_TIMEOUT, "200");
  REQUIRE(child.defined(PREF_TIMEOUT));
  REQUIRE(child.definedLocal(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("200"), child.get(PREF_TIMEOUT));
  child.removeLocal(PREF_TIMEOUT);
  REQUIRE(child.defined(PREF_TIMEOUT));
  REQUIRE(!child.definedLocal(PREF_TIMEOUT));
}

void OptionTest::testRemove()
{
  Option child;
  auto parent = std::make_shared<Option>();
  child.setParent(parent);

  child.put(PREF_DIR, "foo");
  child.put(PREF_TIMEOUT, "200");
  parent->put(PREF_DIR, "bar");
  parent->put(PREF_TIMEOUT, "400");

  child.remove(PREF_DIR);

  REQUIRE(!child.defined(PREF_DIR));

  child.removeLocal(PREF_TIMEOUT);

  REQUIRE(!child.definedLocal(PREF_TIMEOUT));
  REQUIRE(child.defined(PREF_TIMEOUT));
  REQUIRE(parent->defined(PREF_TIMEOUT));
}

} // namespace aria2
