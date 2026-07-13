#include "BtHaveMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "Peer.h"
#include "MockPieceStorage.h"
#include "DlAbortEx.h"
#include "FileEntry.h"

namespace aria2 {

class BtHaveMessageTest {


private:
public:
  void setUp() {}

  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testDoReceivedAction_goodByeSeeder();
  void testToString();
};

A2_TEST(BtHaveMessageTest, testCreate)
A2_TEST(BtHaveMessageTest, testCreateMessage)
A2_TEST(BtHaveMessageTest, testDoReceivedAction)
A2_TEST(BtHaveMessageTest, testDoReceivedAction_goodByeSeeder)
A2_TEST(BtHaveMessageTest, testToString)

void BtHaveMessageTest::testCreate()
{
  unsigned char msg[9];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 5, 4);
  bittorrent::setIntParam(&msg[5], 12345);
  std::shared_ptr<BtHaveMessage> pm(BtHaveMessage::create(&msg[4], 5));
  REQUIRE_EQ((uint8_t)4, pm->getId());
  REQUIRE_EQ((size_t)12345, pm->getIndex());

  // case: payload size is wrong
  try {
    unsigned char msg[10];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 6, 4);
    BtHaveMessage::create(&msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[9];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 5, 5);
    BtHaveMessage::create(&msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtHaveMessageTest::testCreateMessage()
{
  BtHaveMessage msg;
  msg.setIndex(12345);
  unsigned char data[9];
  bittorrent::createPeerMessageString(data, sizeof(data), 5, 4);
  bittorrent::setIntParam(&data[5], 12345);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)9, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtHaveMessageTest::testDoReceivedAction()
{
  std::shared_ptr<Peer> peer(new Peer("host", 6969));
  peer->allocateSessionResource(16_k, 256_k);
  BtHaveMessage msg;
  msg.setIndex(1);
  msg.setPeer(peer);
  auto pieceStorage = make_unique<MockPieceStorage>();
  msg.setPieceStorage(pieceStorage.get());

  REQUIRE(!peer->hasPiece(msg.getIndex()));

  msg.doReceivedAction();

  REQUIRE(peer->hasPiece(msg.getIndex()));
}

void BtHaveMessageTest::testDoReceivedAction_goodByeSeeder()
{
  std::shared_ptr<Peer> peer(new Peer("ip", 6000));
  peer->allocateSessionResource(1_k, 2_k);
  BtHaveMessage msg;
  msg.setIndex(0);
  msg.setPeer(peer);
  auto pieceStorage = make_unique<MockPieceStorage>();
  msg.setPieceStorage(pieceStorage.get());

  // peer is not seeder and client have not completed download
  msg.doReceivedAction();

  pieceStorage->setDownloadFinished(true);

  // client have completed download but, peer is not seeder
  msg.doReceivedAction();

  msg.setIndex(1);
  pieceStorage->setDownloadFinished(false);

  // peer is a seeder but client have not completed download
  msg.doReceivedAction();

  pieceStorage->setDownloadFinished(true);
  peer->updateBitfield(1, 0);
  try {
    msg.doReceivedAction();
    FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    // success
  }
}

void BtHaveMessageTest::testToString()
{
  BtHaveMessage msg;
  msg.setIndex(1);

  REQUIRE_EQ(std::string("have index=1"), msg.toString());
}

} // namespace aria2
