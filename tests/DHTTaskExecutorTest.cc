#include "DHTTaskExecutor.h"

#include "a2doctest.h"

#include "MockDHTTask.h"
#include "array_fun.h"

namespace aria2 {

class DHTTaskExecutorTest {


public:
  void testUpdate();
};

A2_TEST(DHTTaskExecutorTest, testUpdate)

void DHTTaskExecutorTest::testUpdate()
{
  std::shared_ptr<DHTNode> rn;
  DHTTaskExecutor tex(2);
  std::shared_ptr<MockDHTTask> tasks[] = {
      std::shared_ptr<MockDHTTask>(new MockDHTTask(rn)),
      std::shared_ptr<MockDHTTask>(new MockDHTTask(rn)),
      std::shared_ptr<MockDHTTask>(new MockDHTTask(rn)),
      std::shared_ptr<MockDHTTask>(new MockDHTTask(rn))};
  tasks[1]->finished_ = true;
  for (size_t i = 0; i < arraySize(tasks); ++i) {
    tex.addTask(tasks[i]);
  }
  REQUIRE_EQ((size_t)0, tex.getExecutingTaskSize());
  REQUIRE_EQ((size_t)4, tex.getQueueSize());
  tex.update();
  REQUIRE_EQ((size_t)2, tex.getExecutingTaskSize());
  REQUIRE_EQ((size_t)1, tex.getQueueSize());
  tasks[0]->finished_ = true;
  tasks[2]->finished_ = true;
  tasks[3]->finished_ = true;
  tex.update();
  REQUIRE_EQ((size_t)0, tex.getExecutingTaskSize());
  REQUIRE_EQ((size_t)0, tex.getQueueSize());
}

} // namespace aria2
