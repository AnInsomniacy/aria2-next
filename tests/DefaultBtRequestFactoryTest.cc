#include "DefaultBtRequestFactory.h"

#include <algorithm>

#include "a2doctest.h"

#include "MockBtMessage.h"
#include "MockBtMessageFactory.h"
#include "MockBtMessageDispatcher.h"
#include "MockPieceStorage.h"
#include "Peer.h"
#include "FileEntry.h"
#include "BtHandshakeMessage.h"
#include "DownloadContext.h"
#include "bittorrent_helper.h"

namespace aria2 {

class DefaultBtRequestFactoryTest {


private:
  std::shared_ptr<Peer> peer_;
  std::unique_ptr<DefaultBtRequestFactory> requestFactory_;
  std::unique_ptr<MockPieceStorage> pieceStorage_;
  std::unique_ptr<MockBtMessageFactory> messageFactory_;
  std::unique_ptr<MockBtMessageDispatcher> dispatcher_;

public:
  void testAddTargetPiece();
  void testRemoveCompletedPiece();
  void testCreateRequestMessages();
  void testCreateRequestMessages_onEndGame();
  void testRemoveTargetPiece();
  void testGetTargetPieceIndexes();

  class MockBtRequestMessage : public MockBtMessage {
  public:
    size_t index;
    size_t blockIndex;

    MockBtRequestMessage(size_t index, size_t blockIndex)
        : index(index), blockIndex(blockIndex)
    {
    }
  };

  class MockBtMessageFactory2 : public MockBtMessageFactory {
  public:
    virtual std::unique_ptr<BtRequestMessage>
    createRequestMessage(const std::shared_ptr<Piece>& piece,
                         size_t blockIndex) override
    {
      return make_unique<BtRequestMessage>(piece->getIndex(), 0, 0, blockIndex);
    }
  };

  class MockBtMessageDispatcher2 : public MockBtMessageDispatcher {
  public:
    virtual bool isOutstandingRequest(size_t index,
                                      size_t blockIndex) override
    {
      return index == 0 && blockIndex == 0;
    }
  };

  class BtRequestMessageSorter {
  public:
    bool operator()(const std::unique_ptr<BtRequestMessage>& a,
                    const std::unique_ptr<BtRequestMessage>& b)
    {
      return a->getIndex() < b->getIndex() ||
             (a->getIndex() == b->getIndex() &&
              a->getBlockIndex() < b->getBlockIndex());
    }
  };

