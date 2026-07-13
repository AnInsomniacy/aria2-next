#include "DefaultBtMessageDispatcher.h"

#include <cassert>

#include "a2doctest.h"

#include "util.h"
#include "Exception.h"
#include "MockBtMessage.h"
#include "MockBtMessageFactory.h"
#include "prefs.h"
#include "BtCancelSendingPieceEvent.h"
#include "BtHandshakeMessage.h"
#include "Option.h"
#include "RequestGroupMan.h"
#include "ServerStatMan.h"
#include "RequestGroup.h"
#include "DownloadContext.h"
#include "bittorrent_helper.h"
#include "PeerConnection.h"

namespace aria2 {

class DefaultBtMessageDispatcherTest {


private:
  std::shared_ptr<DownloadContext> dctx_;
  std::shared_ptr<Peer> peer;
  std::unique_ptr<DefaultBtMessageDispatcher> btMessageDispatcher;
  std::unique_ptr<MockBtMessageFactory> messageFactory_;
  std::unique_ptr<RequestGroupMan> rgman_;
  std::shared_ptr<Option> option_;
  std::unique_ptr<RequestGroup> rg_;

public:
  void tearDown() {}

  void testAddMessage();
  void testSendMessages();
  void testSendMessages_underUploadLimit();
  void testSendMessages_overUploadLimit();
  void testDoCancelSendingPieceAction();
  void testCheckRequestSlotAndDoNecessaryThing();
  void testCheckRequestSlotAndDoNecessaryThing_timeout();
  void testCheckRequestSlotAndDoNecessaryThing_completeBlock();
  void testCountOutstandingRequest();
  void testIsOutstandingRequest();
  void testGetOutstandingRequest();
  void testRemoveOutstandingRequest();

  struct EventCheck {
    EventCheck()
        : onQueuedCalled{false}, sendCalled{false}, doCancelActionCalled{false}
    {
    }
    bool onQueuedCalled;
    bool sendCalled;
    bool doCancelActionCalled;
  };

  class MockBtMessage2 : public MockBtMessage {
  public:
    EventCheck* evcheck;
    std::string type;
    MockBtMessage2(EventCheck* evcheck = nullptr) : evcheck{evcheck} {}

    virtual void onQueued() override
    {
      if (evcheck) {
        evcheck->onQueuedCalled = true;
      }
    }

    virtual void send() override
    {
      if (evcheck) {
        evcheck->sendCalled = true;
      }
    }

    virtual void onCancelSendingPieceEvent(
        const BtCancelSendingPieceEvent& event) override
    {
      if (evcheck) {
        evcheck->doCancelActionCalled = true;
      }
    }
  };

  class MockBtMessageFactory2 : public MockBtMessageFactory {
  public:
    virtual std::unique_ptr<BtCancelMessage>
    createCancelMessage(size_t index, int32_t begin,
                        int32_t length) override
    {
      return make_unique<BtCancelMessage>(index, begin, length);
    }
  };

  void setUp()
  {
    option_ = std::make_shared<Option>();
    option_->put(PREF_DIR, ".");

    rg_ = make_unique<RequestGroup>(GroupId::create(), option_);

    dctx_ = std::make_shared<DownloadContext>();
    bittorrent::load(A2_TEST_DIR "/test.torrent", dctx_, option_);

    rg_->setDownloadContext(dctx_);

    peer = std::make_shared<Peer>("192.168.0.1", 6969);
    peer->allocateSessionResource(dctx_->getPieceLength(),
                                  dctx_->getTotalLength());
    messageFactory_ = make_unique<MockBtMessageFactory2>();

    rgman_ = make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 0, option_.get());

    btMessageDispatcher = make_unique<DefaultBtMessageDispatcher>();
    btMessageDispatcher->setPeer(peer);
    btMessageDispatcher->setDownloadContext(dctx_.get());
    btMessageDispatcher->setBtMessageFactory(messageFactory_.get());
    btMessageDispatcher->setCuid(1);
    btMessageDispatcher->setRequestGroupMan(rgman_.get());
  }
};

