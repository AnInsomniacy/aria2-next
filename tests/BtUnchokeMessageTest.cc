#include "BtUnchokeMessage.h"

#include <cstring>
#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "Peer.h"
#include "SocketBuffer.h"

namespace aria2 {

class BtUnchokeMessageTest {


private:
public:
  void setUp() {}

  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testToString();
};

A2_TEST(BtUnchokeMessageTest, testCreate)
A2_TEST(BtUnchokeMessageTest, testCreateMessage)
A2_TEST(BtUnchokeMessageTest, testDoReceivedAction)
A2_TEST(BtUnchokeMessageTest, testToString)

void BtUnchokeMessageTest::testCreate()
{
  unsigned char msg[5];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 1);
  std::shared_ptr<BtUnchokeMessage> pm(BtUnchokeMessage::create(&msg[4], 1));
  REQUIRE_EQ((uint8_t)1, pm->getId());

  // case: payload size is wrong
  try {
    unsigned char msg[6];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 2, 1);
    BtUnchokeMessage::create(&msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[5];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 2);
    BtUnchokeMessage::create(&msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtUnchokeMessageTest::testCreateMessage()
{
  BtUnchokeMessage msg;
  unsigned char data[5];
  bittorrent::createPeerMessageString(data, sizeof(data), 1, 1);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)5, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtUnchokeMessageTest::testDoReceivedAction()
{
  std::shared_ptr<Peer> peer(new Peer("host", 6969));
  peer->allocateSessionResource(1_k, 1_m);
  peer->peerChoking(true);
  BtUnchokeMessage msg;
  msg.setPeer(peer);

  REQUIRE(peer->peerChoking());
  msg.doReceivedAction();
  REQUIRE(!peer->peerChoking());
}

void BtUnchokeMessageTest::testToString()
{
  BtUnchokeMessage msg;
  REQUIRE_EQ(std::string("unchoke"), msg.toString());
}

} // namespace aria2