  void setUp()
  {
    pieceStorage_ = make_unique<MockPieceStorage>();
    peer_ = std::make_shared<Peer>("host", 6969);
    messageFactory_ = make_unique<MockBtMessageFactory2>();
    dispatcher_ = make_unique<MockBtMessageDispatcher>();
    requestFactory_ = make_unique<DefaultBtRequestFactory>();
    requestFactory_->setPieceStorage(pieceStorage_.get());
    requestFactory_->setPeer(peer_);
    requestFactory_->setBtMessageDispatcher(dispatcher_.get());
    requestFactory_->setBtMessageFactory(messageFactory_.get());
  }
};

A2_TEST(DefaultBtRequestFactoryTest, testAddTargetPiece)
A2_TEST(DefaultBtRequestFactoryTest, testRemoveCompletedPiece)
A2_TEST(DefaultBtRequestFactoryTest, testCreateRequestMessages)
A2_TEST(DefaultBtRequestFactoryTest, testCreateRequestMessages_onEndGame)
A2_TEST(DefaultBtRequestFactoryTest, testRemoveTargetPiece)
A2_TEST(DefaultBtRequestFactoryTest, testGetTargetPieceIndexes)

void DefaultBtRequestFactoryTest::testAddTargetPiece()
{
  {
    auto piece = std::make_shared<Piece>(0, 160_k);
    requestFactory_->addTargetPiece(piece);
    REQUIRE_EQ((size_t)1, requestFactory_->countTargetPiece());
  }
  {
    auto piece = std::make_shared<Piece>(1, 16_k * 9);
    piece->completeBlock(0);
    requestFactory_->addTargetPiece(piece);
    REQUIRE_EQ((size_t)2, requestFactory_->countTargetPiece());
  }
  REQUIRE_EQ((size_t)18, requestFactory_->countMissingBlock());
}

void DefaultBtRequestFactoryTest::testRemoveCompletedPiece()
{
  auto piece1 = std::make_shared<Piece>(0, 16_k);
  auto piece2 = std::make_shared<Piece>(1, 16_k);
  piece2->setAllBlock();
  requestFactory_->addTargetPiece(piece1);
  requestFactory_->addTargetPiece(piece2);
  REQUIRE_EQ((size_t)2, requestFactory_->countTargetPiece());
  requestFactory_->removeCompletedPiece();
  REQUIRE_EQ((size_t)1, requestFactory_->countTargetPiece());
  REQUIRE_EQ((size_t)0,
                       requestFactory_->getTargetPieces().front()->getIndex());
}

void DefaultBtRequestFactoryTest::testCreateRequestMessages()
{
  constexpr int PIECE_LENGTH = 32_k;
  auto piece1 = std::make_shared<Piece>(0, PIECE_LENGTH);
  auto piece2 = std::make_shared<Piece>(1, PIECE_LENGTH);
  requestFactory_->addTargetPiece(piece1);
  requestFactory_->addTargetPiece(piece2);

  auto msgs = requestFactory_->createRequestMessages(3, false);

  REQUIRE_EQ((size_t)3, msgs.size());
  auto msg = msgs[0].get();
  REQUIRE_EQ((size_t)0, msg->getIndex());
  REQUIRE_EQ((size_t)0, msg->getBlockIndex());
  msg = msgs[1].get();
  REQUIRE_EQ((size_t)0, msg->getIndex());
  REQUIRE_EQ((size_t)1, msg->getBlockIndex());
  msg = msgs[2].get();
  REQUIRE_EQ((size_t)1, msg->getIndex());
  REQUIRE_EQ((size_t)0, msg->getBlockIndex());

  {
    auto msgs = requestFactory_->createRequestMessages(3, false);
    REQUIRE_EQ((size_t)1, msgs.size());
  }
}

void DefaultBtRequestFactoryTest::testCreateRequestMessages_onEndGame()
{
  auto dispatcher = make_unique<MockBtMessageDispatcher2>();

  requestFactory_->setBtMessageDispatcher(dispatcher.get());

  constexpr int PIECE_LENGTH = 32_k;
  auto piece1 = std::make_shared<Piece>(0, PIECE_LENGTH);
  auto piece2 = std::make_shared<Piece>(1, PIECE_LENGTH);
  requestFactory_->addTargetPiece(piece1);
  requestFactory_->addTargetPiece(piece2);

  auto msgs = requestFactory_->createRequestMessages(3, true);
  std::sort(std::begin(msgs), std::end(msgs), BtRequestMessageSorter());

  REQUIRE_EQ((size_t)3, msgs.size());
  auto msg = msgs[0].get();
  REQUIRE_EQ((size_t)0, msg->getIndex());
  REQUIRE_EQ((size_t)1, msg->getBlockIndex());
  msg = msgs[1].get();
  REQUIRE_EQ((size_t)1, msg->getIndex());
  REQUIRE_EQ((size_t)0, msg->getBlockIndex());
  msg = msgs[2].get();
  REQUIRE_EQ((size_t)1, msg->getIndex());
  REQUIRE_EQ((size_t)1, msg->getBlockIndex());
}

void DefaultBtRequestFactoryTest::testRemoveTargetPiece()
{
  auto piece1 = std::make_shared<Piece>(0, 16_k);

  requestFactory_->addTargetPiece(piece1);

  REQUIRE(std::find_if(requestFactory_->getTargetPieces().begin(),
                              requestFactory_->getTargetPieces().end(),
                              derefEqual(piece1)) !=
                 requestFactory_->getTargetPieces().end());

  requestFactory_->removeTargetPiece(piece1);

  REQUIRE(std::find_if(requestFactory_->getTargetPieces().begin(),
                              requestFactory_->getTargetPieces().end(),
                              derefEqual(piece1)) ==
                 requestFactory_->getTargetPieces().end());
}

void DefaultBtRequestFactoryTest::testGetTargetPieceIndexes()
{
  auto piece1 = std::make_shared<Piece>(1, 16_k);
  auto piece3 = std::make_shared<Piece>(3, 16_k);
  auto piece5 = std::make_shared<Piece>(5, 16_k);

  requestFactory_->addTargetPiece(piece3);
  requestFactory_->addTargetPiece(piece1);
  requestFactory_->addTargetPiece(piece5);

  auto indexes = requestFactory_->getTargetPieceIndexes();
  REQUIRE_EQ((size_t)3, indexes.size());
  REQUIRE_EQ((size_t)3, indexes[0]);
  REQUIRE_EQ((size_t)1, indexes[1]);
  REQUIRE_EQ((size_t)5, indexes[2]);
}

} // namespace aria2
