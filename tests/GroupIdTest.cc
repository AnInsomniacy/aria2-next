#include "GroupId.h"

#include "a2doctest.h"

#include "TestUtil.h"
#include "array_fun.h"

namespace aria2 {

class GroupIdTest {


public:
  void setUp() { GroupId::clear(); }

  void testCreate();
  void testToNumericId();
  void testExpandUnique();
  void testToHex();
  void testToAbbrevHex();
};

A2_TEST(GroupIdTest, testCreate)
A2_TEST(GroupIdTest, testToNumericId)
A2_TEST(GroupIdTest, testExpandUnique)
A2_TEST(GroupIdTest, testToHex)
A2_TEST(GroupIdTest, testToAbbrevHex)

void GroupIdTest::testCreate()
{
  std::shared_ptr<GroupId> gid = GroupId::create();
  REQUIRE(gid);
  REQUIRE(0 != gid->getNumericId());
  REQUIRE(!GroupId::import(gid->getNumericId()));
  REQUIRE(!GroupId::import(0));
}

void GroupIdTest::testToNumericId()
{
  a2_gid_t gid;
  std::string hex;
  hex = std::string(16, '0');
  REQUIRE_EQ((int)GroupId::ERR_INVALID,
                       GroupId::toNumericId(gid, hex.c_str()));

  hex = std::string(16, 'f');
  REQUIRE_EQ(0, GroupId::toNumericId(gid, hex.c_str()));
  REQUIRE_EQ((a2_gid_t)UINT64_MAX, gid);

  REQUIRE_EQ(0, GroupId::toNumericId(gid, "1234567890abcdef"));
  REQUIRE_EQ((a2_gid_t)1311768467294899695LL, gid);

  hex = std::string(15, 'f');
  REQUIRE_EQ((int)GroupId::ERR_INVALID,
                       GroupId::toNumericId(gid, hex.c_str()));

  REQUIRE_EQ((int)GroupId::ERR_INVALID,
                       GroupId::toNumericId(gid, ""));

  REQUIRE_EQ((int)GroupId::ERR_INVALID,
                       GroupId::toNumericId(gid, "1234567890abcdeg"));
}

void GroupIdTest::testExpandUnique()
{
  a2_gid_t gid;
  std::shared_ptr<GroupId> ids[] = {GroupId::import(0xff80000000010000LL),
                                    GroupId::import(0xff80000000020001LL),
                                    GroupId::import(0xfff8000000030000LL)};
  for (const auto& i : ids) {
    REQUIRE(i);
  }

  REQUIRE_EQ((int)GroupId::ERR_NOT_UNIQUE,
                       GroupId::expandUnique(gid, "ff8"));

  REQUIRE_EQ((int)GroupId::ERR_INVALID,
                       GroupId::expandUnique(gid, "ffg"));

  REQUIRE_EQ(
      (int)GroupId::ERR_INVALID,
      GroupId::expandUnique(gid, std::string(17, 'a').c_str()));

  REQUIRE_EQ((int)GroupId::ERR_INVALID,
                       GroupId::expandUnique(gid, ""));

  REQUIRE_EQ((int)GroupId::ERR_NOT_UNIQUE,
                       GroupId::expandUnique(gid, "ff800000000"));

  REQUIRE_EQ(0, GroupId::expandUnique(gid, "ff8000000001"));
  REQUIRE_EQ(std::string("ff80000000010000"), GroupId::toHex(gid));

  REQUIRE_EQ(0, GroupId::expandUnique(gid, "ff8000000002"));
  REQUIRE_EQ(std::string("ff80000000020001"), GroupId::toHex(gid));

  REQUIRE_EQ(0, GroupId::expandUnique(gid, "fff800000003"));
  REQUIRE_EQ(std::string("fff8000000030000"), GroupId::toHex(gid));

  REQUIRE_EQ((int)GroupId::ERR_NOT_FOUND,
                       GroupId::expandUnique(gid, "ff80000000031"));
}

void GroupIdTest::testToHex()
{
  REQUIRE_EQ(std::string("1234567890abcdef"),
                       GroupId::toHex(1311768467294899695LL));
  REQUIRE_EQ(std::string("0000000000000001"),
                       GroupId::toHex(0000000000000000001LL));
}

void GroupIdTest::testToAbbrevHex()
{
  REQUIRE_EQ(std::string("123456"),
                       GroupId::toAbbrevHex(1311768467294899695LL));
  REQUIRE_EQ(std::string("000000"),
                       GroupId::toAbbrevHex(0000000000000000001LL));
}

} // namespace aria2
