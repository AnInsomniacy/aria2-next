/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "app/Console.h"

#include "base/Text.h"
#include "core/Engine.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <string_view>

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

void Console::start()
{
  engine_.listen([this](Event event, const Task& task) {
    if (event == Event::Start && running_) {
      started(task);
    }
  });
  running_ = true;
  tick();
}

void Console::stop()
{
  running_ = false;
  timer_.cancel();
  if (lastWidth_ > 0) {
    std::fputs("\n", stdout);
    std::fflush(stdout);
  }
  if (!engine_.global().flag("quiet") &&
      engine_.global().get("download-result") != "hide") {
    results();
  }
}

void Console::tick()
{
  observe();
  if (readout_) {
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

void Console::started(const Task& task)
{
  seen_.try_emplace(task.gid(), Seen{Clock::now(), task.completedLength()});
  if (readout_) {
    print("[" + describe(task) + "]");
  }
}

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
  print(std::move(line));
}

void Console::print(std::string line)
{
  if (!tty_) {
    // Piped output gets whole lines: readers split on newlines.
    std::fprintf(stdout, "%s\n", line.c_str());
    std::fflush(stdout);
    return;
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

void Console::observe()
{
  const auto now = Clock::now();
  for (const auto& task : engine_.active()) {
    seen_.try_emplace(task->gid(), Seen{now, task->completedLength()});
  }
}

void Console::results()
{
  const auto now = Clock::now();
  std::vector<ResultRow> rows;
  const auto add = [&](const Task& task) {
    ResultRow row{task.gid(), task.state(), -1, task.files()};
    if (const auto seen = seen_.find(task.gid()); seen != seen_.end()) {
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              now - seen->second.since)
              .count();
      if (elapsed > 0) {
        row.averageSpeed =
            std::max<int64_t>(task.completedLength() - seen->second.completed,
                              0) *
            1000 / elapsed;
      }
    }
    rows.push_back(std::move(row));
  };
  for (const auto& task : engine_.stopped()) {
    add(*task);
  }
  for (const auto& task : engine_.active()) {
    add(*task);
  }
  for (const auto& task : engine_.waiting()) {
    add(*task);
  }
  if (rows.empty()) {
    return;
  }
  const auto out =
      formatResults(rows, engine_.global().get("download-result") == "full");
  std::fputs(out.c_str(), stdout);
  std::fflush(stdout);
}

namespace {
std::string_view resultStatus(TaskState state)
{
  switch (state) {
  case TaskState::Complete:
    return "OK";
  case TaskState::Error:
    return "ERR";
  case TaskState::Removed:
    return "RM";
  default:
    return "INPR";
  }
}

// A file is shown by its path, or by its URI before it has one.
std::string filePath(const TaskFile& file)
{
  if (!file.path.empty()) {
    return file.path;
  }
  return file.uris.empty() ? "n/a" : file.uris.front().uri;
}
} // namespace

std::string formatResults(const std::vector<ResultRow>& rows, bool full)
{
  std::string out = "\nDownload Results:\ngid   |stat|avg speed  |";
  out +=
      full ? "  %|path/URI\n======+====+===========+===+" + std::string(51, '=')
           : "path/URI\n======+====+===========+" + std::string(55, '=');
  out += '\n';
  std::set<std::string_view> legend;
  for (const auto& row : rows) {
    const auto status = resultStatus(row.state);
    legend.insert(status);
    const auto speed = row.averageSpeed < 0
                           ? std::string("n/a")
                           : text::formatSize(row.averageSpeed) + "/s";
    const auto head = fmt::format("{}|{:<4}|{:>11}|",
                                  gidText(row.gid).substr(0, 6), status, speed);
    std::vector<const TaskFile*> files;
    for (const auto& file : row.files) {
      if (file.selected) {
        files.push_back(&file);
      }
    }
    if (!full) {
      out += head;
      if (files.empty()) {
        out += "n/a";
      }
      else {
        out += filePath(*files.front());
        if (files.size() > 1) {
          out += fmt::format(" ({}more)", files.size() - 1);
        }
      }
      out += '\n';
      continue;
    }
    if (files.empty()) {
      out += head + "  -|n/a\n";
    }
    for (size_t i = 0; i < files.size(); ++i) {
      const auto& file = *files[i];
      out += i == 0 ? head : std::string("      |    |           |");
      out += file.length > 0
                 ? fmt::format("{:>3}|", file.completed * 100 / file.length)
                 : std::string("  -|");
      out += filePath(file) + '\n';
    }
  }
  // The legend, in aria2's order and wording.
  constexpr std::array<std::pair<std::string_view, std::string_view>, 4>
      meanings{{
          {"OK", "(OK):download completed."},
          {"ERR", "(ERR):error occurred."},
          {"INPR", "(INPR):download in-progress."},
          {"RM", "(RM):download removed."},
      }};
  out += "\nStatus Legend:\n";
  for (const auto& [status, meaning] : meanings) {
    if (legend.contains(status)) {
      out += meaning;
      out += '\n';
    }
  }
  return out;
}

} // namespace aria2
