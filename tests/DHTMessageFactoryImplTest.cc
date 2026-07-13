#include "DHTMessageFactoryImpl.h"

#include <cstring>
#include <iostream>

#include "a2doctest.h"

#include "RecoverableException.h"
#include "util.h"
#include "DHTNode.h"
#include "DHTRoutingTable.h"
#include "Peer.h"
#include "bittorrent_helper.h"
#include "DHTBucket.h"
#include "DHTPingMessage.h"
#include "DHTPingReplyMessage.h"
#include "DHTFindNodeMessage.h"
#include "DHTFindNodeReplyMessage.h"
#include "DHTGetPeersMessage.h"
#include "DHTGetPeersReplyMessage.h"
#include "DHTAnnouncePeerMessage.h"
#include "DHTAnnouncePeerReplyMessage.h"
#include "bencode2.h"

namespace aria2 {

class DHTMessageFactoryImplTest {


public:
  std::unique_ptr<DHTMessageFactoryImpl> factory;

  std::unique_ptr<DHTRoutingTable> routingTable;

  std::shared_ptr<DHTNode> localNode;

  std::unique_ptr<DHTNode> remoteNode_;
  std::unique_ptr<DHTNode> remoteNode6_;

  unsigned char transactionID[DHT_TRANSACTION_ID_LENGTH];

  unsigned char remoteNodeID[DHT_ID_LENGTH];

  void setUp()
  {
    localNode = std::make_shared<DHTNode>();
    factory = make_unique<DHTMessageFactoryImpl>(AF_INET);
    factory->setLocalNode(localNode);
    memset(transactionID, 0xff, DHT_TRANSACTION_ID_LENGTH);
    memset(remoteNodeID, 0x0f, DHT_ID_LENGTH);
    routingTable = make_unique<DHTRoutingTable>(localNode);
    factory->setRoutingTable(routingTable.get());

    remoteNode_ = make_unique<DHTNode>(remoteNodeID);
    remoteNode_->setIPAddress("192.168.0.1");
    remoteNode_->setPort(6881);

    remoteNode6_ = make_unique<DHTNode>(remoteNodeID);
    remoteNode6_->setIPAddress("2001::2001");
    remoteNode6_->setPort(6881);
  }

  void tearDown() {}

  void testCreatePingMessage();
  void testCreatePingReplyMessage();
  void testCreateFindNodeMessage();
  void testCreateFindNodeReplyMessage();
  void testCreateFindNodeReplyMessage6();
  void testCreateGetPeersMessage();
  void testCreateGetPeersReplyMessage();
  void testCreateGetPeersReplyMessage6();
  void testCreateAnnouncePeerMessage();
  void testCreateAnnouncePeerReplyMessage();
  void testReceivedErrorMessage();
};

A2_TEST(DHTMessageFactoryImplTest, testCreatePingMessage)
A2_TEST(DHTMessageFactoryImplTest, testCreatePingReplyMessage)
A2_TEST(DHTMessageFactoryImplTest, testCreateFindNodeMessage)
A2_TEST(DHTMessageFactoryImplTest, testCreateFindNodeReplyMessage)
A2_TEST(DHTMessageFactoryImplTest, testCreateFindNodeReplyMessage6)
A2_TEST(DHTMessageFactoryImplTest, testCreateGetPeersMessage)
A2_TEST(DHTMessageFactoryImplTest, testCreateGetPeersReplyMessage)
A2_TEST(DHTMessageFactoryImplTest, testCreateGetPeersReplyMessage6)
A2_TEST(DHTMessageFactoryImplTest, testCreateAnnouncePeerMessage)
A2_TEST(DHTMessageFactoryImplTest, testCreateAnnouncePeerReplyMessage)
A2_TEST(DHTMessageFactoryImplTest, testReceivedErrorMessage)

void DHTMessageFactoryImplTest::testCreatePingMessage()
{
  Dict dict;
  dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
  dict.put("y", "q");
  dict.put("q", "ping");
  auto aDict = Dict::g();
  aDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
  dict.put("a", std::move(aDict));

  auto r = factory->createQueryMessage(&dict, "192.168.0.1", 6881);
  auto m = dynamic_cast<DHTPingMessage*>(r.get());

  REQUIRE(*localNode == *m->getLocalNode());
  REQUIRE(*remoteNode_ == *m->getRemoteNode());
  REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                       util::toHex(m->getTransactionID()));
}

