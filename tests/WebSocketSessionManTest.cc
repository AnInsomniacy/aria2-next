#include "WebSocketSessionMan.h"

#include <cppunit/extensions/HelperMacros.h>

#include "AsioRuntime.h"
#include "DownloadEngine.h"
#include "Option.h"
#include "RequestGroup.h"
#include "RpcWebSocketSession.h"
#include "SelectEventPoll.h"
#include "SocketCore.h"
#include "prefs.h"

#include <boost/beast/http.hpp>

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

  boost::asio::ip::tcp::socket socket(e_->getRuntime().ioContext());
  boost::beast::http::request<boost::beast::http::string_body> request;
  auto session = std::make_shared<RpcWebSocketSession>(std::move(socket),
                                                       e_.get(), request);

  CPPUNIT_ASSERT(!session->isAuthorized());
}

void WebSocketSessionManTest::testNotificationRecipientsExcludeUnauthorizedSessions()
{
  option_->put(PREF_RPC_SECRET, "secret");

  WebSocketSessionMan sessionMan;
  boost::beast::http::request<boost::beast::http::string_body> request;
  boost::asio::ip::tcp::socket socket1(e_->getRuntime().ioContext());
  auto unauthorizedSession = std::make_shared<RpcWebSocketSession>(
      std::move(socket1), e_.get(), request);
  boost::asio::ip::tcp::socket socket2(e_->getRuntime().ioContext());
  auto authorizedSession = std::make_shared<RpcWebSocketSession>(
      std::move(socket2), e_.get(), request);
  authorizedSession->markAuthorized();
  sessionMan.addSession(unauthorizedSession);
  sessionMan.addSession(authorizedSession);

  CPPUNIT_ASSERT_EQUAL((size_t)1, sessionMan.countNotificationRecipients());
}

} // namespace rpc

} // namespace aria2
