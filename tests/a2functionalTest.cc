#include "a2functional.h"

#include <string>
#include <numeric>
#include <algorithm>

#include "a2doctest.h"

namespace aria2 {

class a2functionalTest {


public:
  void testStrjoin();
  void testLeastRecentAccess();

  struct LastAccess {
    time_t lastAccess_;
    LastAccess(time_t lastAccess) : lastAccess_(lastAccess) {}

    time_t getLastAccessTime() const { return lastAccess_; }
  };
};

A2_TEST(a2functionalTest, testStrjoin)
A2_TEST(a2functionalTest, testLeastRecentAccess)

void a2functionalTest::testStrjoin()
{
  std::vector<std::string> v;
  REQUIRE_EQ(std::string(""), strjoin(v.begin(), v.end(), " "));

  v.push_back("A");

  REQUIRE_EQ(std::string("A"), strjoin(v.begin(), v.end(), " "));

  v.push_back("hero");
  v.push_back("is");
  v.push_back("lonely");

  REQUIRE_EQ(std::string("A hero is lonely"),
                       strjoin(v.begin(), v.end(), " "));
}

void a2functionalTest::testLeastRecentAccess()
{
  std::vector<LastAccess> v;
  for (int i = 99; i >= 0; --i) {
    v.push_back(LastAccess(i));
  }
  std::sort(v.begin(), v.end(), LeastRecentAccess<LastAccess>());
  for (int i = 0; i < 100; ++i) {
    REQUIRE_EQ((time_t)i, v[i].lastAccess_);
  }
}

} // namespace aria2
