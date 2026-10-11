/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Log.h"

#include "base/Path.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>

namespace aria2::log {
namespace {

constexpr std::array<std::string_view, 5> names{"debug", "info", "notice",
                                                "warn", "error"};

spdlog::level::level_enum toSpdlog(Level level)
{
  return static_cast<spdlog::level::level_enum>(static_cast<int>(level));
}

// Prints aria2 level names (debug/info/notice/warn/error) for %*.
class LevelFlag final : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg& message, const std::tm&,
              spdlog::memory_buf_t& out) override
  {
    const auto index = std::min<size_t>(message.level, names.size() - 1);
    out.append(names[index].data(), names[index].data() + names[index].size());
  }
  std::unique_ptr<custom_flag_formatter> clone() const override
  {
    return std::make_unique<LevelFlag>();
  }
};

std::unique_ptr<spdlog::formatter> formatter(const char* pattern)
{
  auto result = std::make_unique<spdlog::pattern_formatter>();
  result->add_flag<LevelFlag>('*').set_pattern(pattern);
  return result;
}

std::mutex mutex;
std::shared_ptr<spdlog::logger> current = std::make_shared<spdlog::logger>(
    "aria2", std::make_shared<spdlog::sinks::null_sink_mt>());
std::atomic<int> threshold{static_cast<int>(Level::Error)};

} // namespace

bool parseLevel(std::string_view text, Level& level)
{
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i] == text) {
      level = static_cast<Level>(i);
      return true;
    }
  }
  return false;
}

void configure(const Config& config)
{
  std::vector<spdlog::sink_ptr> sinks;
  constexpr auto filePattern = "%Y-%m-%d %H:%M:%S.%e [%*] %v";
  if (config.file == "-") {
    auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    sink->set_formatter(formatter(filePattern));
    sink->set_level(toSpdlog(config.fileLevel));
    sinks.push_back(std::move(sink));
  }
  else {
    if (!config.file.empty()) {
      auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          toPath(config.file).native(),
          config.maxFileSize ? config.maxFileSize : SIZE_MAX,
          config.maxFiles > 1 ? config.maxFiles - 1 : 0);
      sink->set_formatter(formatter(filePattern));
      sink->set_level(toSpdlog(config.fileLevel));
      sinks.push_back(std::move(sink));
    }
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>(
        config.color ? spdlog::color_mode::automatic
                     : spdlog::color_mode::never);
    console->set_formatter(formatter("%m/%d %H:%M:%S [%^%*%$] %v"));
    console->set_level(toSpdlog(config.consoleLevel));
    sinks.push_back(std::move(console));
  }
  auto next = std::make_shared<spdlog::logger>("aria2", sinks.begin(),
                                               sinks.end());
  auto lowest = std::min(static_cast<int>(config.consoleLevel),
                         config.file.empty()
                             ? static_cast<int>(Level::Error)
                             : static_cast<int>(config.fileLevel));
  next->set_level(static_cast<spdlog::level::level_enum>(lowest));
  next->flush_on(spdlog::level::warn);
  threshold = lowest;
  std::lock_guard lock(mutex);
  current = std::move(next);
}

std::shared_ptr<spdlog::logger> logger()
{
  std::lock_guard lock(mutex);
  return current;
}

bool enabled(Level level) { return static_cast<int>(level) >= threshold; }

void flush() { logger()->flush(); }

} // namespace aria2::log
