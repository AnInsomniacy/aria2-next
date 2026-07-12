/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 The aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#include "Log.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "BufferedFile.h"
#include "DlAbortEx.h"
#include "Exception.h"
#include "File.h"
#include "util.h"

namespace aria2 {
namespace logging {

Settings::Settings()
    : maxFileSize(10 * 1024 * 1024),
      maxFiles(4),
      fileLevel(spdlog::level::trace),
      consoleLevel(spdlog::level::info),
      consoleOutput(true),
      colorOutput(true),
      consoleToStderr(false)
{
}

namespace {

std::mutex configMutex;
Settings currentSettings;
std::shared_ptr<spdlog::logger> currentLogger;

class BoundedFormatter final : public spdlog::formatter {
public:
  BoundedFormatter(std::unique_ptr<spdlog::formatter> formatter,
                   size_t maxSize)
      : formatter_(std::move(formatter)), maxSize_(maxSize)
  {
  }

  void format(const spdlog::details::log_msg& message,
              spdlog::memory_buf_t& destination) override
  {
    spdlog::memory_buf_t formatted;
    formatter_->format(message, formatted);
    if (formatted.size() <= maxSize_) {
      destination.append(formatted.data(), formatted.data() + formatted.size());
      return;
    }

    static const char marker[] = "\n[log record truncated]\n";
    const size_t markerSize = sizeof(marker) - 1;
    if (maxSize_ <= markerSize) {
      destination.append(formatted.data(), formatted.data() + maxSize_);
      return;
    }
    destination.append(formatted.data(),
                       formatted.data() + maxSize_ - markerSize);
    destination.append(marker, marker + markerSize);
  }

  std::unique_ptr<spdlog::formatter> clone() const override
  {
    return std::unique_ptr<spdlog::formatter>(
        new BoundedFormatter(formatter_->clone(), maxSize_));
  }

private:
  std::unique_ptr<spdlog::formatter> formatter_;
  size_t maxSize_;
};

spdlog::filename_t toSpdlogFilename(const std::string& path)
{
#ifdef _WIN32
  return utf8ToWChar(path);
#else
  return path;
#endif
}

std::string fromSpdlogFilename(const spdlog::filename_t& path)
{
#ifdef _WIN32
  return wCharToUtf8(path);
#else
  return path;
#endif
}

bool removeFile(const std::string& path)
{
  File file(path);
  return !file.exists() || file.remove();
}

bool truncateFile(const std::string& path)
{
  BufferedFile file(path.c_str(), BufferedFile::WRITE);
  return file && file.close() != EOF;
}

std::string nativeHistoryPath(const std::string& path, size_t index)
{
  return fromSpdlogFilename(
      spdlog::sinks::rotating_file_sink_mt::calc_filename(
          toSpdlogFilename(path), index));
}

void reconcileLogFiles(const Settings& settings)
{
  for (size_t i = 1; i <= MAX_FILES; ++i) {
    const auto native = nativeHistoryPath(settings.file, i);
    const auto legacy = settings.file + "." + std::to_string(i);
    if (legacy != native && !removeFile(legacy)) {
      throw DL_ABORT_EX("Failed to remove legacy log file " + legacy);
    }

    File history(native);
    if (history.exists() &&
        (i >= settings.maxFiles ||
         history.size() > static_cast<int64_t>(settings.maxFileSize)) &&
        !history.remove()) {
      throw DL_ABORT_EX("Failed to remove log history " + native);
    }
  }

  File active(settings.file);
  if (active.exists() &&
      active.size() > static_cast<int64_t>(settings.maxFileSize) &&
      !truncateFile(settings.file)) {
    throw DL_ABORT_EX("Failed to truncate oversized log file " +
                      settings.file);
  }
}

std::unique_ptr<spdlog::formatter> fileFormatter(size_t maxSize)
{
  std::unique_ptr<spdlog::formatter> pattern(new spdlog::pattern_formatter(
      "%Y-%m-%d %H:%M:%S.%f [%l] [%g:%#] %v"));
  return std::unique_ptr<spdlog::formatter>(
      new BoundedFormatter(std::move(pattern), maxSize));
}

std::shared_ptr<spdlog::logger> makeLogger(const Settings& settings)
{
  if (settings.maxFileSize == 0 || settings.maxFiles == 0 ||
      settings.maxFiles > MAX_FILES) {
    throw DL_ABORT_EX("Invalid log limits");
  }

  std::vector<spdlog::sink_ptr> sinks;
  if (settings.file == "-") {
    spdlog::sink_ptr sink;
    if (settings.consoleToStderr) {
      sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
    }
    else {
      sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    }
    sink->set_level(settings.fileLevel);
    sink->set_pattern("%Y-%m-%d %H:%M:%S.%f [%l] [%g:%#] %v");
    sinks.push_back(std::move(sink));
  }
  else {
    if (!settings.file.empty()) {
      reconcileLogFiles(settings);
      auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          toSpdlogFilename(settings.file), settings.maxFileSize,
          settings.maxFiles - 1, false);
      sink->set_level(settings.fileLevel);
      sink->set_formatter(fileFormatter(settings.maxFileSize));
      sinks.push_back(std::move(sink));
    }

    if (settings.consoleOutput) {
      const auto colorMode = settings.colorOutput
                                 ? spdlog::color_mode::automatic
                                 : spdlog::color_mode::never;
      spdlog::sink_ptr sink;
      if (settings.consoleToStderr) {
        sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>(colorMode);
      }
      else {
        sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>(colorMode);
      }
      sink->set_level(settings.consoleLevel);
      sink->set_pattern("\n%m/%d %H:%M:%S [%^%l%$] %v");
      sinks.push_back(std::move(sink));
    }
  }

