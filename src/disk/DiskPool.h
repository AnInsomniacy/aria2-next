/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <functional>
#include <utility>

namespace aria2 {

// Threads for blocking file work. Each file serializes its operations through
// its own strand; different files proceed in parallel. Completions run on the
// control thread.
class DiskPool {
public:
  using Strand = boost::asio::strand<boost::asio::thread_pool::executor_type>;

  explicit DiskPool(boost::asio::io_context& control, unsigned threads = 4)
      : control_(control), pool_(threads)
  {
  }
  ~DiskPool() { drain(); }

  Strand strand() { return boost::asio::make_strand(pool_); }

  // Runs `work` on `strand`, then `done` with its result on the control thread.
  template <typename Work, typename Done>
  void run(Strand& strand, Work work, Done done)
  {
    ++pending_;
    boost::asio::post(strand, [this, work = std::move(work),
                               done = std::move(done)]() mutable {
      auto result = work();
      boost::asio::post(control_, [this, result = std::move(result),
                                   done = std::move(done)]() mutable {
        --pending_;
        done(std::move(result));
      });
    });
  }

  size_t pending() const { return pending_; }
  // Blocks until queued work finished; used at shutdown.
  void drain() { pool_.join(); }

private:
  boost::asio::io_context& control_;
  boost::asio::thread_pool pool_;
  std::atomic<size_t> pending_{0};
};

} // namespace aria2
