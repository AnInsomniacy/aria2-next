#include "BitfieldMan.h"

#include <cstring>
#include <vector>

#include "a2doctest.h"

#include "bitfield.h"
#include "array_fun.h"

namespace aria2 {

class BitfieldManTest {


public:
  void testGetBlockSize();
  void testGetFirstMissingUnusedIndex();
  void testGetFirstMissingIndex();
  void testGetAllMissingIndexes();
  void testGetAllMissingIndexes_noarg();
  void testGetAllMissingIndexes_checkLastByte();
  void testGetAllMissingUnusedIndexes();

  void testIsAllBitSet();
  void testFilter();
  void testIsFilterBitSet();
  void testAddFilter_zeroLength();
  void testAddNotFilter();
  void testAddNotFilter_zeroLength();
  void testAddNotFilter_overflow();
  void testGetSparseMissingUnusedIndex();
  void testGetSparseMissingUnusedIndex_setBit();
  void testGetSparseMissingUnusedIndex_withMinSplitSize();
  void testIsBitSetOffsetRange();
  void testGetOffsetCompletedLength();
  void testGetOffsetCompletedLength_largeFile();
  void testGetMissingUnusedLength();
  void testSetBitRange();
  void testCountFilteredBlock();
  void testCountMissingBlock();
  void testZeroLengthFilter();
  void testGetFirstNMissingUnusedIndex();
  void testGetInorderMissingUnusedIndex();
  void testGetGeomMissingUnusedIndex();
};

A2_TEST(BitfieldManTest, testGetBlockSize)
A2_TEST(BitfieldManTest, testGetFirstMissingUnusedIndex)
A2_TEST(BitfieldManTest, testGetFirstMissingIndex)
A2_TEST(BitfieldManTest, testIsAllBitSet)
A2_TEST(BitfieldManTest, testFilter)
A2_TEST(BitfieldManTest, testIsFilterBitSet)
A2_TEST(BitfieldManTest, testAddFilter_zeroLength)
A2_TEST(BitfieldManTest, testAddNotFilter)
A2_TEST(BitfieldManTest, testAddNotFilter_zeroLength)
A2_TEST(BitfieldManTest, testAddNotFilter_overflow)
A2_TEST(BitfieldManTest, testGetSparseMissingUnusedIndex)
A2_TEST(BitfieldManTest, testGetSparseMissingUnusedIndex_setBit)
A2_TEST(BitfieldManTest, testGetSparseMissingUnusedIndex_withMinSplitSize)
A2_TEST(BitfieldManTest, testIsBitSetOffsetRange)
A2_TEST(BitfieldManTest, testGetOffsetCompletedLength)
A2_TEST(BitfieldManTest, testGetOffsetCompletedLength_largeFile)
A2_TEST(BitfieldManTest, testGetMissingUnusedLength)
A2_TEST(BitfieldManTest, testSetBitRange)
A2_TEST(BitfieldManTest, testGetAllMissingIndexes)
A2_TEST(BitfieldManTest, testGetAllMissingIndexes_noarg)
A2_TEST(BitfieldManTest, testGetAllMissingIndexes_checkLastByte)
A2_TEST(BitfieldManTest, testGetAllMissingUnusedIndexes)
A2_TEST(BitfieldManTest, testCountFilteredBlock)
A2_TEST(BitfieldManTest, testCountMissingBlock)
A2_TEST(BitfieldManTest, testZeroLengthFilter)
A2_TEST(BitfieldManTest, testGetFirstNMissingUnusedIndex)
A2_TEST(BitfieldManTest, testGetInorderMissingUnusedIndex)
A2_TEST(BitfieldManTest, testGetGeomMissingUnusedIndex)

void BitfieldManTest::testGetBlockSize()
{
  BitfieldMan bt1(1_k, 10_k);
  REQUIRE_EQ((int32_t)1_k, bt1.getBlockLength(9));

  BitfieldMan bt2(1_k, 10_k + 1);
  REQUIRE_EQ((int32_t)1_k, bt2.getBlockLength(9));
  REQUIRE_EQ((int32_t)1, bt2.getBlockLength(10));
  REQUIRE_EQ((int32_t)0, bt2.getBlockLength(11));
}

void BitfieldManTest::testGetFirstMissingUnusedIndex()
{
  {
    BitfieldMan bt1(1_k, 10_k);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingUnusedIndex(index));
      REQUIRE_EQ((size_t)0, index);
    }
    bt1.setUseBit(0);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingUnusedIndex(index));
      REQUIRE_EQ((size_t)1, index);
    }
    bt1.unsetUseBit(0);
    bt1.setBit(0);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingUnusedIndex(index));
      REQUIRE_EQ((size_t)1, index);
    }
    bt1.setAllBit();
    {
      size_t index;
      REQUIRE(!bt1.getFirstMissingUnusedIndex(index));
    }
  }
  {
    BitfieldMan bt1(1_k, 10_k);

    bt1.addFilter(1_k, 10_k);
    bt1.enableFilter();
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingUnusedIndex(index));
      REQUIRE_EQ((size_t)1, index);
    }
    bt1.setUseBit(1);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingUnusedIndex(index));
      REQUIRE_EQ((size_t)2, index);
    }
    bt1.setBit(2);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingUnusedIndex(index));
      REQUIRE_EQ((size_t)3, index);
    }
  }
}

