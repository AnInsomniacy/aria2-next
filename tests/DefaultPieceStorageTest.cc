#include "DefaultPieceStorage.h"

#include "a2doctest.h"

#include "util.h"
#include "Exception.h"
#include "Piece.h"
#include "Peer.h"
#include "Option.h"
#include "FileEntry.h"
#include "RarestPieceSelector.h"
#include "InorderPieceSelector.h"
#include "DownloadContext.h"
#include "bittorrent_helper.h"
#include "DiskAdaptor.h"
#include "DiskWriterFactory.h"
#include "PieceStatMan.h"
#include "prefs.h"

namespace aria2 {

class DefaultPieceStorageTest {


private:
  std::shared_ptr<DownloadContext> dctx_;
  std::shared_ptr<Peer> peer;
  std::shared_ptr<Option> option_;
  std::unique_ptr<PieceSelector> pieceSelector_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    option_->put(PREF_DIR, ".");
    dctx_ = std::make_shared<DownloadContext>();
    bittorrent::load(A2_TEST_DIR "/test.torrent", dctx_, option_);
    peer = std::make_shared<Peer>("192.168.0.1", 6889);
    peer->allocateSessionResource(dctx_->getPieceLength(),
                                  dctx_->getTotalLength());
    pieceSelector_ = make_unique<InorderPieceSelector>();
  }

  void testGetTotalLength();
  void testGetMissingPiece();
  void testGetMissingPiece_many();
  void testGetMissingPiece_excludedIndexes();
  void testGetMissingPiece_manyWithExcludedIndexes();
  void testGetMissingFastPiece();
  void testGetMissingFastPiece_excludedIndexes();
  void testHasMissingPiece();
  void testCompletePiece();
  void testGetPiece();
  void testGetPieceInUsedPieces();
  void testGetPieceCompletedPiece();
  void testCancelPiece();
  void testMarkPiecesDone();
  void testGetCompletedLength();
  void testGetFilteredCompletedLength();
  void testGetNextUsedIndex();
  void testAdvertisePiece();
};

A2_TEST(DefaultPieceStorageTest, testGetTotalLength)
A2_TEST(DefaultPieceStorageTest, testGetMissingPiece)
A2_TEST(DefaultPieceStorageTest, testGetMissingPiece_many)
A2_TEST(DefaultPieceStorageTest, testGetMissingPiece_excludedIndexes)
A2_TEST(DefaultPieceStorageTest, testGetMissingPiece_manyWithExcludedIndexes)
A2_TEST(DefaultPieceStorageTest, testGetMissingFastPiece)
A2_TEST(DefaultPieceStorageTest, testGetMissingFastPiece_excludedIndexes)
A2_TEST(DefaultPieceStorageTest, testHasMissingPiece)
A2_TEST(DefaultPieceStorageTest, testCompletePiece)
A2_TEST(DefaultPieceStorageTest, testGetPiece)
A2_TEST(DefaultPieceStorageTest, testGetPieceInUsedPieces)
A2_TEST(DefaultPieceStorageTest, testGetPieceCompletedPiece)
A2_TEST(DefaultPieceStorageTest, testCancelPiece)
A2_TEST(DefaultPieceStorageTest, testMarkPiecesDone)
A2_TEST(DefaultPieceStorageTest, testGetCompletedLength)
A2_TEST(DefaultPieceStorageTest, testGetFilteredCompletedLength)
A2_TEST(DefaultPieceStorageTest, testGetNextUsedIndex)
A2_TEST(DefaultPieceStorageTest, testAdvertisePiece)

void DefaultPieceStorageTest::testGetTotalLength()
{
  DefaultPieceStorage pss(dctx_, option_.get());

  REQUIRE_EQ((int64_t)384LL, pss.getTotalLength());
}

void DefaultPieceStorageTest::testGetMissingPiece()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  pss.setPieceSelector(std::move(pieceSelector_));
  peer->setAllBitfield();

  auto piece = pss.getMissingPiece(peer, 1);
  REQUIRE_EQ(std::string("piece: index=0, length=128"),
                       piece->toString());
  REQUIRE(piece->usedBy(1));
  piece = pss.getMissingPiece(peer, 1);
  REQUIRE_EQ(std::string("piece: index=1, length=128"),
                       piece->toString());
  piece = pss.getMissingPiece(peer, 1);
  REQUIRE_EQ(std::string("piece: index=2, length=128"),
                       piece->toString());
  piece = pss.getMissingPiece(peer, 1);
  REQUIRE(!piece);
}

