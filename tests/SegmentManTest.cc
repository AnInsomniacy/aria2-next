#include "SegmentMan.h"

#include "a2doctest.h"

#include "DownloadContext.h"
#include "UnknownLengthPieceStorage.h"
#include "DefaultPieceStorage.h"
#include "Segment.h"
#include "Option.h"
#include "Piece.h"
#include "PieceSelector.h"
#include "FileEntry.h"
#include "PeerStat.h"

namespace aria2 {

class SegmentManTest {


private:
  std::shared_ptr<Option> option_;
  std::shared_ptr<DownloadContext> dctx_;
  std::shared_ptr<DefaultPieceStorage> pieceStorage_;
  std::shared_ptr<SegmentMan> segmentMan_;

public:
  void setUp()
  {
    size_t pieceLength = 1_m;
    uint64_t totalLength = 64_m;
    option_.reset(new Option());
    dctx_.reset(new DownloadContext(pieceLength, totalLength, "aria2.tar.bz2"));
    pieceStorage_.reset(new DefaultPieceStorage(dctx_, option_.get()));
    segmentMan_.reset(new SegmentMan(dctx_, pieceStorage_));
  }

  void testNullBitfield();
  void testCompleteSegment();
  void testGetSegment_sameFileEntry();
  void testRegisterPeerStat();
  void testCancelAllSegments();
  void testCancelPartialSegmentResumesAtMissingBlock();
  void testGetPeerStat();
  void testGetCleanSegmentIfOwnerIsIdle();
};

A2_TEST(SegmentManTest, testNullBitfield)
A2_TEST(SegmentManTest, testCompleteSegment)
A2_TEST(SegmentManTest, testGetSegment_sameFileEntry)
A2_TEST(SegmentManTest, testRegisterPeerStat)
A2_TEST(SegmentManTest, testCancelAllSegments)
A2_TEST(SegmentManTest, testCancelPartialSegmentResumesAtMissingBlock)
A2_TEST(SegmentManTest, testGetPeerStat)
A2_TEST(SegmentManTest, testGetCleanSegmentIfOwnerIsIdle)

void SegmentManTest::testNullBitfield()
{
  Option op;
  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(0, 0, "aria2.tar.bz2"));
  std::shared_ptr<UnknownLengthPieceStorage> ps(
      new UnknownLengthPieceStorage(dctx));
  SegmentMan segmentMan(dctx, ps);
  size_t minSplitSize = dctx->getPieceLength();

  std::shared_ptr<Segment> segment = segmentMan.getSegment(1, minSplitSize);
  REQUIRE(segment);
  REQUIRE_EQ((size_t)0, segment->getIndex());
  REQUIRE_EQ((int64_t)0, segment->getLength());
  REQUIRE_EQ((int64_t)0, segment->getSegmentLength());
  REQUIRE_EQ((int64_t)0, segment->getWrittenLength());

  std::shared_ptr<Segment> segment2 = segmentMan.getSegment(2, minSplitSize);
  REQUIRE(!segment2);

  segmentMan.cancelSegment(1);
  REQUIRE(segmentMan.getSegment(2, minSplitSize));
}

void SegmentManTest::testCompleteSegment()
{
  Option op;
  size_t pieceLength = 1_m;
  uint64_t totalLength = 64_m;
  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(pieceLength, totalLength, "aria2.tar.bz2"));
  std::shared_ptr<DefaultPieceStorage> ps(new DefaultPieceStorage(dctx, &op));

  SegmentMan segmentMan(dctx, ps);

  REQUIRE(segmentMan.getSegmentWithIndex(1, 0));
  std::shared_ptr<Segment> seg = segmentMan.getSegmentWithIndex(1, 1);
  REQUIRE(seg);
  REQUIRE(segmentMan.getSegmentWithIndex(1, 2));

  seg->updateWrittenLength(pieceLength);
  segmentMan.completeSegment(1, seg);

  std::vector<std::shared_ptr<Segment>> segments;
  segmentMan.getInFlightSegment(segments, 1);
  REQUIRE_EQ((size_t)2, segments.size());
  REQUIRE_EQ((size_t)0, segments[0]->getIndex());
  REQUIRE_EQ((size_t)2, segments[1]->getIndex());
}

