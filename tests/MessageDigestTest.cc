#include "MessageDigest.h"

#include "a2doctest.h"

#include "util.h"

namespace aria2 {

class MessageDigestTest {


  std::unique_ptr<MessageDigest> sha1_;
  std::unique_ptr<MessageDigest> md5_;
  std::unique_ptr<MessageDigest> adler32_;

public:
  void setUp()
  {
    md5_ = MessageDigest::create("md5");
    sha1_ = MessageDigest::sha1();
#ifdef HAVE_ZLIB
    adler32_ = MessageDigest::create("adler32");
#endif // HAVE_ZLIB
  }

  void testDigest();
  void testSupports();
  void testGetDigestLength();
  void testIsStronger();
  void testIsValidHash();
  void testGetCanonicalHashType();
};

A2_TEST(MessageDigestTest, testDigest)
A2_TEST(MessageDigestTest, testSupports)
A2_TEST(MessageDigestTest, testGetDigestLength)
A2_TEST(MessageDigestTest, testIsStronger)
A2_TEST(MessageDigestTest, testIsValidHash)
A2_TEST(MessageDigestTest, testGetCanonicalHashType)

void MessageDigestTest::testDigest()
{
  md5_->update("aria2", 5);
  REQUIRE_EQ(std::string("2c90cadbef42945f0dcff2b959977ff8"),
                       util::toHex(md5_->digest()));
  md5_->reset();
  md5_->update("abc", 3);
  REQUIRE_EQ(std::string("900150983cd24fb0d6963f7d28e17f72"),
                       util::toHex(md5_->digest()));

  sha1_->update("aria2", 5);
  REQUIRE_EQ(std::string("f36003f22b462ffa184390533c500d8989e9f681"),
                       util::toHex(sha1_->digest()));
  sha1_->reset();
  sha1_->update("abc", 3);
  REQUIRE_EQ(std::string("a9993e364706816aba3e25717850c26c9cd0d89d"),
                       util::toHex(sha1_->digest()));

#ifdef HAVE_ZLIB
  adler32_->reset();
  adler32_->update("aria2", 5);
  REQUIRE_EQ(std::string("05e101d0"),
                       util::toHex(adler32_->digest()));

  adler32_->reset();
  adler32_->update("abc", 3);
  REQUIRE_EQ(std::string("024d0127"),
                       util::toHex(adler32_->digest()));
#endif // HAVE_ZLIB
}

void MessageDigestTest::testSupports()
{
  REQUIRE(MessageDigest::supports("md5"));
  REQUIRE(MessageDigest::supports("sha-1"));
  // Fails because sha1 is not valid name.
  REQUIRE(!MessageDigest::supports("sha1"));
}

void MessageDigestTest::testGetDigestLength()
{
  REQUIRE_EQ((size_t)16, MessageDigest::getDigestLength("md5"));
  REQUIRE_EQ((size_t)20, MessageDigest::getDigestLength("sha-1"));
  REQUIRE_EQ((size_t)20, sha1_->getDigestLength());
}

void MessageDigestTest::testIsStronger()
{
  REQUIRE(MessageDigest::isStronger("sha-1", "md5"));
  REQUIRE(!MessageDigest::isStronger("md5", "sha-1"));
  REQUIRE(!MessageDigest::isStronger("unknown", "sha-1"));
  REQUIRE(MessageDigest::isStronger("sha-1", "unknown"));
}

void MessageDigestTest::testIsValidHash()
{
  REQUIRE(MessageDigest::isValidHash(
      "sha-1", "f36003f22b462ffa184390533c500d8989e9f681"));
  REQUIRE(
      !MessageDigest::isValidHash("sha-1", "f36003f22b462ffa184390533c500d89"));
}

void MessageDigestTest::testGetCanonicalHashType()
{
  REQUIRE_EQ(std::string("sha-1"),
                       MessageDigest::getCanonicalHashType("sha1"));
  REQUIRE_EQ(std::string("sha-256"),
                       MessageDigest::getCanonicalHashType("sha256"));
  REQUIRE_EQ(std::string("unknown"),
                       MessageDigest::getCanonicalHashType("unknown"));
}

} // namespace aria2
