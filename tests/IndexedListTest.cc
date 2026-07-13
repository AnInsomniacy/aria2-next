#include "IndexedList.h"

#include <vector>
#include <deque>
#include <iostream>

#include "a2doctest.h"

#include "TestUtil.h"
#include "array_fun.h"
#include "TimerA2.h"

namespace aria2 {

class IndexedListTest {


public:
  void setUp() {}

  void testPushBack();
  void testPushFront();
  void testRemove();
  void testErase();
  void testPopFront();
  void testMove();
  void testGet();
  void testInsert();
  void testInsert_keyFunc();
  void testIterator();
  void testRemoveIf();
};

A2_TEST(IndexedListTest, testPushBack)
A2_TEST(IndexedListTest, testPushFront)
A2_TEST(IndexedListTest, testRemove)
A2_TEST(IndexedListTest, testErase)
A2_TEST(IndexedListTest, testPopFront)
A2_TEST(IndexedListTest, testMove)
A2_TEST(IndexedListTest, testGet)
A2_TEST(IndexedListTest, testInsert)
A2_TEST(IndexedListTest, testInsert_keyFunc)
A2_TEST(IndexedListTest, testIterator)
A2_TEST(IndexedListTest, testRemoveIf)

void IndexedListTest::testPushBack()
{
  int a[] = {1, 2, 3, 4, 5};
  IndexedList<int, int*> list;
  for (int i = 0; i < 5; ++i) {
    REQUIRE(list.push_back(i, &a[i]));
  }
  for (int i = 0; i < 5; ++i) {
    REQUIRE_EQ(a[i], *list.get(i));
  }
  int ai = 0;
  for (IndexedList<int, int*>::iterator i = list.begin(); i != list.end();
       ++i) {
    REQUIRE_EQ(a[ai++], **i);
  }
}

void IndexedListTest::testPushFront()
{
  int a[] = {1, 2, 3, 4, 5};
  IndexedList<int, int*> list;
  for (int i = 0; i < 5; ++i) {
    REQUIRE(list.push_front(i, &a[i]));
  }
  for (int i = 0; i < 5; ++i) {
    REQUIRE_EQ(a[i], *list.get(i));
  }
  int ai = 4;
  for (IndexedList<int, int*>::iterator i = list.begin(); i != list.end();
       ++i) {
    REQUIRE_EQ(a[ai--], **i);
  }
}

void IndexedListTest::testRemove()
{
  int a[] = {1, 2, 3, 4, 5};
  IndexedList<int, int*> list;
  for (int i = 0; i < 5; ++i) {
    list.push_back(i, &a[i]);
  }
  for (int i = 0; i < 5; ++i) {
    REQUIRE(list.remove(i));
    REQUIRE_EQ((size_t)5 - i - 1, list.size());
    for (int j = i + 1; j < 5; ++j) {
      REQUIRE_EQ(a[j], *list.get(j));
    }
  }
}

void IndexedListTest::testErase()
{
  int a[] = {1, 2, 3, 4, 5};
  IndexedList<int, int*> list;
  for (int i = 0; i < 5; ++i) {
    list.push_back(i, &a[i]);
  }
  int* p = a;
  for (IndexedList<int, int*>::iterator i = list.begin(); i != list.end();) {
    i = list.erase(i);
    REQUIRE_EQ((size_t)(std::distance(i, list.end())), list.size());

    int* pp = ++p;
    for (IndexedList<int, int*>::iterator j = list.begin(); j != list.end();
         ++j, ++pp) {
      REQUIRE_EQ(*pp, **j);
    }
  }
}

void IndexedListTest::testPopFront()
{
  int a[] = {1, 2, 3, 4, 5};
  IndexedList<int, int*> list;
  for (int i = 0; i < 5; ++i) {
    list.push_back(i, &a[i]);
  }
  for (int i = 0; i < 5; ++i) {
    REQUIRE(list.pop_front());
    REQUIRE_EQ((size_t)5 - i - 1, list.size());
    for (int j = i + 1; j < 5; ++j) {
      REQUIRE_EQ(a[j], *list.get(j));
    }
  }
}

#define LIST_CHECK(a, list)                                                    \
  {                                                                            \
    int ai = 0;                                                                \
    for (IndexedList<int, int*>::iterator i = list.begin(); i != list.end();   \
         ++i) {                                                                \
      REQUIRE_EQ(a[ai++], **i);                                      \
    }                                                                          \
  }

void IndexedListTest::testMove()
{
  int a[] = {0, 1, 2, 3, 4};
  IndexedList<int, int*> list;
  for (int i = 0; i < 5; ++i) {
    list.push_back(i, &a[i]);
  }
  REQUIRE_EQ((ssize_t)-1, list.move(100, 0, OFFSET_MODE_SET));
  int a0[] = {0, 1, 2, 3, 4};
  REQUIRE_EQ((ssize_t)0, list.move(0, 0, OFFSET_MODE_SET));
  LIST_CHECK(a0, list);

  int a1[] = {0, 2, 3, 4, 1};
  REQUIRE_EQ((ssize_t)4, list.move(1, 4, OFFSET_MODE_SET));
  LIST_CHECK(a1, list);

  int a2[] = {0, 3, 4, 2, 1};
  REQUIRE_EQ((ssize_t)3, list.move(2, 3, OFFSET_MODE_SET));
  LIST_CHECK(a2, list);

  int a3[] = {0, 2, 3, 4, 1};
  REQUIRE_EQ((ssize_t)1, list.move(2, 1, OFFSET_MODE_SET));
  LIST_CHECK(a3, list);

  int a4[] = {1, 0, 2, 3, 4};
  REQUIRE_EQ((ssize_t)0, list.move(1, 0, OFFSET_MODE_SET));
  LIST_CHECK(a4, list);

  int a5[] = {1, 0, 3, 2, 4};
  REQUIRE_EQ((ssize_t)2, list.move(3, 2, OFFSET_MODE_SET));
  LIST_CHECK(a5, list);

  int a6[] = {1, 3, 2, 4, 0};
  REQUIRE_EQ((ssize_t)4, list.move(0, 5, OFFSET_MODE_SET));
  LIST_CHECK(a6, list);

  int a7[] = {3, 1, 2, 4, 0};
  REQUIRE_EQ((ssize_t)1, list.move(1, 1, OFFSET_MODE_CUR));
  LIST_CHECK(a7, list);

  int a8[] = {3, 2, 4, 1, 0};
  REQUIRE_EQ((ssize_t)3, list.move(1, 2, OFFSET_MODE_CUR));
  LIST_CHECK(a8, list);

  int a9[] = {3, 2, 1, 4, 0};
  REQUIRE_EQ((ssize_t)2, list.move(1, -1, OFFSET_MODE_CUR));
  LIST_CHECK(a9, list);

  int a10[] = {1, 3, 2, 4, 0};
  REQUIRE_EQ((ssize_t)0, list.move(1, -1233, OFFSET_MODE_CUR));
  LIST_CHECK(a10, list);

  int a11[] = {3, 2, 4, 0, 1};
  REQUIRE_EQ((ssize_t)4, list.move(1, 8733, OFFSET_MODE_CUR));
  LIST_CHECK(a11, list);

  int a12[] = {3, 2, 4, 0, 1};
  REQUIRE_EQ((ssize_t)3, list.move(0, -1, OFFSET_MODE_END));
  LIST_CHECK(a12, list);

  int a13[] = {3, 2, 0, 4, 1};
  REQUIRE_EQ((ssize_t)2, list.move(0, -2, OFFSET_MODE_END));
  LIST_CHECK(a13, list);

  int a14[] = {0, 3, 2, 4, 1};
  REQUIRE_EQ((ssize_t)0, list.move(0, -8733, OFFSET_MODE_END));
  LIST_CHECK(a14, list);

  int a15[] = {0, 2, 4, 1, 3};
  REQUIRE_EQ((ssize_t)4, list.move(3, 0, OFFSET_MODE_END));
  LIST_CHECK(a15, list);

  int a16[] = {2, 4, 1, 3, 0};
  REQUIRE_EQ((ssize_t)4, list.move(0, 1000, OFFSET_MODE_END));
  LIST_CHECK(a16, list);

  int a17[] = {2, 1, 4, 3, 0};
  REQUIRE_EQ((ssize_t)2, list.move(4, 2, OFFSET_MODE_SET));
  LIST_CHECK(a17, list);
}

void IndexedListTest::testGet()
{
  IndexedList<int, int*> list;
  int a = 1000;
  int b = 1;
  list.push_back(123, &a);
  list.push_back(1, &b);
  REQUIRE_EQ((int*)nullptr, list.get(1000));
  REQUIRE_EQ(&a, list.get(123));
}

namespace {
struct KeyFunc {
  int n;
  KeyFunc(int n) : n(n) {}
  int operator()(const std::shared_ptr<std::string>& x) { return n++; }
};
} // namespace

void IndexedListTest::testInsert_keyFunc()
{
  std::shared_ptr<std::string> s[] = {
      std::shared_ptr<std::string>(new std::string("a")),
      std::shared_ptr<std::string>(new std::string("b")),
      std::shared_ptr<std::string>(new std::string("c")),
      std::shared_ptr<std::string>(new std::string("d"))};
  size_t slen = sizeof(s) / sizeof(s[0]);
  IndexedList<int, std::shared_ptr<std::string>> list;
  list.insert(list.begin(), KeyFunc(0), std::begin(s), std::end(s));
  REQUIRE_EQ((size_t)slen, list.size());
  for (size_t i = 0; i < slen; ++i) {
    REQUIRE_EQ(*s[i], *list.get(i));
  }
  list.insert(list.begin() + 2, KeyFunc(slen), std::begin(s), std::end(s));
  REQUIRE_EQ((size_t)slen * 2, list.size());
  for (size_t i = slen; i < slen * 2; ++i) {
    REQUIRE_EQ(*s[i - slen], *list.get(i));
  }
  IndexedList<int, std::shared_ptr<std::string>>::iterator itr;
  itr = list.begin();
  REQUIRE_EQ(std::string("a"), *(*itr++));
  REQUIRE_EQ(std::string("b"), *(*itr++));
  REQUIRE_EQ(std::string("a"), *(*itr++));
  REQUIRE_EQ(std::string("b"), *(*itr++));
  REQUIRE_EQ(std::string("c"), *(*itr++));
  REQUIRE_EQ(std::string("d"), *(*itr++));
  REQUIRE_EQ(std::string("c"), *(*itr++));
  REQUIRE_EQ(std::string("d"), *(*itr++));

  list.insert(list.begin(), KeyFunc(2 * slen - 1), std::begin(s), std::end(s));
  REQUIRE_EQ((size_t)slen * 3 - 1, list.size());
  itr = list.begin();
  REQUIRE_EQ(std::string("b"), *(*itr++));
  REQUIRE_EQ(std::string("c"), *(*itr++));
  REQUIRE_EQ(std::string("d"), *(*itr++));
  REQUIRE_EQ(std::string("a"), *(*itr++));
}

void IndexedListTest::testInsert()
{
  int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  IndexedList<int, int*> list;
  IndexedList<int, int*>::iterator itr;
  REQUIRE(list.end() == list.insert(1, 0, &a[5]));
  itr = list.insert(0, 5, &a[5]);
  REQUIRE_EQ(5, **itr);
  itr = list.insert(1, 3, &a[3]);
  REQUIRE_EQ(3, **itr);
  itr = list.insert(1, 4, &a[4]);
  REQUIRE_EQ(4, **itr);
  itr = list.insert(0, 9, &a[9]);
  REQUIRE_EQ(9, **itr);
  int a1[] = {9, 5, 4, 3};
  LIST_CHECK(a1, list);

  // use iterator to insert
  itr = list.insert(itr, 2, &a[2]);
  REQUIRE_EQ(2, **itr);
  itr = list.insert(list.end(), 1, &a[1]);
  REQUIRE_EQ(1, **itr);
  int a2[] = {2, 9, 5, 4, 3, 1};
  LIST_CHECK(a2, list);

  // 2 has been already added.
  REQUIRE(list.end() == list.insert(list.end(), 2, &a[2]));
}

void IndexedListTest::testIterator()
{
  int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  IndexedList<int, int*> list;
  IndexedList<int, int*>::iterator itr;
  IndexedList<int, int*>::const_iterator citr;
  for (auto& i : a) {
    REQUIRE(list.push_back(i, &i));
  }
  REQUIRE(list.begin() == list.begin());
  itr = list.begin();
  citr = list.begin();
  // operator*()
  REQUIRE_EQ(&a[0], *itr);
  REQUIRE_EQ(&a[0], *citr);
  // operator==(iterator, iterator)
  REQUIRE(itr == list.begin());
  REQUIRE(!(itr == list.end()));
  REQUIRE(citr == list.begin());
  REQUIRE(!(citr == list.end()));
  // operator++()
  ++itr;
  ++citr;
  // operator!=(iterator, iterator)
  REQUIRE(itr != list.begin());
  REQUIRE(!(itr != itr));
  REQUIRE(citr != list.begin());
  REQUIRE(!(citr != citr));
  // operator+(difference_type)
  REQUIRE(itr == list.begin() + 1);
  REQUIRE(citr == list.begin() + 1);
  // operator-(difference_type)
  REQUIRE(itr - 1 == list.begin());
  REQUIRE(citr - 1 == list.begin());
  // operator++(int)
  IndexedList<int, int*>::iterator itr2 = itr++;
  IndexedList<int, int*>::const_iterator citr2 = citr++;
  REQUIRE(itr2 + 1 == itr);
  REQUIRE(citr2 + 1 == citr);
  // operator+(difference_type, iterator)
  REQUIRE(-1 + itr == itr2);
  REQUIRE(-1 + citr == citr2);
  // operator<(iterator, iterator)
  REQUIRE(list.begin() < itr);
  REQUIRE(!(itr < list.begin()));
  REQUIRE(list.begin() < citr);
  REQUIRE(!(citr < list.begin()));
  // operator>(iterator, iterator)
  REQUIRE(itr > list.begin());
  REQUIRE(!(list.begin() > itr));
  REQUIRE(citr > list.begin());
  REQUIRE(!(list.begin() > citr));
  // operator<=(iterator, iterator)
  REQUIRE(itr <= itr);
  REQUIRE(list.begin() <= itr);
  REQUIRE(!(itr <= list.begin()));
  REQUIRE(citr <= citr);
  REQUIRE(list.begin() <= citr);
  REQUIRE(!(citr <= list.begin()));
  // operator>=(iterator, iterator)
  REQUIRE(itr >= itr);
  REQUIRE(itr >= list.begin());
  REQUIRE(!(list.begin() >= itr));
  REQUIRE(citr >= citr);
  REQUIRE(citr >= list.begin());
  REQUIRE(!(list.begin() >= citr));
  // operator-(iterator, iterator)
  REQUIRE(2 == itr - list.begin());
  REQUIRE(-2 == list.begin() - itr);
  REQUIRE(2 == citr - list.begin());
  REQUIRE(-2 == list.begin() - citr);
  // operator+=(difference_type)
  itr = list.begin();
  itr += 2;
  REQUIRE(itr == list.begin() + 2);
  citr = list.begin();
  citr += 2;
  REQUIRE(citr == list.begin() + 2);
  // operator-=(difference_type)
  itr -= 2;
  REQUIRE(itr == list.begin());
  citr -= 2;
  REQUIRE(citr == list.begin());
  // operator[](size_type)
  itr = list.begin();
  itr += 3;
  REQUIRE_EQ(*(itr[1]), a[4]);
  citr = list.begin();
  citr += 3;
  REQUIRE_EQ(*(citr[1]), a[4]);
}

namespace {
struct RemoveOdd {
  bool operator()(int* p) const { return *p % 2 == 1; }
};
} // namespace
void IndexedListTest::testRemoveIf()
{
  int a[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  IndexedList<int, int*> list;
  for (auto& i : a) {
    REQUIRE(list.push_back(i, &i));
  }
  list.remove_if(RemoveOdd());
  REQUIRE_EQ((size_t)5, list.size());
  for (int i = 0; i < 5; ++i) {
    REQUIRE_EQ(i * 2, *list[i]);
  }
}

} // namespace aria2
