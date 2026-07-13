#include "BtHaveNoneMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "Peer.h"

namespace aria2 {

class BtHaveNoneMessageTest {


private:
public:
  void setUp() {}

  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testToString();
};

A2_TEST(BtHaveNoneMessageTest, testCreate)
A2_TEST(BtHaveNoneMessageTest, testCreateMessage)
A2_TEST(BtHaveNoneMessageTest, testDoReceivedAction)
A2_TEST(BtHaveNoneMessageTest, testToString)

void BtHaveNoneMessageTest::testCreate()
{
  unsigned char msg[5];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 15);
  std::shared_ptr<BtHaveNoneMessage> pm(BtHaveNoneMessage::create(&msg[4], 1));
  REQUIRE_EQ((uint8_t)15, pm->getId());

  // case: payload size is wrong
  try {
    unsigned char msg[6];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 2, 15);
    BtHaveNoneMessage::create(&msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[5];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 16);
    BtHaveNoneMessage::create(&msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtHaveNoneMessageTest::testCreateMessage()
{
  BtHaveNoneMessage msg;
  unsigned char data[5];
  bittorrent::createPeerMessageString(data, sizeof(data), 1, 15);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)5, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtHaveNoneMessageTest::testDoReceivedAction()
{
  BtHaveNoneMessage msg;
  std::shared_ptr<Peer> peer(new Peer("host", 6969));
  peer->allocateSessionResource(1_k, 1_m);
  peer->setFastExtensionEnabled(true);
  msg.setPeer(peer);
  msg.doReceivedAction();

  peer->setFastExtensionEnabled(false);
  try {
    msg.doReceivedAction();
    FAIL("an exception must be thrown.");
  }
  catch (...) {
  }
}

void BtHaveNoneMessageTest::testToString()
{
  BtHaveNoneMessage msg;
  REQUIRE_EQ(std::string("have none"), msg.toString());
}

} // namespace aria2
