#include "BtNotInterestedMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "Peer.h"
#include "MockPeerStorage.h"
#include "SocketBuffer.h"

namespace aria2 {

class BtNotInterestedMessageTest {


public:
  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testToString();
};

A2_TEST(BtNotInterestedMessageTest, testCreate)
A2_TEST(BtNotInterestedMessageTest, testCreateMessage)
A2_TEST(BtNotInterestedMessageTest, testDoReceivedAction)
A2_TEST(BtNotInterestedMessageTest, testToString)

void BtNotInterestedMessageTest::testCreate()
{
  unsigned char msg[5];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 3);
  std::shared_ptr<BtNotInterestedMessage> pm(
      BtNotInterestedMessage::create(&msg[4], 1));
  REQUIRE_EQ((uint8_t)3, pm->getId());

  // case: payload size is wrong
  try {
    unsigned char msg[6];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 2, 3);
    BtNotInterestedMessage::create(&msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[5];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 4);
    BtNotInterestedMessage::create(&msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtNotInterestedMessageTest::testCreateMessage()
{
  BtNotInterestedMessage msg;
  unsigned char data[5];
  bittorrent::createPeerMessageString(data, sizeof(data), 1, 3);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)5, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtNotInterestedMessageTest::testDoReceivedAction()
{
  std::shared_ptr<Peer> peer(new Peer("host", 6969));
  peer->allocateSessionResource(1_k, 1_m);
  peer->peerInterested(true);

  auto peerStorage = make_unique<MockPeerStorage>();

  BtNotInterestedMessage msg;
  msg.setPeer(peer);
  msg.setPeerStorage(peerStorage.get());

  REQUIRE(peer->peerInterested());
  msg.doReceivedAction();
  REQUIRE(!peer->peerInterested());
  REQUIRE_EQ(0, peerStorage->getNumChokeExecuted());

  peer->amChoking(false);
  msg.doReceivedAction();
  REQUIRE_EQ(1, peerStorage->getNumChokeExecuted());
}

void BtNotInterestedMessageTest::testToString()
{
  BtNotInterestedMessage msg;
  REQUIRE_EQ(std::string("not interested"), msg.toString());
}

} // namespace aria2
