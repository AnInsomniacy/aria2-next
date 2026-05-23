#include "DefaultProgressInfoFile.h"

#include <fstream>

#include <cppunit/extensions/HelperMacros.h>

#include "Option.h"
#include "util.h"
#include "Exception.h"
#include "MockPieceStorage.h"
#include "prefs.h"
#include "DownloadContext.h"
#include "Piece.h"
#include "FileEntry.h"
#include "array_fun.h"
namespace aria2 {

class DefaultProgressInfoFileTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DefaultProgressInfoFileTest);
  CPPUNIT_TEST(testSave_nonBt);
  CPPUNIT_TEST(testLoad_nonBt);
#ifndef WORDS_BIGENDIAN
  CPPUNIT_TEST(testLoad_nonBt_compat);
#endif // !WORDS_BIGENDIAN
  CPPUNIT_TEST(testLoad_nonBt_pieceLengthShorter);
  CPPUNIT_TEST(testUpdateFilename);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<MockPieceStorage> pieceStorage_;
  std::shared_ptr<Option> option_;
  std::shared_ptr<BitfieldMan> bitfield_;

public:
  void initializeMembers(int32_t pieceLength, int64_t totalLength)
  {
    option_.reset(new Option());
    option_->put(PREF_DIR, A2_TEST_OUT_DIR);

    bitfield_.reset(new BitfieldMan(pieceLength, totalLength));

    pieceStorage_.reset(new MockPieceStorage());
    pieceStorage_->setBitfield(bitfield_.get());
  }

  void testSave_nonBt();
  void testLoad_nonBt();
#ifndef WORDS_BIGENDIAN
  void testLoad_nonBt_compat();
#endif // !WORDS_BIGENDIAN
  void testLoad_nonBt_pieceLengthShorter();
  void testUpdateFilename();
};

#undef BLOCK_LENGTH
#define BLOCK_LENGTH 256

CPPUNIT_TEST_SUITE_REGISTRATION(DefaultProgressInfoFileTest);

// Because load-nonBt.aria2 is made for little endian systems, exclude
// testLoad_nonBt_compat() for big endian systems.
#ifndef WORDS_BIGENDIAN
void DefaultProgressInfoFileTest::testLoad_nonBt_compat()
{
  initializeMembers(1_k, 80_k);

  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(1_k, 80_k, A2_TEST_DIR "/load-nonBt"));

  DefaultProgressInfoFile infoFile(dctx, pieceStorage_, option_.get());

  CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_DIR "/load-nonBt.aria2"),
                       infoFile.getFilename());
  infoFile.load();

  // check the contents of objects

  // total length
  CPPUNIT_ASSERT_EQUAL((int64_t)80_k, dctx->getTotalLength());

  // bitfield
  CPPUNIT_ASSERT_EQUAL(
      std::string("fffffffffffffffffffe"),
      util::toHex(bitfield_->getBitfield(), bitfield_->getBitfieldLength()));

  // the number of in-flight pieces
  CPPUNIT_ASSERT_EQUAL((size_t)2, pieceStorage_->countInFlightPiece());

  // piece index 1
  std::vector<std::shared_ptr<Piece>> inFlightPieces;
  pieceStorage_->getInFlightPieces(inFlightPieces);

  std::shared_ptr<Piece> piece1 = inFlightPieces[0];
  CPPUNIT_ASSERT_EQUAL((size_t)1, piece1->getIndex());
  CPPUNIT_ASSERT_EQUAL((int64_t)1_k, piece1->getLength());
  CPPUNIT_ASSERT_EQUAL((size_t)1, piece1->getBitfieldLength());
  CPPUNIT_ASSERT_EQUAL(
      std::string("00"),
      util::toHex(piece1->getBitfield(), piece1->getBitfieldLength()));

  // piece index 2
  std::shared_ptr<Piece> piece2 = inFlightPieces[1];
  CPPUNIT_ASSERT_EQUAL((size_t)2, piece2->getIndex());
  CPPUNIT_ASSERT_EQUAL((int64_t)512, piece2->getLength());
}
#endif // !WORDS_BIGENDIAN