void BitfieldManTest::testGetFirstMissingIndex()
{
  {
    BitfieldMan bt1(1_k, 10_k);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingIndex(index));
      REQUIRE_EQ((size_t)0, index);
    }
    bt1.setUseBit(0);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingIndex(index));
      REQUIRE_EQ((size_t)0, index);
    }
    bt1.unsetUseBit(0);
    bt1.setBit(0);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingIndex(index));
      REQUIRE_EQ((size_t)1, index);
    }
    bt1.setAllBit();
    {
      size_t index;
      REQUIRE(!bt1.getFirstMissingIndex(index));
    }
  }
  {
    BitfieldMan bt1(1_k, 10_k);

    bt1.addFilter(1_k, 10_k);
    bt1.enableFilter();
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingIndex(index));
      REQUIRE_EQ((size_t)1, index);
    }
    bt1.setUseBit(1);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingIndex(index));
      REQUIRE_EQ((size_t)1, index);
    }
    bt1.setBit(1);
    {
      size_t index;
      REQUIRE(bt1.getFirstMissingIndex(index));
      REQUIRE_EQ((size_t)2, index);
    }
  }
}

void BitfieldManTest::testIsAllBitSet()
{
  BitfieldMan bt1(1_k, 10_k);
  REQUIRE(!bt1.isAllBitSet());
  bt1.setBit(1);
  REQUIRE(!bt1.isAllBitSet());

  for (size_t i = 0; i < 8; i++) {
    REQUIRE(bt1.setBit(i));
  }
  REQUIRE(!bt1.isAllBitSet());

  for (size_t i = 0; i < bt1.countBlock(); i++) {
    REQUIRE(bt1.setBit(i));
  }
  REQUIRE(bt1.isAllBitSet());

  BitfieldMan btzero(1_k, 0);
  REQUIRE(btzero.isAllBitSet());
}

void BitfieldManTest::testFilter()
{
  BitfieldMan btman(2, 32);
  // test offset=4, length=12
  btman.addFilter(4, 12);
  btman.enableFilter();
  std::vector<size_t> out;
  REQUIRE_EQ((size_t)6, btman.getFirstNMissingUnusedIndex(out, 32));
  const size_t ans[] = {2, 3, 4, 5, 6, 7};
  for (size_t i = 0; i < arraySize(ans); ++i) {
    REQUIRE_EQ(ans[i], out[i]);
  }
  REQUIRE_EQ((int64_t)12ULL, btman.getFilteredTotalLength());

  // test offset=5, length=2
  out.clear();
  btman.clearAllBit();
  btman.clearAllUseBit();
  btman.clearFilter();
  btman.addFilter(5, 2);
  btman.enableFilter();
  REQUIRE_EQ((size_t)2, btman.getFirstNMissingUnusedIndex(out, 32));
  REQUIRE_EQ((size_t)2, out[0]);
  REQUIRE_EQ((size_t)3, out[1]);
  btman.setBit(2);
  btman.setBit(3);
  REQUIRE_EQ((int64_t)4ULL, btman.getFilteredTotalLength());
  REQUIRE(btman.isFilteredAllBitSet());

  BitfieldMan btman2(2, 31);
  btman2.addFilter(0, 31);
  btman2.enableFilter();
  REQUIRE_EQ((int64_t)31ULL, btman2.getFilteredTotalLength());
}

