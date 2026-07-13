#include "BtAllowedFastMessage.h"

#include <cstring>
#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "util.h"
#include "Peer.h"
#include "SocketBuffer.h"

namespace aria2 {

class BtAllowedFastMessageTest {


private:
public:
  void setUp() {}

  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testOnSendComplete();
  void testToString();
};

A2_TEST(BtAllowedFastMessageTest, testCreate)
A2_TEST(BtAllowedFastMessageTest, testCreateMessage)
A2_TEST(BtAllowedFastMessageTest, testDoReceivedAction)
A2_TEST(BtAllowedFastMessageTest, testOnSendComplete)
A2_TEST(BtAllowedFastMessageTest, testToString)

void BtAllowedFastMessageTest::testCreate()
{
  unsigned char msg[9];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 5, 17);
  bittorrent::setIntParam(&msg[5], 12345);
  std::shared_ptr<BtAllowedFastMessage> pm(
      BtAllowedFastMessage::create(&msg[4], 5));
  REQUIRE_EQ((uint8_t)17, pm->getId());
  REQUIRE_EQ((size_t)12345, pm->getIndex());

  // case: payload size is wrong
  try {
    unsigned char msg[10];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 6, 17);
    BtAllowedFastMessage::create(&msg[4], 6);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[9];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 5, 18);
    BtAllowedFastMessage::create(&msg[4], 5);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtAllowedFastMessageTest::testCreateMessage()
{
  BtAllowedFastMessage msg;
  msg.setIndex(12345);
  unsigned char data[9];
  bittorrent::createPeerMessageString(data, sizeof(data), 5, 17);
  bittorrent::setIntParam(&data[5], 12345);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)9, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtAllowedFastMessageTest::testDoReceivedAction()
{
  BtAllowedFastMessage msg;
  msg.setIndex(1);
  std::shared_ptr<Peer> peer(new Peer("localhost", 6969));
  peer->allocateSessionResource(1_k, 1_m);
  peer->setFastExtensionEnabled(true);
  msg.setPeer(peer);
  REQUIRE(!peer->isInPeerAllowedIndexSet(1));
  msg.doReceivedAction();
  REQUIRE(peer->isInPeerAllowedIndexSet(1));

  peer->setFastExtensionEnabled(false);
  try {
    msg.doReceivedAction();
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtAllowedFastMessageTest::testOnSendComplete()
{
  BtAllowedFastMessage msg;
  msg.setIndex(1);
  std::shared_ptr<Peer> peer(new Peer("localhost", 6969));
  peer->allocateSessionResource(1_k, 1_m);
  peer->setFastExtensionEnabled(true);
  msg.setPeer(peer);
  REQUIRE(!peer->isInAmAllowedIndexSet(1));
  std::shared_ptr<ProgressUpdate> pu(msg.getProgressUpdate());
  pu->update(0, true);
  REQUIRE(peer->isInAmAllowedIndexSet(1));
}

void BtAllowedFastMessageTest::testToString()
{
  BtAllowedFastMessage msg;
  msg.setIndex(1);
  REQUIRE_EQ(std::string("allowed fast index=1"), msg.toString());
}

} // namespace aria2
