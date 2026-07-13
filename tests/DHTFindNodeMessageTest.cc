#include "DHTFindNodeMessage.h"

#include "a2doctest.h"

#include "DHTNode.h"
#include "Exception.h"
#include "util.h"
#include "MockDHTMessageFactory.h"
#include "MockDHTMessage.h"
#include "MockDHTMessageDispatcher.h"
#include "DHTRoutingTable.h"
#include "bencode2.h"

namespace aria2 {

class DHTFindNodeMessageTest {


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
    virtual std::unique_ptr<DHTFindNodeReplyMessage> createFindNodeReplyMessage(
        const std::shared_ptr<DHTNode>& remoteNode,
        std::vector<std::shared_ptr<DHTNode>> closestKNodes,
        const std::string& transactionID) override
    {
      auto m = make_unique<DHTFindNodeReplyMessage>(AF_INET, localNode_,
                                                    remoteNode, transactionID);
      m->setClosestKNodes(std::move(closestKNodes));
      return m;
    }
  };
};

A2_TEST(DHTFindNodeMessageTest, testGetBencodedMessage)
A2_TEST(DHTFindNodeMessageTest, testDoReceivedAction)

void DHTFindNodeMessageTest::testGetBencodedMessage()
{
  unsigned char tid[DHT_TRANSACTION_ID_LENGTH];
  util::generateRandomData(tid, DHT_TRANSACTION_ID_LENGTH);
  std::string transactionID(&tid[0], &tid[DHT_TRANSACTION_ID_LENGTH]);

  auto targetNode = std::make_shared<DHTNode>();

  DHTFindNodeMessage msg(localNode_, remoteNode_, targetNode->getID(),
                         transactionID);
  msg.setVersion("A200");
  std::string msgbody = msg.getBencodedMessage();

  Dict dict;
  dict.put("t", transactionID);
  dict.put("v", "A200");
  dict.put("y", "q");
  dict.put("q", "find_node");
  auto aDict = Dict::g();
  aDict->put("id", String::g(localNode_->getID(), DHT_ID_LENGTH));
  aDict->put("target", String::g(targetNode->getID(), DHT_ID_LENGTH));
  dict.put("a", std::move(aDict));

  REQUIRE_EQ(bencode2::encode(&dict), msgbody);
}

void DHTFindNodeMessageTest::testDoReceivedAction()
{
  unsigned char tid[DHT_TRANSACTION_ID_LENGTH];
  util::generateRandomData(tid, DHT_TRANSACTION_ID_LENGTH);
  std::string transactionID(&tid[0], &tid[DHT_TRANSACTION_ID_LENGTH]);

  auto targetNode = std::make_shared<DHTNode>();

  MockDHTMessageDispatcher dispatcher;
  MockDHTMessageFactory2 factory;
  factory.setLocalNode(localNode_);
  DHTRoutingTable routingTable(localNode_);
  routingTable.addNode(targetNode);

  DHTFindNodeMessage msg(localNode_, remoteNode_, targetNode->getID(),
                         transactionID);
  msg.setMessageDispatcher(&dispatcher);
  msg.setMessageFactory(&factory);
  msg.setRoutingTable(&routingTable);

  msg.doReceivedAction();

  REQUIRE_EQ((size_t)1, dispatcher.messageQueue_.size());
  auto m = dynamic_cast<DHTFindNodeReplyMessage*>(
      dispatcher.messageQueue_[0].message_.get());
  REQUIRE(*localNode_ == *m->getLocalNode());
  REQUIRE(*remoteNode_ == *m->getRemoteNode());
  REQUIRE_EQ(std::string("find_node"), m->getMessageType());
  REQUIRE_EQ(msg.getTransactionID(), m->getTransactionID());
  REQUIRE_EQ((size_t)1, m->getClosestKNodes().size());
}

} // namespace aria2
