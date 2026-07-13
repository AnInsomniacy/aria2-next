#include "DHTGetPeersMessage.h"

#include "a2doctest.h"

#include "DHTNode.h"
#include "Exception.h"
#include "util.h"
#include "MockDHTMessageFactory.h"
#include "MockDHTMessage.h"
#include "MockDHTMessageDispatcher.h"
#include "DHTTokenTracker.h"
#include "DHTPeerAnnounceStorage.h"
#include "DHTRoutingTable.h"
#include "bencode2.h"
#include "GroupId.h"
#include "DownloadContext.h"
#include "Option.h"
#include "RequestGroup.h"
#include "BtRegistry.h"
#include "TorrentAttribute.h"

namespace aria2 {

class DHTGetPeersMessageTest {


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
    virtual std::unique_ptr<DHTGetPeersReplyMessage> createGetPeersReplyMessage(
        const std::shared_ptr<DHTNode>& remoteNode,
        std::vector<std::shared_ptr<DHTNode>> closestKNodes,
        std::vector<std::shared_ptr<Peer>> peers, const std::string& token,
        const std::string& transactionID) override
    {
      auto m = make_unique<DHTGetPeersReplyMessage>(
          AF_INET, localNode_, remoteNode, token, transactionID);
      m->setClosestKNodes(closestKNodes);
      m->setValues(peers);
      return m;
    }
  };
};

A2_TEST(DHTGetPeersMessageTest, testGetBencodedMessage)
A2_TEST(DHTGetPeersMessageTest, testDoReceivedAction)

void DHTGetPeersMessageTest::testGetBencodedMessage()
{
  unsigned char tid[DHT_TRANSACTION_ID_LENGTH];
  util::generateRandomData(tid, DHT_TRANSACTION_ID_LENGTH);
  std::string transactionID(&tid[0], &tid[DHT_TRANSACTION_ID_LENGTH]);

  unsigned char infoHash[DHT_ID_LENGTH];
  util::generateRandomData(infoHash, DHT_ID_LENGTH);

  DHTGetPeersMessage msg(localNode_, remoteNode_, infoHash, transactionID);
  msg.setVersion("A200");

  std::string msgbody = msg.getBencodedMessage();

  Dict dict;
  dict.put("t", transactionID);
  dict.put("v", "A200");
  dict.put("y", "q");
  dict.put("q", "get_peers");
  auto aDict = Dict::g();
  aDict->put("id", String::g(localNode_->getID(), DHT_ID_LENGTH));
  aDict->put("info_hash", String::g(infoHash, DHT_ID_LENGTH));
  dict.put("a", std::move(aDict));

  REQUIRE_EQ(util::percentEncode(bencode2::encode(&dict)),
                       util::percentEncode(msgbody));
}

