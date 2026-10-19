/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "base/Path.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace aria2::media {

// Native separators, so the long-path prefix of nativePath() survives a
// round trip through a string.
inline std::string utf8(const std::filesystem::path& path)
{
  const auto text = path.u8string();
  return std::string(text.begin(), text.end());
}

inline std::filesystem::path nativePath(const std::string& value)
{
  auto path = toPath(value);
#ifdef _WIN32
  auto wide = path.native();
  if (wide.rfind(L"\\\\?\\", 0) == 0)
    return path;
  path = std::filesystem::absolute(path).lexically_normal().make_preferred();
  wide = path.native();
  return std::filesystem::path(wide.rfind(L"\\\\", 0) == 0
                                   ? L"\\\\?\\UNC\\" + wide.substr(2)
                                   : L"\\\\?\\" + wide);
#else
  return path;
#endif
}

// Creates a directory and its parents. A missing drive or share is named:
// with the long-path prefix, the system's error would name only "\\?\".
inline void createDirectories(const std::string& value)
{
  std::error_code error;
  std::filesystem::create_directories(nativePath(value), error);
  if (!error)
    return;
  if (auto reason = missingVolumeMessage(value); !reason.empty())
    throw std::runtime_error("Cannot create " + value + ": " + reason);
  throw std::filesystem::filesystem_error("create_directories", toPath(value),
                                          error);
}

inline std::string failureMessage(const std::exception& error)
{
  // Native system messages can use the Windows ANSI code page. RPC strings
  // must remain UTF-8 regardless of the machine's display language.
  if (auto file =
          dynamic_cast<const std::filesystem::filesystem_error*>(&error))
    return "Media file operation failed: code=" +
           std::to_string(file->code().value()) +
           " path=" + utf8(file->path1()) + " target=" + utf8(file->path2());
  if (auto system = dynamic_cast<const std::system_error*>(&error))
    return "Media system operation failed: code=" +
           std::to_string(system->code().value());
  return error.what();
}

inline void syncFile(const std::string& value)
{
  const auto path = nativePath(value);
#ifdef _WIN32
  auto handle =
      CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    throw std::system_error(GetLastError(), std::system_category(),
                            "Cannot open media file for synchronization");
  const auto error = FlushFileBuffers(handle) ? ERROR_SUCCESS : GetLastError();
  CloseHandle(handle);
  if (error != ERROR_SUCCESS)
    throw std::system_error(error, std::system_category(),
                            "Cannot synchronize media file");
#else
  const int file = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (file < 0)
    throw std::system_error(errno, std::generic_category(),
                            "Cannot open media file for synchronization");
  const int error = fsync(file) == 0 ? 0 : errno;
  close(file);
  if (error)
    throw std::system_error(error, std::generic_category(),
                            "Cannot synchronize media file");
#endif
}

inline void syncParent(const std::string& value)
{
#ifndef _WIN32
  syncFile(utf8(nativePath(value).parent_path()));
#else
  (void)value; // Publication uses MoveFileExW with MOVEFILE_WRITE_THROUGH.
#endif
}

} // namespace aria2::media
