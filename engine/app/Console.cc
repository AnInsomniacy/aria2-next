/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "app/Console.h"

#include "base/Text.h"
#include "core/Engine.h"

#include <cstdio>

#ifdef _WIN32
#  include <io.h>
#  define isatty _isatty
#  define fileno _fileno
#else
#  include <unistd.h>
#endif

namespace aria2 {

Console::Console(Engine& engine)
    : engine_(engine),
      timer_(engine.io()),
      tty_(isatty(fileno(stdout)) != 0),
      readout_(engine.global().flag("show-console-readout") &&
               !engine.global().flag("quiet")),
      summaryInterval_(engine.global().integer("summary-interval"))
{
}

void Console::start() { tick(); }

void Console::stop()
{
  timer_.cancel();
  if (lastWidth_ > 0) {
    std::fputs("\n", stdout);
    std::fflush(stdout);
  }
}

void Console::tick()
{
  if (readout_ && tty_) {
    readout();
  }
  if (summaryInterval_ > 0 && ++seconds_ % summaryInterval_ == 0 &&
      !engine_.global().flag("quiet")) {
    summary();
  }
  timer_.expires_after(std::chrono::seconds(1));
  timer_.async_wait([this](const boost::system::error_code& error) {
    if (!error) {
      tick();
    }
  });
}

namespace {
std::string describe(const Task& task)
{
  const auto total = task.totalLength();
  const auto done = task.completedLength();
  const auto speed = task.downloadSpeed();
  std::string line = "#" + gidText(task.gid()).substr(0, 6) + " " +
                     text::formatSize(done) + "/" + text::formatSize(total);
  if (total > 0) {
    line += "(" + std::to_string(done * 100 / total) + "%)";
  }
  line += " CN:" + std::to_string(task.connections());
  line += " DL:" + text::formatSize(speed);
  if (speed > 0 && total > done) {
    line += " ETA:" + text::formatDuration((total - done) / speed);
  }
  return line;
}
} // namespace

void Console::readout()
{
  const auto active = engine_.active();
  if (active.empty()) {
    return;
  }
  std::string line = "[" + describe(*active.front()) + "]";
  if (active.size() > 1) {
    line += "(+" + std::to_string(active.size() - 1) + ")";
  }
  const auto width = line.size();
  if (width < lastWidth_) {
    line.append(lastWidth_ - width, ' ');
  }
  lastWidth_ = width;
  std::fprintf(stdout, "\r%s", line.c_str());
  std::fflush(stdout);
}

void Console::summary()
{
  const auto active = engine_.active();
  if (active.empty()) {
    return;
  }
  std::string out = "\n *** Download Progress Summary ***\n";
  for (const auto& task : active) {
    out += "[" + describe(*task) + "]\n";
    for (const auto& file : task->files()) {
      if (!file.path.empty()) {
        out += "FILE: " + file.path + "\n";
      }
    }
  }
  std::fputs(out.c_str(), stdout);
  std::fflush(stdout);
  lastWidth_ = 0;
}

} // namespace aria2
