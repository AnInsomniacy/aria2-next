#include "BtInterestedMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "Peer.h"
#include "MockPeerStorage.h"
#include "SocketBuffer.h"

namespace aria2 {

class BtInterestedMessageTest {


public:
  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testToString();
};

A2_TEST(BtInterestedMessageTest, testCreate)
A2_TEST(BtInterestedMessageTest, testCreateMessage)
A2_TEST(BtInterestedMessageTest, testDoReceivedAction)
A2_TEST(BtInterestedMessageTest, testToString)

void BtInterestedMessageTest::testCreate()
{
  unsigned char msg[5];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 2);
  std::shared_ptr<BtInterestedMessage> pm(
      BtInterestedMessage::create(&msg[4], 1));
  REQUIRE_EQ((uint8_t)2, pm->getId());

  // case: payload size is wrong
  try {
    unsigned char msg[6];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 2, 2);
    BtInterestedMessage::create(&msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[5];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 3);
    BtInterestedMessage::create(&msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtInterestedMessageTest::testCreateMessage()
{
  BtInterestedMessage msg;
  unsigned char data[5];
  bittorrent::createPeerMessageString(data, sizeof(data), 1, 2);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)5, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtInterestedMessageTest::testDoReceivedAction()
{
  BtInterestedMessage msg;
  std::shared_ptr<Peer> peer(new Peer("host", 6969));
  peer->allocateSessionResource(1_k, 1_m);
  msg.setPeer(peer);

  auto peerStorage = make_unique<MockPeerStorage>();

  msg.setPeerStorage(peerStorage.get());

  REQUIRE(!peer->peerInterested());
  msg.doReceivedAction();
  REQUIRE(peer->peerInterested());
  REQUIRE_EQ(1, peerStorage->getNumChokeExecuted());

  peer->amChoking(false);
  msg.doReceivedAction();
  REQUIRE_EQ(1, peerStorage->getNumChokeExecuted());
}

void BtInterestedMessageTest::testToString()
{
  BtInterestedMessage msg;
  REQUIRE_EQ(std::string("interested"), msg.toString());
}

} // namespace aria2
