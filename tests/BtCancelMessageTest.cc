#include "BtCancelMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "MockBtMessageDispatcher.h"
#include "Peer.h"
#include "FileEntry.h"
#include "Piece.h"

namespace aria2 {

class BtCancelMessageTest {


private:
  std::shared_ptr<Peer> peer;

public:
  void setUp() { peer.reset(new Peer("host", 6969)); }

  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();

  class MockBtMessageDispatcher2 : public MockBtMessageDispatcher {
  public:
    size_t index;
    int32_t begin;
    int32_t length;

  public:
    MockBtMessageDispatcher2() : index(0), begin(0), length(0) {}

    virtual void doCancelSendingPieceAction(size_t index, int32_t begin,
                                            int32_t length) override
    {
      this->index = index;
      this->begin = begin;
      this->length = length;
    }
  };
};

A2_TEST(BtCancelMessageTest, testCreate)
A2_TEST(BtCancelMessageTest, testCreateMessage)
A2_TEST(BtCancelMessageTest, testDoReceivedAction)

void BtCancelMessageTest::testCreate()
{
  unsigned char msg[17];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 13, 8);
  bittorrent::setIntParam(&msg[5], 12345);
  bittorrent::setIntParam(&msg[9], 256);
  bittorrent::setIntParam(&msg[13], 1_k);
  auto pm = BtCancelMessage::create(&msg[4], 13);
  REQUIRE_EQ((uint8_t)8, pm->getId());
  REQUIRE_EQ((size_t)12345, pm->getIndex());
  REQUIRE_EQ(256, pm->getBegin());
  REQUIRE_EQ((int32_t)1_k, pm->getLength());

  // case: payload size is wrong
  try {
    unsigned char msg[18];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 14, 8);
    BtCancelMessage::create(&msg[4], 14);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[17];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 13, 9);
    BtCancelMessage::create(&msg[4], 13);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtCancelMessageTest::testCreateMessage()
{
  BtCancelMessage msg;
  msg.setIndex(12345);
  msg.setBegin(256);
  msg.setLength(1_k);
  unsigned char data[17];
  bittorrent::createPeerMessageString(data, sizeof(data), 13, 8);
  bittorrent::setIntParam(&data[5], 12345);
  bittorrent::setIntParam(&data[9], 256);
  bittorrent::setIntParam(&data[13], 1_k);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)17, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtCancelMessageTest::testDoReceivedAction()
{
  BtCancelMessage msg;
  msg.setIndex(1);
  msg.setBegin(32_k);
  msg.setLength(16_k);
  msg.setPeer(peer);
  auto dispatcher = make_unique<MockBtMessageDispatcher2>();
  msg.setBtMessageDispatcher(dispatcher.get());

  msg.doReceivedAction();
  REQUIRE_EQ(msg.getIndex(), dispatcher->index);
  REQUIRE_EQ(msg.getBegin(), dispatcher->begin);
  REQUIRE_EQ(msg.getLength(), dispatcher->length);
}

} // namespace aria2
