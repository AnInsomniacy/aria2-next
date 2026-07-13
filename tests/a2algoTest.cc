#include "a2algo.h"

#include "a2doctest.h"

#include "array_fun.h"

namespace aria2 {

class a2algoTest {


public:
  void setUp() {}

  void tearDown() {}

  void testSelect();
};

A2_TEST(a2algoTest, testSelect)

void a2algoTest::testSelect()
{
  size_t A[] = {1, 2, 3, 4, 7, 10, 11, 12, 13, 14, 15, 100, 112, 113, 114};

  std::pair<size_t*, size_t> p = max_sequence(std::begin(A), std::end(A));
  REQUIRE_EQ(&A[5], p.first);
  REQUIRE_EQ((size_t)6, p.second);

  p = max_sequence(&A[0], &A[0]);
  REQUIRE_EQ(&A[0], p.first);
  REQUIRE_EQ((size_t)0, p.second);

  p = max_sequence(&A[0], &A[4]);
  REQUIRE_EQ(&A[0], p.first);
  REQUIRE_EQ((size_t)4, p.second);
}

} // namespace aria2