void BitfieldManTest::testIsFilterBitSet()
{
  BitfieldMan btman(2, 32);
  REQUIRE(!btman.isFilterBitSet(0));
  btman.addFilter(0, 2);
  REQUIRE(btman.isFilterBitSet(0));
  REQUIRE(!btman.isFilterBitSet(1));
  btman.addFilter(2, 4);
  REQUIRE(btman.isFilterBitSet(1));
}

void BitfieldManTest::testAddFilter_zeroLength()
{
  BitfieldMan bits(1_k, 1_m);
  bits.addFilter(2_k, 0);
  bits.enableFilter();
  REQUIRE_EQ((size_t)0, bits.countMissingBlock());
  REQUIRE(bits.isFilteredAllBitSet());
}

void BitfieldManTest::testAddNotFilter()
{
  BitfieldMan btman(2, 32);

  btman.addNotFilter(3, 6);
  REQUIRE(bitfield::test(btman.getFilterBitfield(), 16, 0));
  for (size_t i = 1; i < 5; ++i) {
    REQUIRE(!bitfield::test(btman.getFilterBitfield(), 16, i));
  }
  for (size_t i = 5; i < 16; ++i) {
    REQUIRE(bitfield::test(btman.getFilterBitfield(), 16, i));
  }
}

void BitfieldManTest::testAddNotFilter_zeroLength()
{
  BitfieldMan btman(2, 6);
  btman.addNotFilter(2, 0);
  REQUIRE(!bitfield::test(btman.getFilterBitfield(), 3, 0));
  REQUIRE(!bitfield::test(btman.getFilterBitfield(), 3, 1));
  REQUIRE(!bitfield::test(btman.getFilterBitfield(), 3, 2));
}

void BitfieldManTest::testAddNotFilter_overflow()
{
  BitfieldMan btman(2, 6);
  btman.addNotFilter(6, 100);
  REQUIRE(bitfield::test(btman.getFilterBitfield(), 3, 0));
  REQUIRE(bitfield::test(btman.getFilterBitfield(), 3, 1));
  REQUIRE(bitfield::test(btman.getFilterBitfield(), 3, 2));
}

// TODO1.5 add test using ignoreBitfield
void BitfieldManTest::testGetSparseMissingUnusedIndex()
{
  BitfieldMan bitfield(1_m, 10_m);
  const size_t length = 2;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_m;
  size_t index;
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)0, index);
  bitfield.setUseBit(0);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)5, index);
  bitfield.setUseBit(5);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)3, index);
  bitfield.setUseBit(3);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)8, index);
  bitfield.setUseBit(8);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)2, index);
  bitfield.setUseBit(2);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)1, index);
  bitfield.setUseBit(1);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)4, index);
  bitfield.setUseBit(4);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)7, index);
  bitfield.setUseBit(7);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)6, index);
  bitfield.setUseBit(6);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)9, index);
  bitfield.setUseBit(9);
  REQUIRE(!bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                       ignoreBitfield, length));
}

void BitfieldManTest::testGetSparseMissingUnusedIndex_setBit()
{
  BitfieldMan bitfield(1_m, 10_m);
  const size_t length = 2;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_m;
  size_t index;
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)0, index);
  bitfield.setBit(0);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)1, index);
  bitfield.setBit(1);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)2, index);
  bitfield.setBit(2);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)3, index);
  bitfield.setBit(3);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)4, index);
  bitfield.setBit(4);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)5, index);
  bitfield.setBit(5);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)6, index);
  bitfield.setBit(6);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)7, index);
  bitfield.setBit(7);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)8, index);
  bitfield.setBit(8);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)9, index);
  bitfield.setBit(9);
  REQUIRE(!bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                       ignoreBitfield, length));
}