  auto logger = std::make_shared<spdlog::logger>("aria2-next", sinks.begin(),
                                                 sinks.end());
  auto level = spdlog::level::off;
  for (const auto& sink : sinks) {
    level = std::min(level, sink->level());
  }
  logger->set_level(level);
  logger->flush_on(spdlog::level::info);
  return logger;
}

std::shared_ptr<spdlog::logger> logger()
{
  auto value = std::atomic_load(&currentLogger);
  if (value) {
    return value;
  }

  std::lock_guard<std::mutex> lock(configMutex);
  value = std::atomic_load(&currentLogger);
  if (!value) {
    value = makeLogger(currentSettings);
    std::atomic_store(&currentLogger, value);
  }
  return value;
}

} // namespace

Settings getSettings()
{
  std::lock_guard<std::mutex> lock(configMutex);
  return currentSettings;
}

spdlog::level::level_enum parseLevel(const std::string& level)
{
  return spdlog::level::from_str(level);
}

void configure(const Settings& settings)
{
  std::lock_guard<std::mutex> lock(configMutex);
  std::shared_ptr<spdlog::logger> replacement;
  try {
    replacement = makeLogger(settings);
  }
  catch (const spdlog::spdlog_ex& error) {
    throw DL_ABORT_EX(std::string("Failed to configure logging: ") +
                      error.what());
  }

  auto previous = std::atomic_exchange(&currentLogger, replacement);
  currentSettings = settings;
  if (previous) {
    previous->flush();
  }
}

void flush()
{
  auto value = std::atomic_load(&currentLogger);
  if (value) {
    value->flush();
  }
}

void shutdown()
{
  std::lock_guard<std::mutex> lock(configMutex);
  auto previous =
      std::atomic_exchange(&currentLogger, std::shared_ptr<spdlog::logger>());
  currentSettings = Settings();
  if (previous) {
    previous->flush();
  }
}

bool enabled(spdlog::level::level_enum level)
{
  return logger()->should_log(level);
}

void write(spdlog::level::level_enum level, const char* sourceFile,
           int lineNum, const char* message)
{
  logger()->log(spdlog::source_loc(sourceFile, lineNum, ""), level,
                spdlog::string_view_t(message));
}

void write(spdlog::level::level_enum level, const char* sourceFile,
           int lineNum, const std::string& message)
{
  write(level, sourceFile, lineNum, message.c_str());
}

void write(spdlog::level::level_enum level, const char* sourceFile,
           int lineNum, const char* message, const Exception& exception)
{
  write(level, sourceFile, lineNum,
        std::string(message) + "\n" + exception.stackTrace());
}

void write(spdlog::level::level_enum level, const char* sourceFile,
           int lineNum, const std::string& message,
           const Exception& exception)
{
  write(level, sourceFile, lineNum, message.c_str(), exception);
}

} // namespace logging
} // namespace aria2