A2_TEST(DefaultBtMessageDispatcherTest, testAddMessage)
A2_TEST(DefaultBtMessageDispatcherTest, testSendMessages)
A2_TEST(DefaultBtMessageDispatcherTest, testSendMessages_underUploadLimit)
A2_TEST(DefaultBtMessageDispatcherTest, testDoCancelSendingPieceAction)
A2_TEST(DefaultBtMessageDispatcherTest, testCheckRequestSlotAndDoNecessaryThing)
A2_TEST(DefaultBtMessageDispatcherTest, testCheckRequestSlotAndDoNecessaryThing_timeout)
A2_TEST(DefaultBtMessageDispatcherTest, testCheckRequestSlotAndDoNecessaryThing_completeBlock)
A2_TEST(DefaultBtMessageDispatcherTest, testCountOutstandingRequest)
A2_TEST(DefaultBtMessageDispatcherTest, testIsOutstandingRequest)
A2_TEST(DefaultBtMessageDispatcherTest, testGetOutstandingRequest)
A2_TEST(DefaultBtMessageDispatcherTest, testRemoveOutstandingRequest)

void DefaultBtMessageDispatcherTest::testAddMessage()
{
  auto evcheck = EventCheck{};
  auto msg = make_unique<MockBtMessage2>(&evcheck);
  btMessageDispatcher->addMessageToQueue(std::move(msg));
  REQUIRE_EQ(true, evcheck.onQueuedCalled);
  REQUIRE_EQ((size_t)1,
                       btMessageDispatcher->getMessageQueue().size());
}

void DefaultBtMessageDispatcherTest::testSendMessages()
{
  auto evcheck1 = EventCheck{};
  auto msg1 = make_unique<MockBtMessage2>(&evcheck1);
  msg1->setUploading(false);
  auto evcheck2 = EventCheck{};
  auto msg2 = make_unique<MockBtMessage2>(&evcheck2);
  msg2->setUploading(false);
  btMessageDispatcher->addMessageToQueue(std::move(msg1));
  btMessageDispatcher->addMessageToQueue(std::move(msg2));
  btMessageDispatcher->sendMessagesInternal();

  REQUIRE(evcheck1.sendCalled);
  REQUIRE(evcheck2.sendCalled);
}

void DefaultBtMessageDispatcherTest::testSendMessages_underUploadLimit()
{
  auto evcheck1 = EventCheck{};
  auto msg1 = make_unique<MockBtMessage2>(&evcheck1);
  msg1->setUploading(true);
  auto evcheck2 = EventCheck{};
  auto msg2 = make_unique<MockBtMessage2>(&evcheck2);
  msg2->setUploading(true);
  btMessageDispatcher->addMessageToQueue(std::move(msg1));
  btMessageDispatcher->addMessageToQueue(std::move(msg2));
  btMessageDispatcher->sendMessagesInternal();

  REQUIRE(evcheck1.sendCalled);
  REQUIRE(evcheck2.sendCalled);
}

void DefaultBtMessageDispatcherTest::testDoCancelSendingPieceAction()
{
  auto evcheck1 = EventCheck{};
  auto msg1 = make_unique<MockBtMessage2>(&evcheck1);
  auto evcheck2 = EventCheck{};
  auto msg2 = make_unique<MockBtMessage2>(&evcheck2);

  btMessageDispatcher->addMessageToQueue(std::move(msg1));
  btMessageDispatcher->addMessageToQueue(std::move(msg2));

  btMessageDispatcher->doCancelSendingPieceAction(0, 0, 0);

  REQUIRE(evcheck1.doCancelActionCalled);
  REQUIRE(evcheck2.doCancelActionCalled);
}

namespace {
int MY_PIECE_LENGTH = 16_k;
} // namespace

void DefaultBtMessageDispatcherTest::testCheckRequestSlotAndDoNecessaryThing()
{
  auto piece = std::make_shared<Piece>(0, MY_PIECE_LENGTH);
  size_t index;
  REQUIRE(piece->getMissingUnusedBlockIndex(index));
  REQUIRE_EQ((size_t)0, index);

  btMessageDispatcher->setRequestTimeout(1_min);
  btMessageDispatcher->addOutstandingRequest(
      make_unique<RequestSlot>(0, 0, MY_PIECE_LENGTH, 0, piece));

  btMessageDispatcher->checkRequestSlotAndDoNecessaryThing();

  REQUIRE_EQ((size_t)0,
                       btMessageDispatcher->getMessageQueue().size());
  REQUIRE_EQ((size_t)1,
                       btMessageDispatcher->getRequestSlots().size());
}

