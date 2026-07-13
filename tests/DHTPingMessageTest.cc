#include "DHTPingMessage.h"

#include "a2doctest.h"

#include "DHTNode.h"
#include "Exception.h"
#include "util.h"
#include "MockDHTMessageFactory.h"
#include "MockDHTMessageDispatcher.h"
#include "MockDHTMessage.h"
#include "bencode2.h"

namespace aria2 {

class DHTPingMessageTest {


public:
  std::shared_ptr<DHTNode> localNode_;
  std::shared_ptr<DHTNode> remoteNode_;

  void setUp()
  {
    localNode_ = std::make_shared<DHTNode>();
    remoteNode_ = std::make_shared<DHTNode>();
  }

  void tearDown() {}

  void testGetBencodedMessage();
  void testDoReceivedAction();

  class MockDHTMessageFactory2 : public MockDHTMessageFactory {
  public:
    virtual std::unique_ptr<DHTPingReplyMessage>
    createPingReplyMessage(const std::shared_ptr<DHTNode>& remoteNode,
                           const unsigned char* remoteNodeID,
                           const std::string& transactionID) override
    {
      unsigned char id[DHT_ID_LENGTH];
      std::fill(std::begin(id), std::end(id), '0');
      return make_unique<DHTPingReplyMessage>(localNode_, remoteNode, id,
                                              transactionID);
    }
  };
};

A2_TEST(DHTPingMessageTest, testGetBencodedMessage)
A2_TEST(DHTPingMessageTest, testDoReceivedAction)

void DHTPingMessageTest::testGetBencodedMessage()
{
  unsigned char tid[DHT_TRANSACTION_ID_LENGTH];
  util::generateRandomData(tid, DHT_TRANSACTION_ID_LENGTH);
  std::string transactionID(&tid[0], &tid[DHT_TRANSACTION_ID_LENGTH]);

  DHTPingMessage msg(localNode_, remoteNode_, transactionID);
  msg.setVersion("A200");

  std::string msgbody = msg.getBencodedMessage();

  Dict dict;
  dict.put("t", transactionID);
  dict.put("v", "A200");
  dict.put("y", "q");
  dict.put("q", "ping");
  auto aDict = Dict::g();
  aDict->put("id", String::g(localNode_->getID(), DHT_ID_LENGTH));
  dict.put("a", std::move(aDict));

  REQUIRE_EQ(bencode2::encode(&dict), msgbody);
}

void DHTPingMessageTest::testDoReceivedAction()
{
  unsigned char tid[DHT_TRANSACTION_ID_LENGTH];
  util::generateRandomData(tid, DHT_TRANSACTION_ID_LENGTH);
  std::string transactionID(&tid[0], &tid[DHT_TRANSACTION_ID_LENGTH]);

  MockDHTMessageDispatcher dispatcher;
  MockDHTMessageFactory2 factory;
  factory.setLocalNode(localNode_);

  DHTPingMessage msg(localNode_, remoteNode_, transactionID);
  msg.setMessageDispatcher(&dispatcher);
  msg.setMessageFactory(&factory);

  msg.doReceivedAction();

  REQUIRE_EQ((size_t)1, dispatcher.messageQueue_.size());
  auto m = dynamic_cast<DHTPingReplyMessage*>(
      dispatcher.messageQueue_[0].message_.get());
  REQUIRE(*localNode_ == *m->getLocalNode());
  REQUIRE(*remoteNode_ == *m->getRemoteNode());
  REQUIRE_EQ(std::string("ping"), m->getMessageType());
  REQUIRE_EQ(msg.getTransactionID(), m->getTransactionID());
}

} // namespace aria2