void DefaultPieceStorageTest::testGetMissingPiece_many()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  pss.setPieceSelector(std::move(pieceSelector_));
  peer->setAllBitfield();
  std::vector<std::shared_ptr<Piece>> pieces;
  pss.getMissingPiece(pieces, 2, peer, 1);
  REQUIRE_EQ((size_t)2, pieces.size());
  REQUIRE_EQ(std::string("piece: index=0, length=128"),
                       pieces[0]->toString());
  REQUIRE(pieces[0]->usedBy(1));
  REQUIRE_EQ(std::string("piece: index=1, length=128"),
                       pieces[1]->toString());
  pieces.clear();
  pss.getMissingPiece(pieces, 2, peer, 1);
  REQUIRE_EQ((size_t)1, pieces.size());
  REQUIRE_EQ(std::string("piece: index=2, length=128"),
                       pieces[0]->toString());
}

void DefaultPieceStorageTest::testGetMissingPiece_excludedIndexes()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  pss.setPieceSelector(std::move(pieceSelector_));
  pss.setEndGamePieceNum(0);

  peer->setAllBitfield();

  std::vector<size_t> excludedIndexes;
  excludedIndexes.push_back(1);

  auto piece = pss.getMissingPiece(peer, excludedIndexes, 1);
  REQUIRE_EQ(std::string("piece: index=0, length=128"),
                       piece->toString());

  piece = pss.getMissingPiece(peer, excludedIndexes, 1);
  REQUIRE_EQ(std::string("piece: index=2, length=128"),
                       piece->toString());

  piece = pss.getMissingPiece(peer, excludedIndexes, 1);
  REQUIRE(!piece);
}

void DefaultPieceStorageTest::testGetMissingPiece_manyWithExcludedIndexes()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  pss.setPieceSelector(std::move(pieceSelector_));
  peer->setAllBitfield();
  std::vector<size_t> excludedIndexes;
  excludedIndexes.push_back(1);
  std::vector<std::shared_ptr<Piece>> pieces;
  pss.getMissingPiece(pieces, 2, peer, excludedIndexes, 1);
  REQUIRE_EQ((size_t)2, pieces.size());
  REQUIRE_EQ(std::string("piece: index=0, length=128"),
                       pieces[0]->toString());
  REQUIRE_EQ(std::string("piece: index=2, length=128"),
                       pieces[1]->toString());
  pieces.clear();
  pss.getMissingPiece(pieces, 2, peer, excludedIndexes, 1);
  REQUIRE(pieces.empty());
}

void DefaultPieceStorageTest::testGetMissingFastPiece()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  pss.setPieceSelector(std::move(pieceSelector_));
  pss.setEndGamePieceNum(0);

  peer->setAllBitfield();
  peer->setFastExtensionEnabled(true);
  peer->addPeerAllowedIndex(2);

  auto piece = pss.getMissingFastPiece(peer, 1);
  REQUIRE_EQ(std::string("piece: index=2, length=128"),
                       piece->toString());

  REQUIRE(!pss.getMissingFastPiece(peer, 1));
}

void DefaultPieceStorageTest::testGetMissingFastPiece_excludedIndexes()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  pss.setPieceSelector(std::move(pieceSelector_));
  pss.setEndGamePieceNum(0);

  peer->setAllBitfield();
  peer->setFastExtensionEnabled(true);
  peer->addPeerAllowedIndex(1);
  peer->addPeerAllowedIndex(2);

  std::vector<size_t> excludedIndexes;
  excludedIndexes.push_back(2);

  auto piece = pss.getMissingFastPiece(peer, excludedIndexes, 1);
  REQUIRE_EQ(std::string("piece: index=1, length=128"),
                       piece->toString());

  REQUIRE(!pss.getMissingFastPiece(peer, excludedIndexes, 1));
}

void DefaultPieceStorageTest::testHasMissingPiece()
{
  DefaultPieceStorage pss(dctx_, option_.get());

  REQUIRE(!pss.hasMissingPiece(peer));

  peer->setAllBitfield();

  REQUIRE(pss.hasMissingPiece(peer));
}

void DefaultPieceStorageTest::testCompletePiece()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  pss.setPieceSelector(std::move(pieceSelector_));
  pss.setEndGamePieceNum(0);

  peer->setAllBitfield();

  auto piece = pss.getMissingPiece(peer, 1);
  REQUIRE_EQ(std::string("piece: index=0, length=128"),
                       piece->toString());

  REQUIRE_EQ((int64_t)0LL, pss.getCompletedLength());

  pss.completePiece(piece);

  REQUIRE_EQ((int64_t)128LL, pss.getCompletedLength());

  auto incompletePiece = pss.getMissingPiece(peer, 1);
  incompletePiece->completeBlock(0);
  REQUIRE_EQ((int64_t)256LL, pss.getCompletedLength());
}