void DefaultProgressInfoFileTest::testLoad_nonBt()
{
  initializeMembers(1_k, 80_k);

  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(1_k, 80_k, A2_TEST_DIR "/load-nonBt-v0001"));

  DefaultProgressInfoFile infoFile(dctx, pieceStorage_, option_.get());

  CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_DIR "/load-nonBt-v0001.aria2"),
                       infoFile.getFilename());
  infoFile.load();

  // check the contents of objects

  // total length
  CPPUNIT_ASSERT_EQUAL((int64_t)80_k, dctx->getTotalLength());

  // bitfield
  CPPUNIT_ASSERT_EQUAL(
      std::string("fffffffffffffffffffe"),
      util::toHex(bitfield_->getBitfield(), bitfield_->getBitfieldLength()));

  // the number of in-flight pieces
  CPPUNIT_ASSERT_EQUAL((size_t)2, pieceStorage_->countInFlightPiece());

  // piece index 1
  std::vector<std::shared_ptr<Piece>> inFlightPieces;
  pieceStorage_->getInFlightPieces(inFlightPieces);

  std::shared_ptr<Piece> piece1 = inFlightPieces[0];
  CPPUNIT_ASSERT_EQUAL((size_t)1, piece1->getIndex());
  CPPUNIT_ASSERT_EQUAL((int64_t)1_k, piece1->getLength());
  CPPUNIT_ASSERT_EQUAL((size_t)1, piece1->getBitfieldLength());
  CPPUNIT_ASSERT_EQUAL(
      std::string("00"),
      util::toHex(piece1->getBitfield(), piece1->getBitfieldLength()));

  // piece index 2
  std::shared_ptr<Piece> piece2 = inFlightPieces[1];
  CPPUNIT_ASSERT_EQUAL((size_t)2, piece2->getIndex());
  CPPUNIT_ASSERT_EQUAL((int64_t)512, piece2->getLength());
}

void DefaultProgressInfoFileTest::testLoad_nonBt_pieceLengthShorter()
{
  initializeMembers(512, 80_k);
  option_->put(PREF_ALLOW_PIECE_LENGTH_CHANGE, A2_V_TRUE);

  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(512, 80_k, A2_TEST_DIR "/load-nonBt-v0001"));

  DefaultProgressInfoFile infoFile(dctx, pieceStorage_, option_.get());

  CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_DIR "/load-nonBt-v0001.aria2"),
                       infoFile.getFilename());
  infoFile.load();

  // check the contents of objects

  // bitfield
  CPPUNIT_ASSERT_EQUAL(
      std::string("fffffffffffffffffffffffffffffffffffffffc"),
      util::toHex(bitfield_->getBitfield(), bitfield_->getBitfieldLength()));

  // the number of in-flight pieces
  CPPUNIT_ASSERT_EQUAL((size_t)0, pieceStorage_->countInFlightPiece());
}