void DHTMessageFactoryImplTest::testCreatePingReplyMessage()
{
  Dict dict;
  dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
  dict.put("y", "r");
  auto rDict = Dict::g();
  rDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
  dict.put("r", std::move(rDict));

  auto r = factory->createResponseMessage(
      "ping", &dict, remoteNode_->getIPAddress(), remoteNode_->getPort());
  auto m = dynamic_cast<DHTPingReplyMessage*>(r.get());

  REQUIRE(*localNode == *m->getLocalNode());
  REQUIRE(*remoteNode_ == *m->getRemoteNode());
  REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                       util::toHex(m->getTransactionID()));
}

void DHTMessageFactoryImplTest::testCreateFindNodeMessage()
{
  Dict dict;
  dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
  dict.put("y", "q");
  dict.put("q", "find_node");
  auto aDict = Dict::g();
  aDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
  unsigned char targetNodeID[DHT_ID_LENGTH];
  memset(targetNodeID, 0x11, DHT_ID_LENGTH);
  aDict->put("target", String::g(targetNodeID, DHT_ID_LENGTH));
  dict.put("a", std::move(aDict));

  auto r = factory->createQueryMessage(&dict, "192.168.0.1", 6881);
  auto m = dynamic_cast<DHTFindNodeMessage*>(r.get());

  REQUIRE(*localNode == *m->getLocalNode());
  REQUIRE(*remoteNode_ == *m->getRemoteNode());
  REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                       util::toHex(m->getTransactionID()));
  REQUIRE_EQ(util::toHex(targetNodeID, DHT_ID_LENGTH),
                       util::toHex(m->getTargetNodeID(), DHT_ID_LENGTH));
}

void DHTMessageFactoryImplTest::testCreateFindNodeReplyMessage()
{
  try {
    Dict dict;
    dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
    dict.put("y", "r");
    auto rDict = Dict::g();
    rDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
    std::string compactNodeInfo;
    std::shared_ptr<DHTNode> nodes[8];
    for (size_t i = 0; i < DHTBucket::K; ++i) {
      nodes[i] = std::make_shared<DHTNode>();
      nodes[i]->setIPAddress("192.168.0." + util::uitos(i + 1));
      nodes[i]->setPort(6881 + i);

      unsigned char buf[COMPACT_LEN_IPV6];
      REQUIRE_EQ(COMPACT_LEN_IPV4, bittorrent::packcompact(
                                                 buf, nodes[i]->getIPAddress(),
                                                 nodes[i]->getPort()));
      compactNodeInfo += std::string(&nodes[i]->getID()[0],
                                     &nodes[i]->getID()[DHT_ID_LENGTH]) +
                         std::string(&buf[0], &buf[COMPACT_LEN_IPV4]);
    }
    rDict->put("nodes", compactNodeInfo);
    dict.put("r", std::move(rDict));

    auto r = factory->createResponseMessage("find_node", &dict,
                                            remoteNode_->getIPAddress(),
                                            remoteNode_->getPort());
    auto m = dynamic_cast<DHTFindNodeReplyMessage*>(r.get());

    REQUIRE(*localNode == *m->getLocalNode());
    REQUIRE(*remoteNode_ == *m->getRemoteNode());
    REQUIRE_EQ((size_t)DHTBucket::K, m->getClosestKNodes().size());
    REQUIRE(*nodes[0] == *m->getClosestKNodes()[0]);
    REQUIRE(*nodes[7] == *m->getClosestKNodes()[7]);
    REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                         util::toHex(m->getTransactionID()));
  }
  catch (Exception& e) {
    FAIL(e.stackTrace());
  }
}

