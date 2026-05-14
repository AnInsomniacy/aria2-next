#include "WebSocketSessionMan.h"

#include <cppunit/extensions/HelperMacros.h>

#include "DownloadEngine.h"
#include "Option.h"
#include "RequestGroup.h"
#include "SelectEventPoll.h"
#include "SocketCore.h"
#include "WebSocketSession.h"
#include "prefs.h"

namespace aria2 {

namespace rpc {

class WebSocketSessionManTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(WebSocketSessionManTest);
  CPPUNIT_TEST(testSessionRequiresAuthorizationWhenRpcSecretIsSet);
  CPPUNIT_TEST(testNotificationRecipientsExcludeUnauthorizedSessions);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<Option> option_;
  std::shared_ptr<DownloadEngine> e_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
  }

  void testSessionRequiresAuthorizationWhenRpcSecretIsSet();
  void testNotificationRecipientsExcludeUnauthorizedSessions();
};

CPPUNIT_TEST_SUITE_REGISTRATION(WebSocketSessionManTest);

void WebSocketSessionManTest::testSessionRequiresAuthorizationWhenRpcSecretIsSet()
{
  option_->put(PREF_RPC_SECRET, "secret");

  auto session = std::make_shared<WebSocketSession>(
      std::make_shared<SocketCore>(), e_.get());

  CPPUNIT_ASSERT(!session->isAuthorized());
}

void WebSocketSessionManTest::testNotificationRecipientsExcludeUnauthorizedSessions()
{
  option_->put(PREF_RPC_SECRET, "secret");

  WebSocketSessionMan sessionMan;
  auto unauthorizedSession = std::make_shared<WebSocketSession>(
      std::make_shared<SocketCore>(), e_.get());
  auto authorizedSession = std::make_shared<WebSocketSession>(
      std::make_shared<SocketCore>(), e_.get());
  authorizedSession->markAuthorized();
  sessionMan.addSession(unauthorizedSession);
  sessionMan.addSession(authorizedSession);

  CPPUNIT_ASSERT_EQUAL((size_t)1, sessionMan.countNotificationRecipients());
}

} // namespace rpc

} // namespace aria2
