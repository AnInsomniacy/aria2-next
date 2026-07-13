#include "BtHaveAllMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "Peer.h"
#include "MockPieceStorage.h"
#include "DlAbortEx.h"
#include "FileEntry.h"

namespace aria2 {

class BtHaveAllMessageTest {


private:
public:
  void setUp() {}

  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testDoReceivedAction_goodByeSeeder();
};

A2_TEST(BtHaveAllMessageTest, testCreate)
A2_TEST(BtHaveAllMessageTest, testCreateMessage)
A2_TEST(BtHaveAllMessageTest, testDoReceivedAction)
A2_TEST(BtHaveAllMessageTest, testDoReceivedAction_goodByeSeeder)

void BtHaveAllMessageTest::testCreate()
{
  unsigned char msg[5];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 14);
  std::shared_ptr<BtHaveAllMessage> pm(BtHaveAllMessage::create(&msg[4], 1));
  REQUIRE_EQ((uint8_t)14, pm->getId());

  // case: payload size is wrong
  try {
    unsigned char msg[6];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 2, 14);
    BtHaveAllMessage::create(&msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[5];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 15);
    BtHaveAllMessage::create(&msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtHaveAllMessageTest::testCreateMessage()
{
  BtHaveAllMessage msg;
  unsigned char data[5];
  bittorrent::createPeerMessageString(data, sizeof(data), 1, 14);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)5, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtHaveAllMessageTest::testDoReceivedAction()
{
  BtHaveAllMessage msg;
  std::shared_ptr<Peer> peer(new Peer("host", 6969));
  peer->allocateSessionResource(16_k, 256_k);
  peer->setFastExtensionEnabled(true);
  msg.setPeer(peer);
  auto pieceStorage = make_unique<MockPieceStorage>();
  msg.setPieceStorage(pieceStorage.get());

  msg.doReceivedAction();

  REQUIRE(peer->isSeeder());

  peer->setFastExtensionEnabled(false);

  try {
    msg.doReceivedAction();
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtHaveAllMessageTest::testDoReceivedAction_goodByeSeeder()
{
  BtHaveAllMessage msg;
  std::shared_ptr<Peer> peer(new Peer("ip", 6000));
  peer->allocateSessionResource(1_k, 1_k);
  peer->setFastExtensionEnabled(true);
  msg.setPeer(peer);
  auto pieceStorage = make_unique<MockPieceStorage>();
  msg.setPieceStorage(pieceStorage.get());

  pieceStorage->setDownloadFinished(true);

  try {
    msg.doReceivedAction();
    FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    // success
  }
}

} // namespace aria2