void DHTMessageFactoryImplTest::testCreateFindNodeReplyMessage6()
{
  factory = make_unique<DHTMessageFactoryImpl>(AF_INET6);
  factory->setLocalNode(localNode);
  factory->setRoutingTable(routingTable.get());
  try {
    Dict dict;
    dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
    dict.put("y", "r");
    auto rDict = Dict::g();
    rDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
    std::string compactNodeInfo;
    std::shared_ptr<DHTNode> nodes[8];
    for (size_t i = 0; i < DHTBucket::K; ++i) {
      nodes[i] = std::make_shared<DHTNode>();
      nodes[i]->setIPAddress("2001::000" + util::uitos(i + 1));
      nodes[i]->setPort(6881 + i);

      unsigned char buf[COMPACT_LEN_IPV6];
      REQUIRE_EQ(COMPACT_LEN_IPV6, bittorrent::packcompact(
                                                 buf, nodes[i]->getIPAddress(),
                                                 nodes[i]->getPort()));
      compactNodeInfo += std::string(&nodes[i]->getID()[0],
                                     &nodes[i]->getID()[DHT_ID_LENGTH]) +
                         std::string(&buf[0], &buf[COMPACT_LEN_IPV6]);
    }
    rDict->put("nodes6", compactNodeInfo);
    dict.put("r", std::move(rDict));

    auto r = factory->createResponseMessage("find_node", &dict,
                                            remoteNode_->getIPAddress(),
                                            remoteNode_->getPort());
    auto m = dynamic_cast<DHTFindNodeReplyMessage*>(r.get());

    REQUIRE(*localNode == *m->getLocalNode());
    REQUIRE(*remoteNode_ == *m->getRemoteNode());
    REQUIRE_EQ((size_t)DHTBucket::K, m->getClosestKNodes().size());
    REQUIRE(*nodes[0] == *m->getClosestKNodes()[0]);
    REQUIRE(*nodes[7] == *m->getClosestKNodes()[7]);
    REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                         util::toHex(m->getTransactionID()));
  }
  catch (Exception& e) {
    FAIL(e.stackTrace());
  }
}

void DHTMessageFactoryImplTest::testCreateGetPeersMessage()
{
  Dict dict;
  dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
  dict.put("y", "q");
  dict.put("q", "get_peers");
  auto aDict = Dict::g();
  aDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
  unsigned char infoHash[DHT_ID_LENGTH];
  memset(infoHash, 0x11, DHT_ID_LENGTH);
  aDict->put("info_hash", String::g(infoHash, DHT_ID_LENGTH));
  dict.put("a", std::move(aDict));

  auto r = factory->createQueryMessage(&dict, "192.168.0.1", 6881);
  auto m = dynamic_cast<DHTGetPeersMessage*>(r.get());

  REQUIRE(*localNode == *m->getLocalNode());
  REQUIRE(*remoteNode_ == *m->getRemoteNode());
  REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                       util::toHex(m->getTransactionID()));
  REQUIRE_EQ(util::toHex(infoHash, DHT_ID_LENGTH),
                       util::toHex(m->getInfoHash(), DHT_ID_LENGTH));
}

