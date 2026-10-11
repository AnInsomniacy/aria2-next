/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <spdlog/fmt/fmt.h>

#include <stdexcept>
#include <string>

namespace aria2 {

// aria2 exit status and RPC errorCode values. The numbers are part of the
// public contract.
enum class ErrorCode {
  Finished = 0,
  Unknown = 1,
  TimedOut = 2,
  ResourceNotFound = 3,
  MaxFileNotFound = 4,
  TooSlow = 5,
  Network = 6,
  InProgress = 7,
  CannotResume = 8,
  NotEnoughDiskSpace = 9,
  PieceLengthChanged = 10,
  DuplicateDownload = 11,
  DuplicateInfoHash = 12,
  FileExists = 13,
  RenameFailed = 14,
  FileOpen = 15,
  FileCreate = 16,
  FileIo = 17,
  DirCreate = 18,
  NameResolve = 19,
  MetalinkParse = 20,
  FtpProtocol = 21,
  HttpProtocol = 22,
  TooManyRedirects = 23,
  HttpAuth = 24,
  BencodeParse = 25,
  TorrentParse = 26,
  MagnetParse = 27,
  Option = 28,
  ServiceUnavailable = 29,
  JsonParse = 30,
  Removed = 31,
  Checksum = 32,
};

class Error : public std::runtime_error {
public:
  Error(ErrorCode code, const std::string& message)
      : std::runtime_error(message), code_(code)
  {
  }
  ErrorCode code() const noexcept { return code_; }

private:
  ErrorCode code_;
};

template <typename... Args>
[[noreturn]] void fail(ErrorCode code, fmt::format_string<Args...> format,
                       Args&&... args)
{
  throw Error(code, fmt::format(format, std::forward<Args>(args)...));
}

} // namespace aria2