void BitfieldManTest::testGetSparseMissingUnusedIndex_withMinSplitSize()
{
  BitfieldMan bitfield(1_m, 10_m);
  const size_t length = 2;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 2_m;
  size_t index;
  bitfield.setUseBit(1);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)6, index);
  bitfield.setBit(6);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)7, index);
  bitfield.setUseBit(7);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)4, index);
  bitfield.setBit(4);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)0, index);
  bitfield.setBit(0);
  REQUIRE(bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                      ignoreBitfield, length));
  REQUIRE_EQ((size_t)5, index);
  bitfield.setBit(5);
  REQUIRE(!bitfield.getSparseMissingUnusedIndex(index, minSplitSize,
                                                       ignoreBitfield, length));
}

void BitfieldManTest::testIsBitSetOffsetRange()
{
  int64_t totalLength = 4_g;
  int32_t pieceLength = 4_m;
  BitfieldMan bitfield(pieceLength, totalLength);
  bitfield.setAllBit();

  REQUIRE(!bitfield.isBitSetOffsetRange(0, 0));
  REQUIRE(!bitfield.isBitSetOffsetRange(totalLength, 100));
  REQUIRE(!bitfield.isBitSetOffsetRange(totalLength + 1, 100));

  REQUIRE(bitfield.isBitSetOffsetRange(0, totalLength));
  REQUIRE(bitfield.isBitSetOffsetRange(0, totalLength + 1));

  bitfield.clearAllBit();

  bitfield.setBit(100);
  bitfield.setBit(101);

  REQUIRE(
      bitfield.isBitSetOffsetRange(pieceLength * 100, pieceLength * 2));
  REQUIRE(
      !bitfield.isBitSetOffsetRange(pieceLength * 100 - 10, pieceLength * 2));
  REQUIRE(
      !bitfield.isBitSetOffsetRange(pieceLength * 100, pieceLength * 2 + 1));

  bitfield.clearAllBit();

  bitfield.setBit(100);
  bitfield.setBit(102);

  REQUIRE(
      !bitfield.isBitSetOffsetRange(pieceLength * 100, pieceLength * 3));
}

void BitfieldManTest::testGetOffsetCompletedLength()
{
  BitfieldMan bt(1_k, 20_k);
  // 00000|00000|00000|00000
  REQUIRE_EQ((int64_t)0, bt.getOffsetCompletedLength(0, 1_k));
  REQUIRE_EQ((int64_t)0, bt.getOffsetCompletedLength(0, 0));
  for (size_t i = 2; i <= 4; ++i) {
    bt.setBit(i);
  }
  // 00111|00000|00000|00000
  REQUIRE_EQ((int64_t)3072, bt.getOffsetCompletedLength(2048, 3072));
  REQUIRE_EQ((int64_t)3071, bt.getOffsetCompletedLength(2047, 3072));
  REQUIRE_EQ((int64_t)3071, bt.getOffsetCompletedLength(2049, 3072));
  REQUIRE_EQ((int64_t)0, bt.getOffsetCompletedLength(2048, 0));
  REQUIRE_EQ((int64_t)1, bt.getOffsetCompletedLength(2048, 1));
  REQUIRE_EQ((int64_t)0, bt.getOffsetCompletedLength(2047, 1));
  REQUIRE_EQ((int64_t)3072, bt.getOffsetCompletedLength(0, 20_k));
  REQUIRE_EQ((int64_t)3072,
                       bt.getOffsetCompletedLength(0, 20_k + 10));
  REQUIRE_EQ((int64_t)0, bt.getOffsetCompletedLength(20_k, 1));
}

void BitfieldManTest::testGetOffsetCompletedLength_largeFile()
{
  // Test for overflow on 32-bit systems.

  // Total 4TiB, 4MiB block
  BitfieldMan bt(1 << 22, 1LL << 40);
  bt.setBit(1 << 11);
  bt.setBit((1 << 11) + 1);
  bt.setBit((1 << 11) + 2);

  // The last piece is missing:
  REQUIRE_EQ((int64_t)bt.getBlockLength() * 3,
                       bt.getOffsetCompletedLength(1LL << 33, 1 << 24));

  // The first piece is missing:
  REQUIRE_EQ(
      (int64_t)bt.getBlockLength() * 3,
      bt.getOffsetCompletedLength((1LL << 33) - bt.getBlockLength(), 1 << 24));
}