void DHTGetPeersMessageTest::testDoReceivedAction()
{
  remoteNode_->setIPAddress("192.168.0.1");
  remoteNode_->setPort(6881);

  unsigned char tid[DHT_TRANSACTION_ID_LENGTH];
  util::generateRandomData(tid, DHT_TRANSACTION_ID_LENGTH);
  std::string transactionID(&tid[0], &tid[DHT_TRANSACTION_ID_LENGTH]);

  unsigned char infoHash[DHT_ID_LENGTH];
  util::generateRandomData(infoHash, DHT_ID_LENGTH);

  DHTTokenTracker tokenTracker;
  MockDHTMessageDispatcher dispatcher;
  MockDHTMessageFactory2 factory;
  factory.setLocalNode(localNode_);
  DHTRoutingTable routingTable(localNode_);

  auto torrentAttrs = std::make_shared<TorrentAttribute>();
  torrentAttrs->infoHash = std::string(infoHash, infoHash + DHT_ID_LENGTH);

  auto dctx = std::make_shared<DownloadContext>();
  dctx->setAttribute(CTX_ATTR_BT, torrentAttrs);

  auto option = std::make_shared<Option>();
  option->put(PREF_BT_EXTERNAL_IP, "192.168.0.1");

  auto gid = GroupId::create();
  RequestGroup group(gid, option);
  dctx->setOwnerRequestGroup(&group);

  BtRegistry btReg;
  btReg.put(
      gid->getNumericId(),
      make_unique<BtObject>(dctx, nullptr, nullptr, nullptr, nullptr, nullptr));
  btReg.setTcpPort(6890);

  DHTGetPeersMessage msg(localNode_, remoteNode_, infoHash, transactionID);
  msg.setRoutingTable(&routingTable);
  msg.setTokenTracker(&tokenTracker);
  msg.setMessageDispatcher(&dispatcher);
  msg.setMessageFactory(&factory);
  msg.setBtRegistry(&btReg);
  msg.setFamily(AF_INET);
  {
    // localhost has peer contact information for that infohash.
    DHTPeerAnnounceStorage peerAnnounceStorage;
    peerAnnounceStorage.addPeerAnnounce(infoHash, "192.168.0.100", 6888);
    peerAnnounceStorage.addPeerAnnounce(infoHash, "192.168.0.101", 6889);

    msg.setPeerAnnounceStorage(&peerAnnounceStorage);

    msg.doReceivedAction();

    REQUIRE_EQ((size_t)1, dispatcher.messageQueue_.size());
    auto m = dynamic_cast<DHTGetPeersReplyMessage*>(
        dispatcher.messageQueue_[0].message_.get());
    REQUIRE(*localNode_ == *m->getLocalNode());
    REQUIRE(*remoteNode_ == *m->getRemoteNode());
    REQUIRE_EQ(std::string("get_peers"), m->getMessageType());
    REQUIRE_EQ(msg.getTransactionID(), m->getTransactionID());
    REQUIRE_EQ(tokenTracker.generateToken(infoHash,
                                                    remoteNode_->getIPAddress(),
                                                    remoteNode_->getPort()),
                         m->getToken());
    REQUIRE_EQ((size_t)0, m->getClosestKNodes().size());
    REQUIRE_EQ((size_t)3, m->getValues().size());
    {
      auto peer = m->getValues()[0];
      REQUIRE_EQ(std::string("192.168.0.100"), peer->getIPAddress());
      REQUIRE_EQ((uint16_t)6888, peer->getPort());
    }
    {
      auto peer = m->getValues()[1];
      REQUIRE_EQ(std::string("192.168.0.101"), peer->getIPAddress());
      REQUIRE_EQ((uint16_t)6889, peer->getPort());
    }
    {
      auto peer = m->getValues()[2];
      REQUIRE_EQ(std::string("192.168.0.1"), peer->getIPAddress());
      REQUIRE_EQ((uint16_t)6890, peer->getPort());
    }
  }
  msg.setBtRegistry(nullptr);
  dispatcher.messageQueue_.clear();
  {
    // localhost doesn't have peer contact information for that infohash.
    DHTPeerAnnounceStorage peerAnnounceStorage;
    DHTRoutingTable routingTable(localNode_);
    std::shared_ptr<DHTNode> returnNode1(new DHTNode());
    routingTable.addNode(returnNode1);

    msg.setPeerAnnounceStorage(&peerAnnounceStorage);
    msg.setRoutingTable(&routingTable);

    msg.doReceivedAction();

    REQUIRE_EQ((size_t)1, dispatcher.messageQueue_.size());
    auto m = dynamic_cast<DHTGetPeersReplyMessage*>(
        dispatcher.messageQueue_[0].message_.get());
    REQUIRE(*localNode_ == *m->getLocalNode());
    REQUIRE(*remoteNode_ == *m->getRemoteNode());
    REQUIRE_EQ(std::string("get_peers"), m->getMessageType());
    REQUIRE_EQ(msg.getTransactionID(), m->getTransactionID());
    REQUIRE_EQ(tokenTracker.generateToken(infoHash,
                                                    remoteNode_->getIPAddress(),
                                                    remoteNode_->getPort()),
                         m->getToken());
    REQUIRE_EQ((size_t)1, m->getClosestKNodes().size());
    REQUIRE(*returnNode1 == *m->getClosestKNodes()[0]);
    REQUIRE_EQ((size_t)0, m->getValues().size());
  }
}

} // namespace aria2
