#include "SegList.h"

#include "a2doctest.h"

namespace aria2 {

class SegListTest {


public:
  void testNext();
  void testPeek();
  void testClear();
  void testNormalize();
};

A2_TEST(SegListTest, testNext)
A2_TEST(SegListTest, testPeek)
A2_TEST(SegListTest, testClear)
A2_TEST(SegListTest, testNormalize)

void SegListTest::testNext()
{
  SegList<int> sgl;
  sgl.add(-500, -498);
  sgl.add(5, 10);
  sgl.add(1, 5);
  for (int i = -500; i < -498; ++i) {
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(i, sgl.next());
  }
  for (int i = 5; i < 10; ++i) {
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(i, sgl.next());
  }
  for (int i = 1; i < 5; ++i) {
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(i, sgl.next());
  }
  REQUIRE(!sgl.hasNext());
  REQUIRE_EQ(0, sgl.next());
}

void SegListTest::testPeek()
{
  SegList<int> sgl;
  sgl.add(1, 3);
  sgl.add(4, 5);
  REQUIRE_EQ(1, sgl.peek());
  REQUIRE_EQ(1, sgl.peek());
  REQUIRE_EQ(1, sgl.next());
  REQUIRE_EQ(2, sgl.peek());
  REQUIRE_EQ(2, sgl.next());
  REQUIRE_EQ(4, sgl.peek());
  REQUIRE_EQ(4, sgl.next());
  REQUIRE(!sgl.hasNext());
}

void SegListTest::testClear()
{
  SegList<int> sgl;
  sgl.add(1, 3);
  REQUIRE_EQ(1, sgl.next());
  sgl.clear();
  REQUIRE(!sgl.hasNext());
  sgl.add(2, 3);
  REQUIRE_EQ(2, sgl.next());
}

void SegListTest::testNormalize()
{
  SegList<int> sgl;
  sgl.add(10, 15);
  sgl.add(0, 1);
  sgl.add(1, 5);
  sgl.add(14, 16);
  sgl.add(2, 4);
  sgl.add(20, 21);
  sgl.normalize();
  for (int i = 0; i < 5; ++i) {
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(i, sgl.next());
  }
  for (int i = 10; i < 16; ++i) {
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(i, sgl.next());
  }
  REQUIRE(sgl.hasNext());
  REQUIRE_EQ(20, sgl.next());
  REQUIRE(!sgl.hasNext());
}

} // namespace aria2
