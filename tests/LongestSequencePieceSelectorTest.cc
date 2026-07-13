#include "LongestSequencePieceSelector.h"

#include "a2doctest.h"

#include "array_fun.h"
#include "BitfieldMan.h"
#include "a2functional.h"

namespace aria2 {

class LongestSequencePieceSelectorTest {


public:
  void setUp() {}

  void tearDown() {}

  void testSelect();
};

A2_TEST(LongestSequencePieceSelectorTest, testSelect)

void LongestSequencePieceSelectorTest::testSelect()
{
  size_t A[] = {1, 2, 3, 4, 7, 10, 11, 12, 13, 14, 15, 100, 112, 113, 114};
  BitfieldMan bf(1_k, 256_k);
  for (size_t i = 0; i < arraySize(A); ++i) {
    bf.setBit(A[i]);
  }

  LongestSequencePieceSelector selector;
  size_t index;

  REQUIRE(selector.select(index, bf.getBitfield(), bf.countBlock()));
  REQUIRE_EQ((size_t)15, index);

  bf.clearAllBit();
  REQUIRE(!selector.select(index, bf.getBitfield(), bf.countBlock()));

  // See it works in just one range
  bf.setBitRange(1, 4);
  REQUIRE(selector.select(index, bf.getBitfield(), bf.countBlock()));
  REQUIRE_EQ((size_t)4, index);
}

} // namespace aria2