void BitfieldManTest::testGetMissingUnusedLength()
{
  int64_t totalLength = 10_k + 10;
  size_t blockLength = 1_k;

  BitfieldMan bf(blockLength, totalLength);

  // from index 0 and all blocks are unused and not acquired.
  REQUIRE_EQ(totalLength, bf.getMissingUnusedLength(0));

  // from index 10 and all blocks are unused and not acquired.
  REQUIRE_EQ((int64_t)10ULL, bf.getMissingUnusedLength(10));

  // from index 11
  REQUIRE_EQ((int64_t)0ULL, bf.getMissingUnusedLength(11));

  // from index 12
  REQUIRE_EQ((int64_t)0ULL, bf.getMissingUnusedLength(12));

  // from index 0 and 5th block is used.
  bf.setUseBit(5);
  REQUIRE_EQ((int64_t)(5LL * blockLength),
                       bf.getMissingUnusedLength(0));

  // from index 0 and 4th block is acquired.
  bf.setBit(4);
  REQUIRE_EQ((int64_t)(4LL * blockLength),
                       bf.getMissingUnusedLength(0));

  // from index 1
  REQUIRE_EQ((int64_t)(3LL * blockLength),
                       bf.getMissingUnusedLength(1));
}

void BitfieldManTest::testSetBitRange()
{
  size_t blockLength = 1_m;
  int64_t totalLength = 10 * blockLength;

  BitfieldMan bf(blockLength, totalLength);

  bf.setBitRange(0, 4);

  for (size_t i = 0; i < 5; ++i) {
    REQUIRE(bf.isBitSet(i));
  }
  for (size_t i = 5; i < 10; ++i) {
    REQUIRE(!bf.isBitSet(i));
  }
  REQUIRE_EQ((int64_t)(5LL * blockLength), bf.getCompletedLength());
}

void BitfieldManTest::testGetAllMissingIndexes_noarg()
{
  size_t blockLength = 16_k;
  int64_t totalLength = 1_m;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  unsigned char misbitfield[8];
  REQUIRE(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield)));
  REQUIRE_EQ((size_t)64, bitfield::countSetBit(misbitfield, nbits));

  for (size_t i = 0; i < 63; ++i) {
    bf.setBit(i);
  }
  REQUIRE(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield)));
  REQUIRE_EQ((size_t)1, bitfield::countSetBit(misbitfield, nbits));
  REQUIRE(bitfield::test(misbitfield, nbits, 63));
}

// See garbage bits of last byte are 0
void BitfieldManTest::testGetAllMissingIndexes_checkLastByte()
{
  size_t blockLength = 16_k;
  int64_t totalLength = blockLength * 2;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  unsigned char misbitfield[1];
  REQUIRE(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield)));
  REQUIRE_EQ((size_t)2, bitfield::countSetBit(misbitfield, nbits));
  REQUIRE(bitfield::test(misbitfield, nbits, 0));
  REQUIRE(bitfield::test(misbitfield, nbits, 1));
}

void BitfieldManTest::testGetAllMissingIndexes()
{
  size_t blockLength = 16_k;
  int64_t totalLength = 1_m;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  BitfieldMan peerBf(blockLength, totalLength);
  peerBf.setAllBit();
  unsigned char misbitfield[8];

  REQUIRE(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield),
                                         peerBf.getBitfield(),
                                         peerBf.getBitfieldLength()));
  REQUIRE_EQ((size_t)64, bitfield::countSetBit(misbitfield, nbits));
  for (size_t i = 0; i < 62; ++i) {
    bf.setBit(i);
  }
  peerBf.unsetBit(62);

  REQUIRE(bf.getAllMissingIndexes(misbitfield, sizeof(misbitfield),
                                         peerBf.getBitfield(),
                                         peerBf.getBitfieldLength()));
  REQUIRE_EQ((size_t)1, bitfield::countSetBit(misbitfield, nbits));
  REQUIRE(bitfield::test(misbitfield, nbits, 63));
}

