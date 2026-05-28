#include "FeatureConfig.h"

#include <algorithm>
#include <vector>

#include <curl/curl.h>

#include <cppunit/extensions/HelperMacros.h>

#include "a2functional.h"
#include "array_fun.h"
#include "util.h"

namespace aria2 {

class FeatureConfigTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(FeatureConfigTest);
  CPPUNIT_TEST(testGetDefaultPort);
  CPPUNIT_TEST(testStrSupportedFeature);
  CPPUNIT_TEST(testFeatureSummary);
  CPPUNIT_TEST(testUsedLibs);
  CPPUNIT_TEST_SUITE_END();

public:
  void testGetDefaultPort();
  void testStrSupportedFeature();
  void testFeatureSummary();
  void testUsedLibs();
};

CPPUNIT_TEST_SUITE_REGISTRATION(FeatureConfigTest);

void FeatureConfigTest::testGetDefaultPort()
{
  CPPUNIT_ASSERT_EQUAL((uint16_t)80, getDefaultPort("http"));
  CPPUNIT_ASSERT_EQUAL((uint16_t)443, getDefaultPort("https"));
  CPPUNIT_ASSERT_EQUAL((uint16_t)21, getDefaultPort("ftp"));
  CPPUNIT_ASSERT_EQUAL((uint16_t)990, getDefaultPort("ftps"));
  CPPUNIT_ASSERT_EQUAL((uint16_t)22, getDefaultPort("sftp"));
  CPPUNIT_ASSERT_EQUAL((uint16_t)22, getDefaultPort("scp"));
}

void FeatureConfigTest::testStrSupportedFeature()
{
  auto curlInfo = curl_version_info(CURLVERSION_NOW);
  const char* asyncDns = strSupportedFeature(FEATURE_ASYNC_DNS);
  if (curlInfo && (curlInfo->features & CURL_VERSION_ASYNCHDNS)) {
    CPPUNIT_ASSERT_EQUAL(std::string("Async DNS"), std::string(asyncDns));
  }
  else {
    CPPUNIT_ASSERT(!asyncDns);
  }

  const char* https = strSupportedFeature(FEATURE_HTTPS);
#ifdef ENABLE_SSL
  CPPUNIT_ASSERT(https);
#else
  CPPUNIT_ASSERT(!https);
#endif // ENABLE_SSL
  CPPUNIT_ASSERT(!strSupportedFeature(MAX_FEATURE));

}

void FeatureConfigTest::testFeatureSummary()
{
  std::vector<std::string> features;

  auto curlInfo = curl_version_info(CURLVERSION_NOW);
  if (curlInfo && (curlInfo->features & CURL_VERSION_ASYNCHDNS)) {
    features.push_back("Async DNS");
  }
#ifdef ENABLE_BITTORRENT
  features.push_back("BitTorrent");
#endif // ENABLE_BITTORRENT

  features.push_back("ED2K");

#ifdef HAVE_ZLIB
  features.push_back("GZip");
#endif // HAVE_ZLIB

#ifdef ENABLE_SSL
  features.push_back("HTTPS");
#endif // ENABLE_SSL

  features.push_back("Message Digest");

  std::string featuresString =
      strjoin(std::begin(features), std::end(features), ", ");
  CPPUNIT_ASSERT_EQUAL(featuresString, featureSummary());
}

void FeatureConfigTest::testUsedLibs()
{
  auto libs = usedLibs();
  CPPUNIT_ASSERT(libs.find("libcurl/") != std::string::npos);

  auto curlInfo = curl_version_info(CURLVERSION_NOW);
  if (curlInfo && curlInfo->ares && curlInfo->ares[0]) {
    CPPUNIT_ASSERT(libs.find("c-ares/") != std::string::npos);
  }
}

} // namespace aria2
