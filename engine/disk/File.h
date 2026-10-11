/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>
#include <system_error>

namespace aria2 {

enum class Allocation { Sparse, Preallocate, Truncate };
Allocation parseAllocation(std::string_view option); // file-allocation values

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
  std::error_code resize(int64_t length);
  // Reserves space without writing zeros where the platform allows it.
  std::error_code allocate(int64_t length);
  // Marks the file sparse so unwritten regions cost no space (Windows only;
  // other supported file systems are sparse by default).
  void makeSparse();
  std::error_code sync();
  int64_t size() const;

private:
#ifdef _WIN32
  void* handle_ = nullptr;
#else
  int fd_ = -1;
#endif
};

std::error_code removeFile(const std::string& path);
std::error_code renameFile(const std::string& from, const std::string& to);
bool fileExists(const std::string& path);
int64_t fileSize(const std::string& path); // -1 when missing
void setModificationTime(const std::string& path, int64_t unixSeconds);

} // namespace aria2
