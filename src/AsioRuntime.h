/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 AnInsomniacy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_ASIO_RUNTIME_H
#define D_ASIO_RUNTIME_H

#include "common.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <functional>

namespace aria2 {

class AsioRuntime {
public:
  using Task = std::function<void()>;

  AsioRuntime();
  ~AsioRuntime();

  boost::asio::io_context& ioContext();

  void post(Task task);

  void runReady();

  void wake();

  void cancel();

  void scheduleWake(std::chrono::milliseconds delay);

  bool wakeRequested() const { return wakeRequested_; }

  bool consumeWakeRequest();

  bool cancelled() const { return cancelled_; }

private:
  boost::asio::io_context ioContext_;
  boost::asio::steady_timer wakeTimer_;
  bool wakeRequested_;
  bool cancelled_;
};

} // namespace aria2

#endif // D_ASIO_RUNTIME_H