void DefaultProgressInfoFileTest::testSave_nonBt()
{
  initializeMembers(1_k, 80_k);

  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(1_k, 80_k, A2_TEST_OUT_DIR "/save-temp"));

  bitfield_->setAllBit();
  bitfield_->unsetBit(79);
  pieceStorage_->setCompletedLength(80896);

  std::shared_ptr<Piece> p1(new Piece(1, 1_k));
  std::shared_ptr<Piece> p2(new Piece(2, 512));
  std::vector<std::shared_ptr<Piece>> inFlightPieces;
  inFlightPieces.push_back(p1);
  inFlightPieces.push_back(p2);
  pieceStorage_->addInFlightPiece(inFlightPieces);

  DefaultProgressInfoFile infoFile(dctx, pieceStorage_, option_.get());

  CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_OUT_DIR "/save-temp.aria2"),
                       infoFile.getFilename());

  infoFile.save();

  // read and validate
  std::ifstream in(infoFile.getFilename().c_str(), std::ios::binary);

  // in.exceptions(ios::failbit);

  unsigned char version[2];
  in.read((char*)version, sizeof(version));
  CPPUNIT_ASSERT_EQUAL(std::string("0001"),
                       util::toHex(version, sizeof(version)));

  unsigned char extension[4];
  in.read((char*)extension, sizeof(extension));
  CPPUNIT_ASSERT_EQUAL(std::string("00000000"),
                       util::toHex(extension, sizeof(extension)));

  uint32_t infoHashLength;
  in.read(reinterpret_cast<char*>(&infoHashLength), sizeof(infoHashLength));
  infoHashLength = ntohl(infoHashLength);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, infoHashLength);

  uint32_t pieceLength;
  in.read((char*)&pieceLength, sizeof(pieceLength));
  pieceLength = ntohl(pieceLength);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1_k, pieceLength);

  uint64_t totalLength;
  in.read((char*)&totalLength, sizeof(totalLength));
  totalLength = ntoh64(totalLength);
  CPPUNIT_ASSERT_EQUAL((uint64_t)80_k, totalLength);

  uint64_t uploadLength;
  in.read((char*)&uploadLength, sizeof(uploadLength));
  uploadLength = ntoh64(uploadLength);
  CPPUNIT_ASSERT_EQUAL((uint64_t)0, uploadLength);

  uint32_t bitfieldLength;
  in.read((char*)&bitfieldLength, sizeof(bitfieldLength));
  bitfieldLength = ntohl(bitfieldLength);
  CPPUNIT_ASSERT_EQUAL((uint32_t)10, bitfieldLength);

  unsigned char bitfieldRead[10];
  in.read((char*)bitfieldRead, sizeof(bitfieldRead));
  CPPUNIT_ASSERT_EQUAL(std::string("fffffffffffffffffffe"),
                       util::toHex(bitfieldRead, sizeof(bitfieldRead)));

  uint32_t numInFlightPiece;
  in.read((char*)&numInFlightPiece, sizeof(numInFlightPiece));
  numInFlightPiece = ntohl(numInFlightPiece);
  CPPUNIT_ASSERT_EQUAL((uint32_t)2, numInFlightPiece);

  // piece index 1
  uint32_t index1;
  in.read((char*)&index1, sizeof(index1));
  index1 = ntohl(index1);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, index1);

  uint32_t pieceLength1;
  in.read((char*)&pieceLength1, sizeof(pieceLength1));
  pieceLength1 = ntohl(pieceLength1);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1_k, pieceLength1);

  uint32_t pieceBitfieldLength1;
  in.read((char*)&pieceBitfieldLength1, sizeof(pieceBitfieldLength1));
  pieceBitfieldLength1 = ntohl(pieceBitfieldLength1);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, pieceBitfieldLength1);

  unsigned char pieceBitfield1[1];
  in.read((char*)pieceBitfield1, sizeof(pieceBitfield1));
  CPPUNIT_ASSERT_EQUAL(std::string("00"),
                       util::toHex(pieceBitfield1, sizeof(pieceBitfield1)));

  // piece index 2
  uint32_t index2;
  in.read((char*)&index2, sizeof(index2));
  index2 = ntohl(index2);
  CPPUNIT_ASSERT_EQUAL((uint32_t)2, index2);

  uint32_t pieceLength2;
  in.read((char*)&pieceLength2, sizeof(pieceLength2));
  pieceLength2 = ntohl(pieceLength2);
  CPPUNIT_ASSERT_EQUAL((uint32_t)512, pieceLength2);
}

void DefaultProgressInfoFileTest::testUpdateFilename()
{
  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(1_k, 80_k, A2_TEST_DIR "/file1"));

  DefaultProgressInfoFile infoFile(dctx, std::shared_ptr<MockPieceStorage>(),
                                     nullptr);
  CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_DIR "/file1.aria2"),
                       infoFile.getFilename());

  dctx->getFirstFileEntry()->setPath(A2_TEST_DIR "/file1.1");

  CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_DIR "/file1.aria2"),
                       infoFile.getFilename());

  infoFile.updateFilename();

  CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_DIR "/file1.1.aria2"),
                       infoFile.getFilename());
}

} // namespace aria2
