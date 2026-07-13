#include "PriorityPieceSelector.h"

#include "a2doctest.h"

#include "array_fun.h"
#include "BitfieldMan.h"
#include "MockPieceSelector.h"
#include "a2functional.h"

namespace aria2 {

class PriorityPieceSelectorTest {


public:
  void testSelect();
};

A2_TEST(PriorityPieceSelectorTest, testSelect)

void PriorityPieceSelectorTest::testSelect()
{
  constexpr size_t pieceLength = 1_k;
  size_t A[] = {1, 200};
  BitfieldMan bf(pieceLength, pieceLength * 256);
  for (auto i : A) {
    bf.setBit(i);
  }
  PriorityPieceSelector selector(
      std::shared_ptr<PieceSelector>(new MockPieceSelector()));
  selector.setPriorityPiece(std::begin(A), std::end(A));

  size_t index;
  REQUIRE(selector.select(index, bf.getBitfield(), bf.countBlock()));
  REQUIRE_EQ((size_t)1, index);
  bf.unsetBit(1);
  REQUIRE(selector.select(index, bf.getBitfield(), bf.countBlock()));
  REQUIRE_EQ((size_t)200, index);
  bf.unsetBit(200);
  REQUIRE(!selector.select(index, bf.getBitfield(), bf.countBlock()));
}

} // namespace aria2
