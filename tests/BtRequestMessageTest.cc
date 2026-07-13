#include "BtRequestMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "MockBtMessage.h"
#include "MockPieceStorage.h"
#include "MockBtMessageFactory.h"
#include "MockBtMessageDispatcher.h"
#include "BtAbortOutstandingRequestEvent.h"
#include "Peer.h"
#include "FileEntry.h"
#include "BtHandshakeMessage.h"
#include "RangeBtMessageValidator.h"
#include "DlAbortEx.h"

namespace aria2 {

class BtRequestMessageTest {


private:
public:
  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction_hasPieceAndAmNotChoking();
  void testDoReceivedAction_hasPieceAndAmChokingAndFastExtensionEnabled();
  void testDoReceivedAction_hasPieceAndAmChokingAndFastExtensionDisabled();
  void testDoReceivedAction_doesntHavePieceAndFastExtensionEnabled();
  void testDoReceivedAction_doesntHavePieceAndFastExtensionDisabled();
  void testHandleAbortRequestEvent();
  void testHandleAbortRequestEvent_indexNoMatch();
  void testHandleAbortRequestEvent_alreadyInvalidated();
  void testToString();
  void testValidate();
  void testValidate_lengthTooLong();

  class MockPieceStorage2 : public MockPieceStorage {
  public:
    virtual bool hasPiece(size_t index) override { return index == 1; }
  };

  class MockBtMessageFactory2 : public MockBtMessageFactory {
  public:
    virtual std::unique_ptr<BtPieceMessage>
    createPieceMessage(size_t index, int32_t begin,
                       int32_t length) override
    {
      return make_unique<BtPieceMessage>(index, begin, length);
    }

    virtual std::unique_ptr<BtRejectMessage>
    createRejectMessage(size_t index, int32_t begin,
                        int32_t length) override
    {
      return make_unique<BtRejectMessage>(index, begin, length);
    }
  };

  std::unique_ptr<MockPieceStorage> pieceStorage_;
  std::shared_ptr<Peer> peer_;
  std::unique_ptr<MockBtMessageDispatcher> dispatcher_;
  std::unique_ptr<MockBtMessageFactory> messageFactory_;
  std::unique_ptr<BtRequestMessage> msg;

  void setUp()
  {
    pieceStorage_ = make_unique<MockPieceStorage2>();

    peer_ = std::make_shared<Peer>("host", 6969);
    peer_->allocateSessionResource(16_k, 256_k);

    dispatcher_ = make_unique<MockBtMessageDispatcher>();

    messageFactory_ = make_unique<MockBtMessageFactory2>();

    msg = make_unique<BtRequestMessage>();
    msg->setPeer(peer_);
    msg->setIndex(1);
    msg->setBegin(16);
    msg->setLength(32);
    msg->setBlockIndex(2);
    msg->setBtMessageDispatcher(dispatcher_.get());
    msg->setBtMessageFactory(messageFactory_.get());
    msg->setPieceStorage(pieceStorage_.get());
  }
};

A2_TEST(BtRequestMessageTest, testCreate)
A2_TEST(BtRequestMessageTest, testCreateMessage)
A2_TEST(BtRequestMessageTest, testDoReceivedAction_hasPieceAndAmNotChoking)
A2_TEST(BtRequestMessageTest, testDoReceivedAction_hasPieceAndAmChokingAndFastExtensionEnabled)
A2_TEST(BtRequestMessageTest, testDoReceivedAction_hasPieceAndAmChokingAndFastExtensionDisabled)
A2_TEST(BtRequestMessageTest, testDoReceivedAction_doesntHavePieceAndFastExtensionEnabled)
A2_TEST(BtRequestMessageTest, testDoReceivedAction_doesntHavePieceAndFastExtensionDisabled)
A2_TEST(BtRequestMessageTest, testHandleAbortRequestEvent)
A2_TEST(BtRequestMessageTest, testHandleAbortRequestEvent_indexNoMatch)
A2_TEST(BtRequestMessageTest, testHandleAbortRequestEvent_alreadyInvalidated)
A2_TEST(BtRequestMessageTest, testToString)
A2_TEST(BtRequestMessageTest, testValidate)
A2_TEST(BtRequestMessageTest, testValidate_lengthTooLong)

void BtRequestMessageTest::testCreate()
{
  unsigned char msg[17];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 13, 6);
  bittorrent::setIntParam(&msg[5], 12345);
  bittorrent::setIntParam(&msg[9], 256);
  bittorrent::setIntParam(&msg[13], 1_k);
  auto pm = BtRequestMessage::create(&msg[4], 13);
  REQUIRE_EQ((uint8_t)6, pm->getId());
  REQUIRE_EQ((size_t)12345, pm->getIndex());
  REQUIRE_EQ(256, pm->getBegin());
  REQUIRE_EQ((int32_t)1_k, pm->getLength());