void DHTMessageFactoryImplTest::testCreateGetPeersReplyMessage()
{
  try {
    Dict dict;
    dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
    dict.put("y", "r");
    auto rDict = Dict::g();
    rDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
    std::string compactNodeInfo;
    std::shared_ptr<DHTNode> nodes[8];
    for (size_t i = 0; i < DHTBucket::K; ++i) {
      nodes[i] = std::make_shared<DHTNode>();
      nodes[i]->setIPAddress("192.168.0." + util::uitos(i + 1));
      nodes[i]->setPort(6881 + i);

      unsigned char buf[COMPACT_LEN_IPV6];
      REQUIRE_EQ(COMPACT_LEN_IPV4, bittorrent::packcompact(
                                                 buf, nodes[i]->getIPAddress(),
                                                 nodes[i]->getPort()));
      compactNodeInfo += std::string(&nodes[i]->getID()[0],
                                     &nodes[i]->getID()[DHT_ID_LENGTH]) +
                         std::string(&buf[0], &buf[COMPACT_LEN_IPV4]);
    }
    rDict->put("nodes", compactNodeInfo);

    std::deque<std::shared_ptr<Peer>> peers;
    auto valuesList = List::g();
    for (size_t i = 0; i < 4; ++i) {
      auto peer =
          std::make_shared<Peer>("192.168.0." + util::uitos(i + 1), 6881 + i);
      unsigned char buffer[COMPACT_LEN_IPV6];
      REQUIRE_EQ(COMPACT_LEN_IPV4,
                           bittorrent::packcompact(buffer, peer->getIPAddress(),
                                                   peer->getPort()));
      valuesList->append(String::g(buffer, COMPACT_LEN_IPV4));
      peers.push_back(peer);
    }
    rDict->put("values", std::move(valuesList));
    rDict->put("token", "token");
    dict.put("r", std::move(rDict));

    auto r = factory->createResponseMessage("get_peers", &dict,
                                            remoteNode_->getIPAddress(),
                                            remoteNode_->getPort());
    auto m = dynamic_cast<DHTGetPeersReplyMessage*>(r.get());

    REQUIRE(*localNode == *m->getLocalNode());
    REQUIRE(*remoteNode_ == *m->getRemoteNode());
    REQUIRE_EQ(std::string("token"), m->getToken());
    REQUIRE_EQ((size_t)DHTBucket::K, m->getClosestKNodes().size());
    REQUIRE(*nodes[0] == *m->getClosestKNodes()[0]);
    REQUIRE(*nodes[7] == *m->getClosestKNodes()[7]);
    REQUIRE_EQ((size_t)4, m->getValues().size());
    for (int i = 0; i < 4; ++i) {
      REQUIRE_EQ(peers[i]->getIPAddress(),
                           m->getValues()[i]->getIPAddress());
      REQUIRE_EQ(peers[i]->getPort(), m->getValues()[i]->getPort());
    }
    REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                         util::toHex(m->getTransactionID()));
  }
  catch (Exception& e) {
    FAIL(e.stackTrace());
  }
}

void DHTMessageFactoryImplTest::testCreateGetPeersReplyMessage6()
{
  factory = make_unique<DHTMessageFactoryImpl>(AF_INET6);
  factory->setLocalNode(localNode);
  factory->setRoutingTable(routingTable.get());
  try {
    Dict dict;
    dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
    dict.put("y", "r");
    auto rDict = Dict::g();
    rDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
    std::string compactNodeInfo;
    std::shared_ptr<DHTNode> nodes[8];
    for (size_t i = 0; i < DHTBucket::K; ++i) {
      nodes[i] = std::make_shared<DHTNode>();
      nodes[i]->setIPAddress("2001::000" + util::uitos(i + 1));
      nodes[i]->setPort(6881 + i);

      unsigned char buf[COMPACT_LEN_IPV6];
      REQUIRE_EQ(COMPACT_LEN_IPV6, bittorrent::packcompact(
                                                 buf, nodes[i]->getIPAddress(),
                                                 nodes[i]->getPort()));
      compactNodeInfo += std::string(&nodes[i]->getID()[0],
                                     &nodes[i]->getID()[DHT_ID_LENGTH]) +
                         std::string(&buf[0], &buf[COMPACT_LEN_IPV6]);
    }
    rDict->put("nodes6", compactNodeInfo);

    std::deque<std::shared_ptr<Peer>> peers;
    auto valuesList = List::g();
    for (size_t i = 0; i < 4; ++i) {
      auto peer =
          std::make_shared<Peer>("2001::100" + util::uitos(i + 1), 6881 + i);
      unsigned char buffer[COMPACT_LEN_IPV6];
      REQUIRE_EQ(COMPACT_LEN_IPV6,
                           bittorrent::packcompact(buffer, peer->getIPAddress(),
                                                   peer->getPort()));
      valuesList->append(String::g(buffer, COMPACT_LEN_IPV6));
      peers.push_back(peer);
    }
    rDict->put("values", std::move(valuesList));
    rDict->put("token", "token");
    dict.put("r", std::move(rDict));

    auto r = factory->createResponseMessage("get_peers", &dict,
                                            remoteNode_->getIPAddress(),
                                            remoteNode_->getPort());
    auto m = dynamic_cast<DHTGetPeersReplyMessage*>(r.get());

    REQUIRE(*localNode == *m->getLocalNode());
    REQUIRE(*remoteNode_ == *m->getRemoteNode());
    REQUIRE_EQ(std::string("token"), m->getToken());
    REQUIRE_EQ((size_t)DHTBucket::K, m->getClosestKNodes().size());
    REQUIRE(*nodes[0] == *m->getClosestKNodes()[0]);
    REQUIRE(*nodes[7] == *m->getClosestKNodes()[7]);
    REQUIRE_EQ((size_t)4, m->getValues().size());
    for (int i = 0; i < 4; ++i) {
      REQUIRE_EQ(peers[i]->getIPAddress(),
                           m->getValues()[i]->getIPAddress());
      REQUIRE_EQ(peers[i]->getPort(), m->getValues()[i]->getPort());
    }
    REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                         util::toHex(m->getTransactionID()));
  }
  catch (Exception& e) {
    FAIL(e.stackTrace());
  }
}

