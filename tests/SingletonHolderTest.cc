#include "SingletonHolder.h"

#include <iostream>

#include "a2doctest.h"

#include "a2functional.h"

namespace aria2 {

class SingletonHolderTest {


private:
public:
  void setUp() {}

  void testInstance();
};

A2_TEST(SingletonHolderTest, testInstance)

class M {
private:
  std::string greeting_;

public:
  M(const std::string& greeting) : greeting_(greeting) {}

  const std::string& greeting() const { return greeting_; }

  void greeting(const std::string& greeting) { greeting_ = greeting; }
};

void SingletonHolderTest::testInstance()
{
  SingletonHolder<M>::instance(make_unique<M>("Hello world."));
  REQUIRE_EQ(std::string("Hello world."),
                       SingletonHolder<M>::instance()->greeting());

  SingletonHolder<M>::instance()->greeting("Yes, it worked!");
  REQUIRE_EQ(std::string("Yes, it worked!"),
                       SingletonHolder<M>::instance()->greeting());
}

} // namespace aria2