void SegmentManTest::testGetSegment_sameFileEntry()
{
  Option op;
  std::shared_ptr<DownloadContext> dctx(new DownloadContext());
  dctx->setPieceLength(2);
  std::shared_ptr<FileEntry> fileEntries[] = {
      std::shared_ptr<FileEntry>(new FileEntry("file1", 3, 0)),
      std::shared_ptr<FileEntry>(new FileEntry("file2", 6, 3)),
      std::shared_ptr<FileEntry>(new FileEntry("file3", 1, 9))};
  dctx->setFileEntries(&fileEntries[0], &fileEntries[3]);
  std::shared_ptr<DefaultPieceStorage> ps(new DefaultPieceStorage(dctx, &op));
  SegmentMan segman(dctx, ps);
  size_t minSplitSize = dctx->getPieceLength();
  std::vector<std::shared_ptr<Segment>> segments;
  segman.getSegment(segments, 1, minSplitSize, fileEntries[1], 4);
  // See 3 segments are returned, not 4 because the part of file1 is
  // not filled in segment#1
  REQUIRE_EQ((size_t)3, segments.size());

  std::shared_ptr<Segment> segmentNo1 = segman.getSegmentWithIndex(2, 1);
  // Fill the part of file1 in segment#1
  segmentNo1->updateWrittenLength(1);
  segman.cancelSegment(2);

  segman.cancelSegment(1);
  segments.clear();
  segman.getSegment(segments, 1, minSplitSize, fileEntries[1], 4);
  REQUIRE_EQ((size_t)4, segments.size());

  segman.cancelSegment(1);
  std::shared_ptr<Segment> segmentNo4 = segman.getSegmentWithIndex(1, 4);
  // Fill the part of file2 in segment#4
  segmentNo4->updateWrittenLength(1);
  segman.cancelSegment(1);

  segments.clear();
  segman.getSegment(segments, 1, minSplitSize, fileEntries[1], 4);
  // segment#4 is not returned because the part of file2 is filled.
  REQUIRE_EQ((size_t)3, segments.size());
}

void SegmentManTest::testRegisterPeerStat()
{
  Option op;
  std::shared_ptr<DownloadContext> dctx(new DownloadContext());
  std::shared_ptr<DefaultPieceStorage> ps(new DefaultPieceStorage(dctx, &op));
  SegmentMan segman(dctx, ps);

  std::shared_ptr<PeerStat> p1(new PeerStat(0, "host1", "http"));
  segman.registerPeerStat(p1);
  REQUIRE_EQ((size_t)1, segman.getPeerStats().size());
  std::shared_ptr<PeerStat> p2(new PeerStat(0, "host2", "http"));
  segman.registerPeerStat(p2);
  REQUIRE_EQ((size_t)2, segman.getPeerStats().size());
}

void SegmentManTest::testCancelAllSegments()
{
  segmentMan_->getSegmentWithIndex(1, 0);
  segmentMan_->getSegmentWithIndex(2, 1);
  REQUIRE(!segmentMan_->getSegmentWithIndex(3, 0));
  REQUIRE(!segmentMan_->getSegmentWithIndex(4, 1));
  segmentMan_->cancelAllSegments();
  REQUIRE(segmentMan_->getSegmentWithIndex(3, 0));
  REQUIRE(segmentMan_->getSegmentWithIndex(4, 1));
}

void SegmentManTest::testCancelPartialSegmentResumesAtMissingBlock()
{
  auto segment = segmentMan_->getSegmentWithIndex(1, 0);
  REQUIRE(segment);
  segment->updateWrittenLength(Piece::BLOCK_LENGTH * 2);

  segmentMan_->cancelSegment(1);

  auto resumed = segmentMan_->getSegmentWithIndex(2, 0);
  REQUIRE(resumed);
  REQUIRE_EQ(static_cast<int64_t>(Piece::BLOCK_LENGTH * 2),
                       resumed->getWrittenLength());
}

void SegmentManTest::testGetPeerStat()
{
  std::shared_ptr<PeerStat> peerStat1(new PeerStat(1));
  segmentMan_->registerPeerStat(peerStat1);
  REQUIRE_EQ((cuid_t)1, segmentMan_->getPeerStat(1)->getCuid());
}

void SegmentManTest::testGetCleanSegmentIfOwnerIsIdle()
{
  std::shared_ptr<Segment> seg1 = segmentMan_->getSegmentWithIndex(1, 0);
  std::shared_ptr<Segment> seg2 = segmentMan_->getSegmentWithIndex(2, 1);
  seg2->updateWrittenLength(100);
  REQUIRE(segmentMan_->getCleanSegmentIfOwnerIsIdle(3, 0));
  std::shared_ptr<PeerStat> peerStat3(new PeerStat(3));
  segmentMan_->registerPeerStat(peerStat3);
  REQUIRE(segmentMan_->getCleanSegmentIfOwnerIsIdle(4, 0));
  std::shared_ptr<PeerStat> peerStat4(new PeerStat(4));
  peerStat4->downloadStart();
  segmentMan_->registerPeerStat(peerStat4);
  // Owner PeerStat is not IDLE
  REQUIRE(!segmentMan_->getCleanSegmentIfOwnerIsIdle(5, 0));
  // Segment::updateWrittenLength != 0
  REQUIRE(!segmentMan_->getCleanSegmentIfOwnerIsIdle(5, 1));
}

} // namespace aria2