void BitfieldManTest::testGetAllMissingUnusedIndexes()
{
  size_t blockLength = 16_k;
  int64_t totalLength = 1_m;
  size_t nbits = (totalLength + blockLength - 1) / blockLength;
  BitfieldMan bf(blockLength, totalLength);
  BitfieldMan peerBf(blockLength, totalLength);
  peerBf.setAllBit();
  unsigned char misbitfield[8];

  REQUIRE(bf.getAllMissingUnusedIndexes(misbitfield, sizeof(misbitfield),
                                               peerBf.getBitfield(),
                                               peerBf.getBitfieldLength()));
  REQUIRE_EQ((size_t)64, bitfield::countSetBit(misbitfield, nbits));

  for (size_t i = 0; i < 61; ++i) {
    bf.setBit(i);
  }
  bf.setUseBit(61);
  peerBf.unsetBit(62);
  REQUIRE(bf.getAllMissingUnusedIndexes(misbitfield, sizeof(misbitfield),
                                               peerBf.getBitfield(),
                                               peerBf.getBitfieldLength()));
  REQUIRE_EQ((size_t)1, bitfield::countSetBit(misbitfield, nbits));
  REQUIRE(bitfield::test(misbitfield, nbits, 63));
}

void BitfieldManTest::testCountFilteredBlock()
{
  BitfieldMan bt(1_k, 256_k);
  REQUIRE_EQ((size_t)256, bt.countBlock());
  REQUIRE_EQ((size_t)0, bt.countFilteredBlock());
  bt.addFilter(1_k, 256_k);
  bt.enableFilter();
  REQUIRE_EQ((size_t)256, bt.countBlock());
  REQUIRE_EQ((size_t)255, bt.countFilteredBlock());
  bt.disableFilter();
  REQUIRE_EQ((size_t)256, bt.countBlock());
  REQUIRE_EQ((size_t)0, bt.countFilteredBlock());
}

void BitfieldManTest::testCountMissingBlock()
{
  BitfieldMan bt(1_k, 10_k);
  REQUIRE_EQ((size_t)10, bt.countMissingBlock());
  bt.setBit(1);
  REQUIRE_EQ((size_t)9, bt.countMissingBlock());
  bt.setAllBit();
  REQUIRE_EQ((size_t)0, bt.countMissingBlock());
}

void BitfieldManTest::testZeroLengthFilter()
{
  BitfieldMan bt(1_k, 10_k);
  bt.enableFilter();
  REQUIRE_EQ((size_t)0, bt.countMissingBlock());
}

void BitfieldManTest::testGetFirstNMissingUnusedIndex()
{
  BitfieldMan bt(1_k, 10_k);
  bt.setUseBit(1);
  bt.setBit(5);
  std::vector<size_t> out;
  REQUIRE_EQ((size_t)8, bt.getFirstNMissingUnusedIndex(out, 256));
  REQUIRE_EQ((size_t)8, out.size());
  const size_t ans[] = {0, 2, 3, 4, 6, 7, 8, 9};
  for (size_t i = 0; i < out.size(); ++i) {
    REQUIRE_EQ(ans[i], out[i]);
  }
  out.clear();
  REQUIRE_EQ((size_t)3, bt.getFirstNMissingUnusedIndex(out, 3));
  REQUIRE_EQ((size_t)3, out.size());
  for (size_t i = 0; i < out.size(); ++i) {
    REQUIRE_EQ(ans[i], out[i]);
  }
  REQUIRE_EQ((size_t)0, bt.getFirstNMissingUnusedIndex(out, 0));
  bt.setAllBit();
  REQUIRE_EQ((size_t)0, bt.getFirstNMissingUnusedIndex(out, 10));
  bt.clearAllBit();
  out.clear();
  bt.addFilter(9_k, 1_k);
  bt.enableFilter();
  REQUIRE_EQ((size_t)1, bt.getFirstNMissingUnusedIndex(out, 256));
  REQUIRE_EQ((size_t)1, out.size());
  REQUIRE_EQ((size_t)9, out[0]);
}

