#include "DownloadEngine.h"

#include <cppunit/extensions/HelperMacros.h>

#include "SelectEventPoll.h"
#include "SocketCore.h"

namespace aria2 {

class DownloadEngineTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadEngineTest);
  CPPUNIT_TEST(testHttpsSocketPoolRequiresSameHostname);
  CPPUNIT_TEST_SUITE_END();

public:
  void testHttpsSocketPoolRequiresSameHostname();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadEngineTest);

void DownloadEngineTest::testHttpsSocketPoolRequiresSameHostname()
{
  DownloadEngine e(make_unique<SelectEventPoll>());
  auto socket = std::make_shared<SocketCore>();

  e.poolSocketForHostname("192.0.2.1", 443, "origin.example", socket);

  CPPUNIT_ASSERT(!e.popPooledSocketForHostname("192.0.2.1", 443,
                                               "redirect.example"));
  CPPUNIT_ASSERT_EQUAL(socket, e.popPooledSocketForHostname("192.0.2.1", 443,
                                                           "origin.example"));
}

} // namespace aria2
