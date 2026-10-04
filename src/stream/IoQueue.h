/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef ARIA2_STREAM_IO_QUEUE_H
#define ARIA2_STREAM_IO_QUEUE_H
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <future>
#include <memory>
#include <utility>

namespace aria2::stream {
// Native file operations remain ordered per output, independent of RPC and
// curl. Only the engine thread reads the queue; workers own the captured I/O
// resources.
class IoQueue {
  static boost::asio::thread_pool& pool()
  {
    static boost::asio::thread_pool value(2);
    return value;
  }
  struct Operation {
    std::future<void> done;
    int64_t begin;
    size_t bytes;
  };
  boost::asio::strand<boost::asio::thread_pool::executor_type> executor_{
      pool().get_executor()};
  std::deque<Operation> pending_;
  size_t bytes_ = 0;

public:
  static constexpr size_t capacity = 8 * 1024 * 1024;
  template <class F> void submit(F work, int64_t begin = 0, size_t bytes = 0)
  {
    auto task = std::make_shared<std::packaged_task<void()>>(std::move(work));
    pending_.push_back({task->get_future(), begin, bytes});
    bytes_ += bytes;
    boost::asio::post(executor_, [task] { (*task)(); });
  }
  template <class F> void poll(F commit)
  {
    while (!pending_.empty() &&
           pending_.front().done.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready) {
      auto op = std::move(pending_.front());
      pending_.pop_front();
      bytes_ -= op.bytes;
      op.done.get();
      if (op.bytes)
        commit(op.begin, op.begin + static_cast<int64_t>(op.bytes));
    }
  }
  bool busy() const { return !pending_.empty(); }
  size_t bytes() const { return bytes_; }
};
} // namespace aria2::stream
#endif