void DefaultPieceStorageTest::testGetPiece()
{
  DefaultPieceStorage pss(dctx_, option_.get());

  auto pieceGot = pss.getPiece(0);
  REQUIRE_EQ((size_t)0, pieceGot->getIndex());
  REQUIRE_EQ((int64_t)128, pieceGot->getLength());
  REQUIRE_EQ(false, pieceGot->pieceComplete());
}

void DefaultPieceStorageTest::testGetPieceInUsedPieces()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  auto piece = std::make_shared<Piece>(0, 128);
  piece->completeBlock(0);
  pss.addUsedPiece(piece);
  auto pieceGot = pss.getPiece(0);
  REQUIRE_EQ((size_t)0, pieceGot->getIndex());
  REQUIRE_EQ((int64_t)128, pieceGot->getLength());
  REQUIRE_EQ((size_t)1, pieceGot->countCompleteBlock());
}

void DefaultPieceStorageTest::testGetPieceCompletedPiece()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  auto piece = std::make_shared<Piece>(0, 128);
  pss.completePiece(piece);
  auto pieceGot = pss.getPiece(0);
  REQUIRE_EQ((size_t)0, pieceGot->getIndex());
  REQUIRE_EQ((int64_t)128, pieceGot->getLength());
  REQUIRE_EQ(true, pieceGot->pieceComplete());
}

void DefaultPieceStorageTest::testCancelPiece()
{
  size_t pieceLength = 256_k;
  int64_t totalLength = 32 * pieceLength; // <-- make the number of piece
                                          // greater than END_GAME_PIECE_NUM
  std::deque<std::string> uris1;
  uris1.push_back("http://localhost/src/file1.txt");
  auto file1 =
      std::make_shared<FileEntry>("src/file1.txt", totalLength, 0 /*, uris1*/);

  auto dctx = std::make_shared<DownloadContext>(pieceLength, totalLength,
                                                "src/file1.txt");

  DefaultPieceStorage ps{dctx, option_.get()};

  auto p = ps.getMissingPiece(0, 1);
  p->completeBlock(0);

  ps.cancelPiece(p, 1);

  auto p2 = ps.getMissingPiece(0, 2);

  REQUIRE(p2->hasBlock(0));
  REQUIRE(p2->usedBy(2));
  REQUIRE(!p2->usedBy(1));
}

void DefaultPieceStorageTest::testMarkPiecesDone()
{
  size_t pieceLength = 256_k;
  int64_t totalLength = 4_m;
  auto dctx = std::make_shared<DownloadContext>(pieceLength, totalLength);

  DefaultPieceStorage ps(dctx, option_.get());

  ps.markPiecesDone(pieceLength * 10 + 32_k + 1);

  for (size_t i = 0; i < 10; ++i) {
    REQUIRE(ps.hasPiece(i));
  }
  for (size_t i = 10; i < (totalLength + pieceLength - 1) / pieceLength; ++i) {
    REQUIRE(!ps.hasPiece(i));
  }
  REQUIRE_EQ((int64_t)pieceLength * 10 + (int64_t)32_k,
                       ps.getCompletedLength());

  ps.markPiecesDone(totalLength);

  for (size_t i = 0; i < (totalLength + pieceLength - 1) / pieceLength; ++i) {
    REQUIRE(ps.hasPiece(i));
  }

  ps.markPiecesDone(0);
  REQUIRE_EQ((int64_t)0, ps.getCompletedLength());
}

void DefaultPieceStorageTest::testGetCompletedLength()
{
  auto dctx = std::make_shared<DownloadContext>(1_m, 256_m);

  DefaultPieceStorage ps(dctx, option_.get());

  REQUIRE_EQ((int64_t)0, ps.getCompletedLength());

  ps.markPiecesDone(250_m);
  REQUIRE_EQ((int64_t)250_m, ps.getCompletedLength());

  std::vector<std::shared_ptr<Piece>> inFlightPieces;
  for (int i = 0; i < 2; ++i) {
    auto p = std::make_shared<Piece>(250 + i, 1_m);
    for (int j = 0; j < 32; ++j) {
      p->completeBlock(j);
    }
    inFlightPieces.push_back(p);
    REQUIRE_EQ((int64_t)512_k, p->getCompletedLength());
  }
  ps.addInFlightPiece(inFlightPieces);

  REQUIRE_EQ((int64_t)251_m, ps.getCompletedLength());

  ps.markPiecesDone(256_m);

  REQUIRE_EQ((int64_t)256_m, ps.getCompletedLength());
}

