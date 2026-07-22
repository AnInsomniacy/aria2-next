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

#include "WinConsoleFile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "util.h"

namespace aria2 {

WinConsoleFile::WinConsoleFile(DWORD stdHandle)
    : handle_(GetStdHandle(stdHandle)), console_(false)
{
  DWORD mode;
  console_ = handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE &&
             GetConsoleMode(handle_, &mode) != 0;
}

size_t WinConsoleFile::write(const char* str)
{
  const auto length = strlen(str);
  if (length == 0 || handle_ == nullptr || handle_ == INVALID_HANDLE_VALUE) {
    return 0;
  }

  DWORD written = 0;
  if (!console_) {
    const auto count = static_cast<DWORD>(
        std::min(length, static_cast<size_t>(std::numeric_limits<DWORD>::max())));
    return WriteFile(handle_, str, count, &written, nullptr) ? written : 0;
  }

  const auto wide = utf8ToWChar(str);
  size_t offset = 0;
  while (offset < wide.size()) {
    const auto remaining = wide.size() - offset;
    const auto count = static_cast<DWORD>(std::min(
        remaining,
        static_cast<size_t>(std::numeric_limits<DWORD>::max())));
    written = 0;
    if (!WriteConsoleW(handle_, wide.data() + offset, count, &written,
                       nullptr) ||
        written == 0) {
      return 0;
    }
    offset += written;
  }
  return length;
}

int WinConsoleFile::vprintf(const char* format, va_list va)
{
  va_list measure;
  va_copy(measure, va);
  const int length = vsnprintf(nullptr, 0, format, measure);
  va_end(measure);
  if (length <= 0) {
    return length;
  }

  std::vector<char> message(static_cast<size_t>(length) + 1);
  va_list render;
  va_copy(render, va);
  const int rendered =
      vsnprintf(message.data(), message.size(), format, render);
  va_end(render);
  if (rendered < 0) {
    return rendered;
  }

  return write(message.data()) == static_cast<size_t>(rendered) ? rendered : -1;
}

bool WinConsoleFile::supportsColor() { return false; }

int WinConsoleFile::flush() { return 0; }

} // namespace aria2
