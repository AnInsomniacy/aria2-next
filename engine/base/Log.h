/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <string_view>

namespace aria2::log {

// aria2's five levels map onto spdlog's trace..error.
enum class Level { Debug, Info, Notice, Warn, Error };

struct Config {
  std::string file; // empty: no file log; "-": standard output
  Level fileLevel = Level::Debug;
  Level consoleLevel = Level::Notice;
  size_t maxFileSize = 0; // 0: unbounded
  size_t maxFiles = 0;
  bool color = true;
};

// Replaces the process logger. Safe to call again after option changes.
void configure(const Config& config);
bool parseLevel(std::string_view text, Level& level);
std::shared_ptr<spdlog::logger> logger();
bool enabled(Level level);
// Writes buffered records; the engine calls it once a second.
void flush();

template <typename... Args>
void debug(spdlog::format_string_t<Args...> format, Args&&... args)
{
  if (enabled(Level::Debug))
    logger()->trace(format, std::forward<Args>(args)...);
}

template <typename... Args>
void info(spdlog::format_string_t<Args...> format, Args&&... args)
{
  if (enabled(Level::Info))
    logger()->debug(format, std::forward<Args>(args)...);
}

template <typename... Args>
void notice(spdlog::format_string_t<Args...> format, Args&&... args)
{
  if (enabled(Level::Notice))
    logger()->info(format, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(spdlog::format_string_t<Args...> format, Args&&... args)
{
  if (enabled(Level::Warn))
    logger()->warn(format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(spdlog::format_string_t<Args...> format, Args&&... args)
{
  if (enabled(Level::Error))
    logger()->error(format, std::forward<Args>(args)...);
}

} // namespace aria2::log