void BitfieldManTest::testGetInorderMissingUnusedIndex()
{
  BitfieldMan bt(1_k, 20_k);
  const size_t length = 3;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_k;
  size_t index;
  // 00000|00000|00000|00000
  REQUIRE(bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                 ignoreBitfield, length));
  REQUIRE_EQ((size_t)0, index);
  bt.setUseBit(0);
  // 10000|00000|00000|00000
  REQUIRE(bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                 ignoreBitfield, length));
  REQUIRE_EQ((size_t)1, index);
  minSplitSize = 2_k;
  REQUIRE(bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                 ignoreBitfield, length));
  REQUIRE_EQ((size_t)2, index);
  bt.unsetUseBit(0);
  bt.setBit(0);
  REQUIRE(bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                 ignoreBitfield, length));
  REQUIRE_EQ((size_t)1, index);
  bt.setAllBit();
  bt.unsetBit(10);
  // 11111|11111|01111|11111
  REQUIRE(bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                 ignoreBitfield, length));
  REQUIRE_EQ((size_t)10, index);
  bt.setUseBit(10);
  REQUIRE(!bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                  ignoreBitfield, length));
  bt.unsetUseBit(10);
  bt.setAllBit();
  // 11111|11111|11111|11111
  REQUIRE(!bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                  ignoreBitfield, length));
  bt.clearAllBit();
  // 00000|00000|00000|00000
  for (int i = 0; i <= 1; ++i) {
    bitfield::flipBit(ignoreBitfield, length, i);
  }
  REQUIRE(bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                 ignoreBitfield, length));
  REQUIRE_EQ((size_t)2, index);
  bt.addFilter(3_k, 3_k);
  bt.enableFilter();
  REQUIRE(bt.getInorderMissingUnusedIndex(index, minSplitSize,
                                                 ignoreBitfield, length));
  REQUIRE_EQ((size_t)3, index);
}

void BitfieldManTest::testGetGeomMissingUnusedIndex()
{
  BitfieldMan bt(1_k, 20_k);
  const size_t length = 3;
  unsigned char ignoreBitfield[length];
  memset(ignoreBitfield, 0, sizeof(ignoreBitfield));
  size_t minSplitSize = 1_k;
  size_t index;
  // 00000|00000|00000|00000
  REQUIRE(bt.getGeomMissingUnusedIndex(index, minSplitSize,
                                              ignoreBitfield, length, 2, 0));
  REQUIRE_EQ((size_t)0, index);
  bt.setUseBit(0);
  // 10000|00000|00000|00000
  REQUIRE(bt.getGeomMissingUnusedIndex(index, minSplitSize,
                                              ignoreBitfield, length, 2, 0));
  REQUIRE_EQ((size_t)1, index);
  bt.setUseBit(1);
  // 11000|00000|00000|00000
  REQUIRE(bt.getGeomMissingUnusedIndex(index, minSplitSize,
                                              ignoreBitfield, length, 2, 0));
  REQUIRE_EQ((size_t)2, index);
  bt.setUseBit(2);
  // 11100|00000|00000|00000
  REQUIRE(bt.getGeomMissingUnusedIndex(index, minSplitSize,
                                              ignoreBitfield, length, 2, 0));
  REQUIRE_EQ((size_t)4, index);
  bt.setUseBit(4);
  // 11110|00000|00000|00000
  REQUIRE(bt.getGeomMissingUnusedIndex(index, minSplitSize,
                                              ignoreBitfield, length, 2, 0));
  REQUIRE_EQ((size_t)8, index);
  bt.setUseBit(8);
  // 11110|00010|00000|00000
  REQUIRE(bt.getGeomMissingUnusedIndex(index, minSplitSize,
                                              ignoreBitfield, length, 2, 0));
  REQUIRE_EQ((size_t)16, index);
  bt.setUseBit(16);
  // 11110|00010|00000|01000
  REQUIRE(bt.getGeomMissingUnusedIndex(index, minSplitSize,
                                              ignoreBitfield, length, 2, 0));
  REQUIRE_EQ((size_t)12, index);
  bt.setUseBit(12);
}

} // namespace aria2