void DefaultPieceStorageTest::testGetFilteredCompletedLength()
{
  const size_t pieceLength = 1_m;
  auto dctx = std::make_shared<DownloadContext>();
  dctx->setPieceLength(pieceLength);
  auto files = std::vector<std::shared_ptr<FileEntry>>{
      std::make_shared<FileEntry>("foo", 2 * pieceLength, 0),
      std::make_shared<FileEntry>("bar", 4 * pieceLength, 2 * pieceLength)};
  files[1]->setRequested(false);
  dctx->setFileEntries(std::begin(files), std::end(files));

  DefaultPieceStorage ps(dctx, option_.get());
  std::vector<std::shared_ptr<Piece>> inflightPieces(2);
  inflightPieces[0] = std::make_shared<Piece>(1, pieceLength);
  inflightPieces[0]->completeBlock(0);
  inflightPieces[1] = std::make_shared<Piece>(2, pieceLength);
  inflightPieces[1]->completeBlock(1);
  inflightPieces[1]->completeBlock(2);

  ps.addInFlightPiece(inflightPieces);
  ps.setupFileFilter();

  auto piece = ps.getMissingPiece(0, 1);
  ps.completePiece(piece);

  REQUIRE_EQ((int64_t)pieceLength + (int64_t)16_k,
                       ps.getFilteredCompletedLength());
}

void DefaultPieceStorageTest::testGetNextUsedIndex()
{
  DefaultPieceStorage pss(dctx_, option_.get());
  REQUIRE_EQ((size_t)3, pss.getNextUsedIndex(0));
  auto piece = pss.getMissingPiece(2, 1);
  REQUIRE_EQ((size_t)2, pss.getNextUsedIndex(0));
  pss.completePiece(piece);
  REQUIRE_EQ((size_t)2, pss.getNextUsedIndex(0));
  piece = pss.getMissingPiece(0, 1);
  REQUIRE_EQ((size_t)2, pss.getNextUsedIndex(0));
}

void DefaultPieceStorageTest::testAdvertisePiece()
{
  DefaultPieceStorage ps(dctx_, option_.get());

  ps.advertisePiece(1, 100, Timer(10_s));
  ps.advertisePiece(2, 101, Timer(11_s));
  ps.advertisePiece(3, 102, Timer(11_s));
  ps.advertisePiece(1, 103, Timer(12_s));
  ps.advertisePiece(2, 104, Timer(100_s));

  std::vector<size_t> res, ans;
  uint64_t lastHaveIndex;

  lastHaveIndex = ps.getAdvertisedPieceIndexes(res, 1, 0);
  ans = std::vector<size_t>{100, 101, 102, 103, 104};

  REQUIRE_EQ((uint64_t)5, lastHaveIndex);
  REQUIRE(ans == res);

  res.clear();
  lastHaveIndex = ps.getAdvertisedPieceIndexes(res, 1, 3);
  ans = std::vector<size_t>{103, 104};

  REQUIRE_EQ((uint64_t)5, lastHaveIndex);
  REQUIRE_EQ((size_t)2, res.size());
  REQUIRE(ans == res);

  res.clear();
  lastHaveIndex = ps.getAdvertisedPieceIndexes(res, 1, 5);

  REQUIRE_EQ((uint64_t)5, lastHaveIndex);
  REQUIRE_EQ((size_t)0, res.size());

  // remove haves

  ps.removeAdvertisedPiece(Timer(11_s));

  res.clear();
  lastHaveIndex = ps.getAdvertisedPieceIndexes(res, 1, 0);
  ans = std::vector<size_t>{103, 104};

  REQUIRE_EQ((uint64_t)5, lastHaveIndex);
  REQUIRE_EQ((size_t)2, res.size());
  REQUIRE(ans == res);

  ps.removeAdvertisedPiece(Timer(300_s));

  res.clear();
  lastHaveIndex = ps.getAdvertisedPieceIndexes(res, 1, 0);

  REQUIRE_EQ((uint64_t)0, lastHaveIndex);
  REQUIRE_EQ((size_t)0, res.size());
}

} // namespace aria2