  // case: payload size is wrong
  try {
    unsigned char msg[18];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 14, 6);
    BtRequestMessage::create(&msg[4], 14);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[17];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 13, 7);
    BtRequestMessage::create(&msg[4], 13);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtRequestMessageTest::testCreateMessage()
{
  BtRequestMessage msg;
  msg.setIndex(12345);
  msg.setBegin(256);
  msg.setLength(1_k);
  unsigned char data[17];
  bittorrent::createPeerMessageString(data, sizeof(data), 13, 6);
  bittorrent::setIntParam(&data[5], 12345);
  bittorrent::setIntParam(&data[9], 256);
  bittorrent::setIntParam(&data[13], 1_k);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)17, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtRequestMessageTest::testDoReceivedAction_hasPieceAndAmNotChoking()
{
  peer_->amChoking(false);
  msg->doReceivedAction();

  REQUIRE_EQ((size_t)1, dispatcher_->messageQueue.size());
  REQUIRE(BtPieceMessage::ID ==
                 dispatcher_->messageQueue.front()->getId());
  auto pieceMsg = static_cast<const BtPieceMessage*>(
      dispatcher_->messageQueue.front().get());
  REQUIRE_EQ((size_t)1, pieceMsg->getIndex());
  REQUIRE_EQ((int32_t)16, pieceMsg->getBegin());
  REQUIRE_EQ((int32_t)32, pieceMsg->getBlockLength());
}

void BtRequestMessageTest::
    testDoReceivedAction_hasPieceAndAmChokingAndFastExtensionEnabled()
{
  peer_->amChoking(true);
  peer_->setFastExtensionEnabled(true);
  msg->doReceivedAction();

  REQUIRE_EQ((size_t)1, dispatcher_->messageQueue.size());
  REQUIRE(BtRejectMessage::ID ==
                 dispatcher_->messageQueue.front()->getId());
  auto rejectMsg = static_cast<const BtRejectMessage*>(
      dispatcher_->messageQueue.front().get());
  REQUIRE_EQ((size_t)1, rejectMsg->getIndex());
  REQUIRE_EQ((int32_t)16, rejectMsg->getBegin());
  REQUIRE_EQ((int32_t)32, rejectMsg->getLength());
}

void BtRequestMessageTest::
    testDoReceivedAction_hasPieceAndAmChokingAndFastExtensionDisabled()
{
  peer_->amChoking(true);
  msg->doReceivedAction();

  REQUIRE_EQ((size_t)0, dispatcher_->messageQueue.size());
}

void BtRequestMessageTest::
    testDoReceivedAction_doesntHavePieceAndFastExtensionEnabled()
{
  msg->setIndex(2);
  peer_->amChoking(false);
  peer_->setFastExtensionEnabled(true);
  msg->doReceivedAction();

  REQUIRE_EQ((size_t)1, dispatcher_->messageQueue.size());
  REQUIRE(BtRejectMessage::ID ==
                 dispatcher_->messageQueue.front()->getId());
  auto rejectMsg = static_cast<const BtRejectMessage*>(
      dispatcher_->messageQueue.front().get());
  REQUIRE_EQ((size_t)2, rejectMsg->getIndex());
  REQUIRE_EQ((int32_t)16, rejectMsg->getBegin());
  REQUIRE_EQ((int32_t)32, rejectMsg->getLength());
}

void BtRequestMessageTest::
    testDoReceivedAction_doesntHavePieceAndFastExtensionDisabled()
{
  msg->setIndex(2);
  peer_->amChoking(false);
  msg->doReceivedAction();

  REQUIRE_EQ((size_t)0, dispatcher_->messageQueue.size());
}

void BtRequestMessageTest::testHandleAbortRequestEvent()
{
  auto piece = std::make_shared<Piece>(1, 16_k);
  REQUIRE(!msg->isInvalidate());
  msg->onAbortOutstandingRequestEvent(BtAbortOutstandingRequestEvent(piece));
  REQUIRE(msg->isInvalidate());
}

void BtRequestMessageTest::testHandleAbortRequestEvent_indexNoMatch()
{
  auto piece = std::make_shared<Piece>(2, 16_k);
  REQUIRE(!msg->isInvalidate());
  msg->onAbortOutstandingRequestEvent(BtAbortOutstandingRequestEvent(piece));
  REQUIRE(!msg->isInvalidate());
}

void BtRequestMessageTest::testHandleAbortRequestEvent_alreadyInvalidated()
{
  auto piece = std::make_shared<Piece>(1, 16_k);
  msg->setInvalidate(true);
  REQUIRE(msg->isInvalidate());
  msg->onAbortOutstandingRequestEvent(BtAbortOutstandingRequestEvent(piece));
  REQUIRE(msg->isInvalidate());
}

void BtRequestMessageTest::testToString()
{
  REQUIRE_EQ(std::string("request index=1, begin=16, length=32"),
                       msg->toString());
}

void BtRequestMessageTest::testValidate()
{
  BtRequestMessage msg(0, 0, 16_k);
  msg.setBtMessageValidator(
      make_unique<RangeBtMessageValidator>(&msg, 1_k, 256_k));
  msg.validate();
}

void BtRequestMessageTest::testValidate_lengthTooLong()
{
  BtRequestMessage msg(0, 0, MAX_BLOCK_LENGTH + 1);
  msg.setBtMessageValidator(
      make_unique<RangeBtMessageValidator>(&msg, 1_k, 256_k));
  try {
    msg.validate();
    FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    REQUIRE_EQ(
        "Length too long: " + util::uitos(MAX_BLOCK_LENGTH + 1) + " > " +
            util::uitos(MAX_BLOCK_LENGTH / 1024) + "KB",
        std::string(e.what()));
  }
}

} // namespace aria2
