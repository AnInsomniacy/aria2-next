#include "GrowSegment.h"
#include "Piece.h"
#include "a2doctest.h"

namespace aria2 {

class GrowSegmentTest {


private:
public:
  void setUp() {}

  void testUpdateWrittenLength();
  void testClear();
};

A2_TEST(GrowSegmentTest, testUpdateWrittenLength)
A2_TEST(GrowSegmentTest, testClear)

void GrowSegmentTest::testUpdateWrittenLength()
{
  GrowSegment segment(std::shared_ptr<Piece>(new Piece()));
  segment.updateWrittenLength(32_k);

  REQUIRE_EQ((int64_t)32_k, segment.getPositionToWrite());
  REQUIRE(!segment.complete());
  REQUIRE(segment.getPiece()->pieceComplete());
}

void GrowSegmentTest::testClear()
{
  GrowSegment segment(std::shared_ptr<Piece>(new Piece()));
  segment.updateWrittenLength(32_k);
  REQUIRE_EQ((int64_t)32_k, segment.getWrittenLength());
  segment.clear(nullptr);
  REQUIRE_EQ((int64_t)0, segment.getWrittenLength());
}

} // namespace aria2
