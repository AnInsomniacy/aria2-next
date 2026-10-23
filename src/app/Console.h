/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Gid.h"
#include "core/Task.h"

#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace aria2 {

class Engine;

// One task in the "Download Results" table printed on exit.
struct ResultRow {
  Gid gid = 0;
  TaskState state = TaskState::Waiting;
  int64_t averageSpeed = -1; // bytes/s over this run; -1: not measured
  std::vector<TaskFile> files;
};

// The table in aria2's layout, which frontends such as Scoop parse: rows of
// gid|stat|avg speed|path and a legend ending in "(OK):download completed.".
// `full` lists every selected file with its progress.
std::string formatResults(const std::vector<ResultRow>& rows, bool full);

// The progress readout, periodic summaries and the results table of
// command-line use. On a terminal the readout redraws one line; piped, it
// prints one line per second. A task is also shown the moment it starts, so a
// download shorter than a second still has a line and an average speed
// (aria2-next#70: Scoop showed nothing for its usual small downloads).
class Console {
public:
  explicit Console(Engine& engine);
  void start();
  void stop();

private:
  using Clock = std::chrono::steady_clock;
  struct Seen {
    Clock::time_point since;
    int64_t completed = 0;
  };

  void tick();
  void started(const Task& task);
  void readout();
  void print(std::string line);
  void summary();
  void results();
  void observe();

  Engine& engine_;
  boost::asio::steady_timer timer_;
  bool tty_;
  bool readout_;
  bool running_ = false; // between start() and stop(); events arrive only then
  int64_t summaryInterval_;
  int seconds_ = 0;
  size_t lastWidth_ = 0;
  // When each task was first seen running, for the average speed.
  std::map<Gid, Seen> seen_;
};

} // namespace aria2
