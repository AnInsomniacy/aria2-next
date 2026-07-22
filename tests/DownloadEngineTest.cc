#include "DownloadEngine.h"

#include "a2doctest.h"

#include "SelectEventPoll.h"
#include "SocketCore.h"

namespace aria2 {

class DownloadEngineTest {


public:
  void testHttpsSocketPoolRequiresSameHostname();
};

A2_TEST(DownloadEngineTest, testHttpsSocketPoolRequiresSameHostname)

void DownloadEngineTest::testHttpsSocketPoolRequiresSameHostname()
{
  DownloadEngine e(make_unique<SelectEventPoll>());
  auto socket = std::make_shared<SocketCore>();

  e.poolSocketForHostname("192.0.2.1", 443, "origin.example", socket);

  REQUIRE(!e.popPooledSocketForHostname("192.0.2.1", 443,
                                               "redirect.example"));
  REQUIRE_UNARY(socket == e.popPooledSocketForHostname(
                              "192.0.2.1", 443, "origin.example"));
}

} // namespace aria2
