#include "WebSocketSessionMan.h"

#include "a2doctest.h"

#include "DownloadEngine.h"
#include "Option.h"
#include "RequestGroup.h"
#include "SelectEventPoll.h"
#include "SocketCore.h"
#include "WebSocketSession.h"
#include "prefs.h"

namespace aria2 {

namespace rpc {

class WebSocketSessionManTest {


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

A2_TEST(WebSocketSessionManTest, testSessionRequiresAuthorizationWhenRpcSecretIsSet)
A2_TEST(WebSocketSessionManTest, testNotificationRecipientsExcludeUnauthorizedSessions)

void WebSocketSessionManTest::testSessionRequiresAuthorizationWhenRpcSecretIsSet()
{
  option_->put(PREF_RPC_SECRET, "secret");

  auto session = std::make_shared<WebSocketSession>(
      std::make_shared<SocketCore>(), e_.get());

  REQUIRE(!session->isAuthorized());
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

  REQUIRE_EQ((size_t)1, sessionMan.countNotificationRecipients());
}

} // namespace rpc

} // namespace aria2
