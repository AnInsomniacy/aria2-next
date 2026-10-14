/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <system_error>

namespace aria2 {

// How a download's file takes its final size (file-allocation): sparse, so
// unwritten ranges take no space yet, or with all its space reserved first.
enum class Allocation { Sparse, Reserve };
Allocation parseAllocation(std::string_view option);

// A file opened for positional I/O. Methods block; the disk pool calls them.
// Safe for concurrent reads and writes at different offsets.
class File {
public:
  File() = default;
  ~File() { close(); }
  File(File&& other) noexcept;
  File& operator=(File&& other) noexcept;
  File(const File&) = delete;
  File& operator=(const File&) = delete;

  // Opens for reading and writing, creating the file and its directory when
  // missing. `truncate` empties an existing file.
  std::error_code open(const std::string& path, bool truncate);
  std::error_code openReadOnly(const std::string& path);
  bool isOpen() const;
  void close();

  std::error_code write(int64_t offset, const char* data, size_t size);
  // Reads up to `size` bytes; returns the count read or an error.
  std::error_code read(int64_t offset, char* data, size_t size, size_t& read);
  // Grows the file to `length` bytes as `allocation` asks, keeping its
  // content. NTFS makes a write beyond the written part of a file that is
  // not sparse wait while it zero-fills the gap, so on Windows a sparse file
  // is flagged sparse and a reserved one has its zeros written here.
  // stopExtending() ends that early from any thread, at the size filled.
  std::error_code extend(int64_t length, Allocation allocation);
  void stopExtending() { stopExtending_ = true; }
  std::error_code sync();
  int64_t size() const;

private:
  std::error_code setLength(int64_t length);

  std::atomic<bool> stopExtending_ = false;
#ifdef _WIN32
  void* handle_ = nullptr;
#else
  int fd_ = -1;
#endif
};

std::error_code removeFile(const std::string& path);
std::error_code renameFile(const std::string& from, const std::string& to);
bool fileExists(const std::string& path);
// The first free "name.N.ext" beside `path` (auto-file-renaming), if any.
// `taken` rejects names that do not exist yet but are spoken for.
std::optional<std::string>
numberedPath(const std::string& path,
             const std::function<bool(const std::string&)>& taken = {});
int64_t fileSize(const std::string& path); // -1 when missing
void setModificationTime(const std::string& path, int64_t unixSeconds);

} // namespace aria2
