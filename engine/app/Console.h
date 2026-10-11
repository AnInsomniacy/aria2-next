/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <boost/asio/steady_timer.hpp>

namespace aria2 {

class Engine;

// The one-line progress readout and periodic summaries of command-line use.
class Console {
public:
  explicit Console(Engine& engine);
  void start();
  void stop();

private:
  void tick();
  void readout();
  void summary();

  Engine& engine_;
  boost::asio::steady_timer timer_;
  bool tty_;
  bool readout_;
  int64_t summaryInterval_;
  int seconds_ = 0;
  size_t lastWidth_ = 0;
};

} // namespace aria2