void DHTMessageFactoryImplTest::testCreateAnnouncePeerMessage()
{
  try {
    Dict dict;
    dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
    dict.put("y", "q");
    dict.put("q", "announce_peer");
    auto aDict = Dict::g();
    aDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
    unsigned char infoHash[DHT_ID_LENGTH];
    memset(infoHash, 0x11, DHT_ID_LENGTH);
    aDict->put("info_hash", String::g(infoHash, DHT_ID_LENGTH));
    std::string token = "ffff";
    uint16_t port = 6881;
    aDict->put("port", Integer::g(port));
    aDict->put("token", token);
    dict.put("a", std::move(aDict));

    remoteNode_->setPort(6882);

    auto r = factory->createQueryMessage(&dict, "192.168.0.1", 6882);
    auto m = dynamic_cast<DHTAnnouncePeerMessage*>(r.get());

    REQUIRE(*localNode == *m->getLocalNode());
    REQUIRE(*remoteNode_ == *m->getRemoteNode());
    REQUIRE_EQ(token, m->getToken());
    REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                         util::toHex(m->getTransactionID()));
    REQUIRE_EQ(util::toHex(infoHash, DHT_ID_LENGTH),
                         util::toHex(m->getInfoHash(), DHT_ID_LENGTH));
    REQUIRE_EQ(port, m->getTCPPort());
  }
  catch (Exception& e) {
    FAIL(e.stackTrace());
  }
}

void DHTMessageFactoryImplTest::testCreateAnnouncePeerReplyMessage()
{
  Dict dict;
  dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
  dict.put("y", "r");
  auto rDict = Dict::g();
  rDict->put("id", String::g(remoteNodeID, DHT_ID_LENGTH));
  dict.put("r", std::move(rDict));

  auto r = factory->createResponseMessage("announce_peer", &dict,
                                          remoteNode_->getIPAddress(),
                                          remoteNode_->getPort());
  auto m = dynamic_cast<DHTAnnouncePeerReplyMessage*>(r.get());

  REQUIRE(*localNode == *m->getLocalNode());
  REQUIRE(*remoteNode_ == *m->getRemoteNode());
  REQUIRE_EQ(util::toHex(transactionID, DHT_TRANSACTION_ID_LENGTH),
                       util::toHex(m->getTransactionID()));
}

void DHTMessageFactoryImplTest::testReceivedErrorMessage()
{
  Dict dict;
  dict.put("t", String::g(transactionID, DHT_TRANSACTION_ID_LENGTH));
  dict.put("y", "e");
  auto list = List::g();
  list->append(Integer::g(404));
  list->append("Not found");
  dict.put("e", std::move(list));

  try {
    factory->createResponseMessage("announce_peer", &dict,
                                   remoteNode_->getIPAddress(),
                                   remoteNode_->getPort());
    FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

} // namespace aria2
