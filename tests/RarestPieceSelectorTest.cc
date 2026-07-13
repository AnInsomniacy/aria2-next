#include "RarestPieceSelector.h"

#include "a2doctest.h"

#include "BitfieldMan.h"
#include "PieceStatMan.h"
#include "a2functional.h"

namespace aria2 {

class RarestPieceSelectorTest {


public:
  void setUp() {}

  void tearDown() {}

  void testAddPieceStats_index();
  void testAddPieceStats_bitfield();
  void testUpdatePieceStats();
  void testSubtractPieceStats();
  void testSelect();
};

A2_TEST(RarestPieceSelectorTest, testSelect)

void RarestPieceSelectorTest::testSelect()
{
  std::shared_ptr<PieceStatMan> pieceStatMan(new PieceStatMan(10, false));
  RarestPieceSelector selector(pieceStatMan);
  BitfieldMan bf(1_k, 10_k);
  bf.setBitRange(0, 2);
  size_t index;

  pieceStatMan->addPieceStats(0);

  REQUIRE(selector.select(index, bf.getBitfield(), bf.countBlock()));
  REQUIRE_EQ((size_t)1, index);

  pieceStatMan->addPieceStats(1);

  REQUIRE(selector.select(index, bf.getBitfield(), bf.countBlock()));
  REQUIRE_EQ((size_t)2, index);
}

} // namespace aria2
