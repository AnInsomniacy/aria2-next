#include "FeatureConfig.h"

#include <algorithm>

#include "a2doctest.h"

#include "a2functional.h"
#include "array_fun.h"
#include "util.h"

namespace aria2 {

class FeatureConfigTest {


public:
  void testGetDefaultPort();
  void testStrSupportedFeature();
  void testFeatureSummary();
};

A2_TEST(FeatureConfigTest, testGetDefaultPort)
A2_TEST(FeatureConfigTest, testStrSupportedFeature)
A2_TEST(FeatureConfigTest, testFeatureSummary)

void FeatureConfigTest::testGetDefaultPort()
{
  REQUIRE_EQ((uint16_t)80, getDefaultPort("http"));
  REQUIRE_EQ((uint16_t)443, getDefaultPort("https"));
  REQUIRE_EQ((uint16_t)21, getDefaultPort("ftp"));
  REQUIRE_EQ((uint16_t)22, getDefaultPort("sftp"));
}

void FeatureConfigTest::testStrSupportedFeature()
{
  const char* https = strSupportedFeature(FEATURE_HTTPS);
#ifdef ENABLE_SSL
  REQUIRE(https);
#else
  REQUIRE(!https);
#endif // ENABLE_SSL
  REQUIRE(!strSupportedFeature(MAX_FEATURE));

  auto sftp = strSupportedFeature(FEATURE_SFTP);
#ifdef HAVE_LIBSSH2
  REQUIRE(sftp);
#else  // !HAVE_LIBSSH2
  REQUIRE(!sftp);
#endif // !HAVE_LIBSSH2
}

void FeatureConfigTest::testFeatureSummary()
{
  const std::string features[] = {

#ifdef ENABLE_ASYNC_DNS
      "Async DNS",
#endif // ENABLE_ASYNC_DNS

#ifdef ENABLE_BITTORRENT
      "BitTorrent",
#endif // ENABLE_BITTORRENT

      "ED2K",

#ifdef HAVE_SQLITE3
      "Firefox3 Cookie",
#endif // HAVE_SQLITE3

#ifdef HAVE_ZLIB
      "GZip",
#endif // HAVE_ZLIB

#ifdef ENABLE_SSL
      "HTTPS",
#endif // ENABLE_SSL

      "Message Digest",

#ifdef ENABLE_METALINK
      "Metalink",
#endif // ENABLE_METALINK

#ifdef ENABLE_XML_RPC
      "XML-RPC",
#endif // ENABLE_XML_RPC

#ifdef HAVE_LIBSSH2
      "SFTP",
#endif // HAVE_LIBSSH2
  };

  std::string featuresString =
      strjoin(std::begin(features), std::end(features), ", ");
  REQUIRE_EQ(featuresString, featureSummary());
}

} // namespace aria2
