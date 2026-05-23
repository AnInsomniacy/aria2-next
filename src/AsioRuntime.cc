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
#include "AsioRuntime.h"

#include <utility>

namespace aria2 {

AsioRuntime::AsioRuntime()
    : wakeTimer_(ioContext_),
      wakeRequested_(false),
      cancelled_(false)
{
}

AsioRuntime::~AsioRuntime()
{
  cancel();
  runReady();
}

boost::asio::io_context& AsioRuntime::ioContext() { return ioContext_; }

void AsioRuntime::post(Task task)
{
  boost::asio::post(ioContext_.get_executor(), std::move(task));
}

void AsioRuntime::runReady()
{
  ioContext_.restart();
  while (ioContext_.poll_one() > 0) {
  }
}

void AsioRuntime::wake()
{
  wakeRequested_ = true;
  wakeTimer_.cancel();
  post([] {});
}

void AsioRuntime::cancel()
{
  cancelled_ = true;
  wakeTimer_.cancel();
}

void AsioRuntime::scheduleWake(std::chrono::milliseconds delay)
{
  wakeTimer_.expires_after(delay);
  wakeTimer_.async_wait([this](const boost::system::error_code& ec) {
    if (!ec) {
      wakeRequested_ = true;
    }
  });
}

bool AsioRuntime::consumeWakeRequest()
{
  const auto requested = wakeRequested_;
  wakeRequested_ = false;
  return requested;
}

} // namespace aria2
