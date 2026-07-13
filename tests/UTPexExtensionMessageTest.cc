#include "UTPexExtensionMessage.h"

#include <iostream>

#include "a2doctest.h"

#include "TestUtil.h"
#include "Peer.h"
#include "a2netcompat.h"
#include "util.h"
#include "bittorrent_helper.h"
#include "MockPeerStorage.h"
#include "Exception.h"
#include "FileEntry.h"
#include "wallclock.h"

namespace aria2 {

class UTPexExtensionMessageTest {


private:
  std::unique_ptr<MockPeerStorage> peerStorage_;

public:
  void setUp()
  {
    peerStorage_ = make_unique<MockPeerStorage>();
    global::wallclock().reset();
  }

  void testGetExtensionMessageID();
  void testGetExtensionName();
  void testGetBencodedData();
  void testToString();
  void testDoReceivedAction();
  void testCreate();
  void testAddFreshPeer();
  void testAddDroppedPeer();
  void testFreshPeersAreFull();
  void testDroppedPeersAreFull();
};

A2_TEST(UTPexExtensionMessageTest, testGetExtensionMessageID)
A2_TEST(UTPexExtensionMessageTest, testGetExtensionName)
A2_TEST(UTPexExtensionMessageTest, testGetBencodedData)
A2_TEST(UTPexExtensionMessageTest, testToString)
A2_TEST(UTPexExtensionMessageTest, testDoReceivedAction)
A2_TEST(UTPexExtensionMessageTest, testCreate)
A2_TEST(UTPexExtensionMessageTest, testAddFreshPeer)
A2_TEST(UTPexExtensionMessageTest, testAddDroppedPeer)
A2_TEST(UTPexExtensionMessageTest, testFreshPeersAreFull)
A2_TEST(UTPexExtensionMessageTest, testDroppedPeersAreFull)

void UTPexExtensionMessageTest::testGetExtensionMessageID()
{
  UTPexExtensionMessage msg(1);
  REQUIRE_EQ((uint8_t)1, msg.getExtensionMessageID());
}

void UTPexExtensionMessageTest::testGetExtensionName()
{
  UTPexExtensionMessage msg(1);
  REQUIRE_EQ(std::string("ut_pex"),
                       std::string(msg.getExtensionName()));
}

void UTPexExtensionMessageTest::testGetBencodedData()
{
  UTPexExtensionMessage msg(1);
  auto p1 = std::make_shared<Peer>("192.168.0.1", 6881);
  p1->allocateSessionResource(256_k, 1_m);
  p1->setAllBitfield();
  REQUIRE(msg.addFreshPeer(p1)); // added seeder, check add.f flag
  auto p2 = std::make_shared<Peer>("10.1.1.2", 9999);
  REQUIRE(msg.addFreshPeer(p2));
  auto p3 = std::make_shared<Peer>("192.168.0.2", 6882);
  p3->startDrop();
  REQUIRE(msg.addDroppedPeer(p3));
  auto p4 = std::make_shared<Peer>("10.1.1.3", 10000);
  p4->startDrop();
  REQUIRE(msg.addDroppedPeer(p4));

  auto p5 =
      std::make_shared<Peer>("1002:1035:4527:3546:7854:1237:3247:3217", 6881);
  REQUIRE(msg.addFreshPeer(p5));
  auto p6 = std::make_shared<Peer>("2001:db8:bd05:1d2:288a:1fc0:1:10ee", 6882);
  p6->startDrop();
  REQUIRE(msg.addDroppedPeer(p6));

  unsigned char c1[COMPACT_LEN_IPV6];
  unsigned char c2[COMPACT_LEN_IPV6];
  unsigned char c3[COMPACT_LEN_IPV6];
  unsigned char c4[COMPACT_LEN_IPV6];
  unsigned char c5[COMPACT_LEN_IPV6];
  unsigned char c6[COMPACT_LEN_IPV6];
  bittorrent::packcompact(c1, p1->getIPAddress(), p1->getPort());
  bittorrent::packcompact(c2, p2->getIPAddress(), p2->getPort());
  bittorrent::packcompact(c3, p3->getIPAddress(), p3->getPort());
  bittorrent::packcompact(c4, p4->getIPAddress(), p4->getPort());
  bittorrent::packcompact(c5, p5->getIPAddress(), p5->getPort());
  bittorrent::packcompact(c6, p6->getIPAddress(), p6->getPort());

  std::string expected =
      "d5:added12:" + std::string(&c1[0], &c1[6]) +
      std::string(&c2[0], &c2[6]) + "7:added.f2:" + fromHex("0200") +
      "6:added618:" + std::string(&c5[0], &c5[COMPACT_LEN_IPV6]) +
      "8:added6.f1:" + fromHex("00") +
      "7:dropped12:" + std::string(&c3[0], &c3[6]) +
      std::string(&c4[0], &c4[6]) +
      "8:dropped618:" + std::string(&c6[0], &c6[COMPACT_LEN_IPV6]) + "e";
  std::string bd = msg.getPayload();
  REQUIRE_EQ(util::percentEncode(expected), util::percentEncode(bd));
}

void UTPexExtensionMessageTest::testToString()
{
  UTPexExtensionMessage msg(1);
  std::shared_ptr<Peer> p1(new Peer("192.168.0.1", 6881));
  p1->allocateSessionResource(256_k, 1_m);
  p1->setAllBitfield();
  msg.addFreshPeer(p1); // added seeder, check add.f flag
  std::shared_ptr<Peer> p2(new Peer("10.1.1.2", 9999));
  msg.addFreshPeer(p2);
  std::shared_ptr<Peer> p3(new Peer("192.168.0.2", 6882));
  p3->startDrop();
  msg.addDroppedPeer(p3);
  std::shared_ptr<Peer> p4(new Peer("10.1.1.3", 10000));
  p4->startDrop();
  msg.addDroppedPeer(p4);
  REQUIRE_EQ(std::string("ut_pex added=2, dropped=2"),
                       msg.toString());
}

void UTPexExtensionMessageTest::testDoReceivedAction()
{
  UTPexExtensionMessage msg(1);
  std::shared_ptr<Peer> p1(new Peer("192.168.0.1", 6881));
  p1->allocateSessionResource(256_k, 1_m);
  p1->setAllBitfield();
  msg.addFreshPeer(p1); // added seeder, check add.f flag
  std::shared_ptr<Peer> p2(
      new Peer("1002:1035:4527:3546:7854:1237:3247:3217", 9999));
  msg.addFreshPeer(p2);
  std::shared_ptr<Peer> p3(new Peer("192.168.0.2", 6882));
  p3->startDrop();
  msg.addDroppedPeer(p3);
  std::shared_ptr<Peer> p4(
      new Peer("2001:db8:bd05:1d2:288a:1fc0:1:10ee", 10000));
  p4->startDrop();
  msg.addDroppedPeer(p4);
  msg.setPeerStorage(peerStorage_.get());

  msg.doReceivedAction();

  REQUIRE_EQ((size_t)4, peerStorage_->getUnusedPeers().size());
  {
    std::shared_ptr<Peer> p = peerStorage_->getUnusedPeers()[0];
    REQUIRE_EQ(std::string("192.168.0.1"), p->getIPAddress());
    REQUIRE_EQ((uint16_t)6881, p->getPort());
  }
  {
    std::shared_ptr<Peer> p = peerStorage_->getUnusedPeers()[1];
    REQUIRE_EQ(std::string("1002:1035:4527:3546:7854:1237:3247:3217"),
                         p->getIPAddress());
    REQUIRE_EQ((uint16_t)9999, p->getPort());
  }
  {
    std::shared_ptr<Peer> p = peerStorage_->getUnusedPeers()[2];
    REQUIRE_EQ(std::string("192.168.0.2"), p->getIPAddress());
  }
  {
    std::shared_ptr<Peer> p = peerStorage_->getUnusedPeers()[3];
    REQUIRE_EQ(std::string("2001:db8:bd05:1d2:288a:1fc0:1:10ee"),
                         p->getIPAddress());
  }
}

void UTPexExtensionMessageTest::testCreate()
{
  unsigned char c1[COMPACT_LEN_IPV6];
  unsigned char c2[COMPACT_LEN_IPV6];
  unsigned char c3[COMPACT_LEN_IPV6];
  unsigned char c4[COMPACT_LEN_IPV6];
  unsigned char c5[COMPACT_LEN_IPV6];
  unsigned char c6[COMPACT_LEN_IPV6];
  bittorrent::packcompact(c1, "192.168.0.1", 6881);
  bittorrent::packcompact(c2, "10.1.1.2", 9999);
  bittorrent::packcompact(c3, "192.168.0.2", 6882);
  bittorrent::packcompact(c4, "10.1.1.3", 10000);
  bittorrent::packcompact(c5, "1002:1035:4527:3546:7854:1237:3247:3217", 6997);
  bittorrent::packcompact(c6, "2001:db8:bd05:1d2:288a:1fc0:1:10ee", 6998);

  char id[1] = {1};

  std::string data =
      std::string(&id[0], &id[1]) +
      "d5:added12:" + std::string(&c1[0], &c1[6]) +
      std::string(&c2[0], &c2[6]) + "7:added.f2:" + fromHex("0200") +
      "6:added618:" + std::string(&c5[0], &c5[COMPACT_LEN_IPV6]) +
      "8:added6.f1:" + fromHex("00") +
      "7:dropped12:" + std::string(&c3[0], &c3[6]) +
      std::string(&c4[0], &c4[6]) +
      "8:dropped618:" + std::string(&c6[0], &c6[COMPACT_LEN_IPV6]) + "e";

  auto msg = UTPexExtensionMessage::create(
      reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
  REQUIRE_EQ((uint8_t)1, msg->getExtensionMessageID());
  REQUIRE_EQ((size_t)3, msg->getFreshPeers().size());
  REQUIRE_EQ(std::string("192.168.0.1"),
                       msg->getFreshPeers()[0]->getIPAddress());
  REQUIRE_EQ((uint16_t)6881, msg->getFreshPeers()[0]->getPort());
  REQUIRE_EQ(std::string("10.1.1.2"),
                       msg->getFreshPeers()[1]->getIPAddress());
  REQUIRE_EQ((uint16_t)9999, msg->getFreshPeers()[1]->getPort());
  REQUIRE_EQ(std::string("1002:1035:4527:3546:7854:1237:3247:3217"),
                       msg->getFreshPeers()[2]->getIPAddress());
  REQUIRE_EQ((uint16_t)6997, msg->getFreshPeers()[2]->getPort());

  REQUIRE_EQ((size_t)3, msg->getDroppedPeers().size());
  REQUIRE_EQ(std::string("192.168.0.2"),
                       msg->getDroppedPeers()[0]->getIPAddress());
  REQUIRE_EQ((uint16_t)6882, msg->getDroppedPeers()[0]->getPort());
  REQUIRE_EQ(std::string("10.1.1.3"),
                       msg->getDroppedPeers()[1]->getIPAddress());
  REQUIRE_EQ((uint16_t)10000, msg->getDroppedPeers()[1]->getPort());
  REQUIRE_EQ(std::string("2001:db8:bd05:1d2:288a:1fc0:1:10ee"),
                       msg->getDroppedPeers()[2]->getIPAddress());
  REQUIRE_EQ((uint16_t)6998, msg->getDroppedPeers()[2]->getPort());
  try {
    // 0 length data
    std::string in = "";
    UTPexExtensionMessage::create(
        reinterpret_cast<const unsigned char*>(in.c_str()), in.size());
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

void UTPexExtensionMessageTest::testAddFreshPeer()
{
  UTPexExtensionMessage msg(1);
  std::shared_ptr<Peer> p1(new Peer("192.168.0.1", 6881));
  REQUIRE(msg.addFreshPeer(p1));
  std::shared_ptr<Peer> p2(new Peer("10.1.1.2", 9999));
  p2->setFirstContactTime(Timer(Timer().getTime() - 61_s));
  REQUIRE(!msg.addFreshPeer(p2));
  std::shared_ptr<Peer> p3(new Peer("10.1.1.3", 9999, true));
  REQUIRE(!msg.addFreshPeer(p3));
}

void UTPexExtensionMessageTest::testAddDroppedPeer()
{
  UTPexExtensionMessage msg(1);
  std::shared_ptr<Peer> p1(new Peer("192.168.0.1", 6881));
  REQUIRE(!msg.addDroppedPeer(p1));
  std::shared_ptr<Peer> p2(new Peer("10.1.1.2", 9999));
  p2->startDrop();
  REQUIRE(msg.addFreshPeer(p2));
  std::shared_ptr<Peer> p3(new Peer("10.1.1.3", 9999, true));
  p3->startDrop();
  REQUIRE(!msg.addDroppedPeer(p3));
}

void UTPexExtensionMessageTest::testFreshPeersAreFull()
{
  UTPexExtensionMessage msg(1);
  REQUIRE_EQ((size_t)50, msg.getMaxFreshPeer());
  msg.setMaxFreshPeer(2);
  std::shared_ptr<Peer> p1(new Peer("192.168.0.1", 6881));
  REQUIRE(msg.addFreshPeer(p1));
  REQUIRE(!msg.freshPeersAreFull());
  std::shared_ptr<Peer> p2(new Peer("10.1.1.2", 9999));
  REQUIRE(msg.addFreshPeer(p2));
  REQUIRE(msg.freshPeersAreFull());
  std::shared_ptr<Peer> p3(new Peer("10.1.1.3", 9999));
  REQUIRE(msg.addFreshPeer(p3));
  REQUIRE(msg.freshPeersAreFull());
}

void UTPexExtensionMessageTest::testDroppedPeersAreFull()
{
  UTPexExtensionMessage msg(1);
  REQUIRE_EQ((size_t)50, msg.getMaxDroppedPeer());
  msg.setMaxDroppedPeer(2);
  std::shared_ptr<Peer> p1(new Peer("192.168.0.1", 6881));
  p1->startDrop();
  REQUIRE(msg.addDroppedPeer(p1));
  REQUIRE(!msg.droppedPeersAreFull());
  std::shared_ptr<Peer> p2(new Peer("10.1.1.2", 9999));
  p2->startDrop();
  REQUIRE(msg.addDroppedPeer(p2));
  REQUIRE(msg.droppedPeersAreFull());
  std::shared_ptr<Peer> p3(new Peer("10.1.1.3", 9999));
  p3->startDrop();
  REQUIRE(msg.addDroppedPeer(p3));
  REQUIRE(msg.droppedPeersAreFull());
}

} // namespace aria2
