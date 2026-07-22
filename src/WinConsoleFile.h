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

#ifndef D_WIN_CONSOLE_FILE_H
#define D_WIN_CONSOLE_FILE_H

#include "OutputFile.h"

#include <windows.h>

namespace aria2 {

class WinConsoleFile final : public OutputFile {
public:
  explicit WinConsoleFile(DWORD stdHandle);

  size_t write(const char* str) override;
  int vprintf(const char* format, va_list va) override;
  bool supportsColor() override;
  int flush() override;

private:
  HANDLE handle_;
  bool console_;

  WinConsoleFile(const WinConsoleFile&) = delete;
  WinConsoleFile& operator=(const WinConsoleFile&) = delete;
};

} // namespace aria2

#endif // D_WIN_CONSOLE_FILE_H