void DefaultBtMessageDispatcherTest::
    testCheckRequestSlotAndDoNecessaryThing_timeout()
{
  auto piece = std::make_shared<Piece>(0, MY_PIECE_LENGTH);
  size_t index;
  REQUIRE(piece->getMissingUnusedBlockIndex(index));
  REQUIRE_EQ((size_t)0, index);

  btMessageDispatcher->setRequestTimeout(1_min);
  auto slot = make_unique<RequestSlot>(0, 0, MY_PIECE_LENGTH, 0, piece);
  // make this slot timeout
  slot->setDispatchedTime(Timer::zero());
  btMessageDispatcher->addOutstandingRequest(std::move(slot));
  btMessageDispatcher->checkRequestSlotAndDoNecessaryThing();

  REQUIRE_EQ((size_t)0,
                       btMessageDispatcher->getMessageQueue().size());
  REQUIRE_EQ((size_t)0,
                       btMessageDispatcher->getRequestSlots().size());
  REQUIRE_EQ(false, piece->isBlockUsed(0));
  REQUIRE_EQ(true, peer->snubbing());
}

void DefaultBtMessageDispatcherTest::
    testCheckRequestSlotAndDoNecessaryThing_completeBlock()
{
  auto piece = std::make_shared<Piece>(0, MY_PIECE_LENGTH);
  piece->completeBlock(0);
  btMessageDispatcher->setRequestTimeout(1_min);
  btMessageDispatcher->addOutstandingRequest(
      make_unique<RequestSlot>(0, 0, MY_PIECE_LENGTH, 0, piece));

  btMessageDispatcher->checkRequestSlotAndDoNecessaryThing();

  REQUIRE_EQ((size_t)1,
                       btMessageDispatcher->getMessageQueue().size());
  REQUIRE_EQ((size_t)0,
                       btMessageDispatcher->getRequestSlots().size());
}

void DefaultBtMessageDispatcherTest::testCountOutstandingRequest()
{
  btMessageDispatcher->addOutstandingRequest(
      make_unique<RequestSlot>(0, 0, MY_PIECE_LENGTH, 0));
  REQUIRE_EQ((size_t)1,
                       btMessageDispatcher->countOutstandingRequest());
}

void DefaultBtMessageDispatcherTest::testIsOutstandingRequest()
{
  btMessageDispatcher->addOutstandingRequest(
      make_unique<RequestSlot>(0, 0, MY_PIECE_LENGTH, 0));

  REQUIRE(btMessageDispatcher->isOutstandingRequest(0, 0));
  REQUIRE(!btMessageDispatcher->isOutstandingRequest(0, 1));
  REQUIRE(!btMessageDispatcher->isOutstandingRequest(1, 0));
  REQUIRE(!btMessageDispatcher->isOutstandingRequest(1, 1));
}

void DefaultBtMessageDispatcherTest::testGetOutstandingRequest()
{
  btMessageDispatcher->addOutstandingRequest(
      make_unique<RequestSlot>(1, 1_k, 16_k, 10));

  REQUIRE(btMessageDispatcher->getOutstandingRequest(1, 1_k, 16_k));

  REQUIRE(!btMessageDispatcher->getOutstandingRequest(1, 1_k, 17_k));

  REQUIRE(!btMessageDispatcher->getOutstandingRequest(1, 2_k, 16_k));

  REQUIRE(!btMessageDispatcher->getOutstandingRequest(2, 1_k, 16_k));
}

void DefaultBtMessageDispatcherTest::testRemoveOutstandingRequest()
{
  auto piece = std::make_shared<Piece>(1, 1_m);
  size_t blockIndex = 0;
  REQUIRE(piece->getMissingUnusedBlockIndex(blockIndex));
  uint32_t begin = blockIndex * piece->getBlockLength();
  size_t length = piece->getBlockLength(blockIndex);
  RequestSlot slot;
  btMessageDispatcher->addOutstandingRequest(make_unique<RequestSlot>(
      piece->getIndex(), begin, length, blockIndex, piece));

  auto s2 = btMessageDispatcher->getOutstandingRequest(piece->getIndex(), begin,
                                                       length);
  REQUIRE(s2);
  REQUIRE(piece->isBlockUsed(blockIndex));

  btMessageDispatcher->removeOutstandingRequest(s2);

  auto s3 = btMessageDispatcher->getOutstandingRequest(piece->getIndex(), begin,
                                                       length);
  REQUIRE(!s3);
  REQUIRE(!piece->isBlockUsed(blockIndex));
}

} // namespace aria2
