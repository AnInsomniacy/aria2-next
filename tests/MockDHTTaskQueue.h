#ifndef D_MOCK_DHT_TASK_QUEUE_H
#define D_MOCK_DHT_TASK_QUEUE_H

#include "DHTTaskQueue.h"

namespace aria2 {

class MockDHTTaskQueue : public DHTTaskQueue {
public:
  std::deque<std::shared_ptr<DHTTask>> periodicTaskQueue1_;

  std::deque<std::shared_ptr<DHTTask>> periodicTaskQueue2_;

  std::deque<std::shared_ptr<DHTTask>> immediateTaskQueue_;

  MockDHTTaskQueue() {}

  virtual ~MockDHTTaskQueue() {}

  virtual void executeTask() override {}

  virtual void
  addPeriodicTask1(const std::shared_ptr<DHTTask>& task) override
  {
    periodicTaskQueue1_.push_back(task);
  }

  virtual void
  addPeriodicTask2(const std::shared_ptr<DHTTask>& task) override
  {
    periodicTaskQueue2_.push_back(task);
  }

  virtual void
  addImmediateTask(const std::shared_ptr<DHTTask>& task) override
  {
    immediateTaskQueue_.push_back(task);
  }
};

} // namespace aria2

#endif // D_MOCK_DHT_TASK_QUEUE_H
