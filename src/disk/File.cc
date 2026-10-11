/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "disk/File.h"

#include "base/Path.h"

#include <chrono>

#ifdef _WIN32
#  include <windows.h>
#  include <winioctl.h>
#else
#  include <cerrno>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace aria2 {

Allocation parseAllocation(std::string_view option)
{
  if (option == "prealloc" || option == "falloc") {
    return Allocation::Preallocate;
  }
  return option == "trunc" ? Allocation::Truncate : Allocation::Sparse;
}

File::File(File&& other) noexcept { *this = std::move(other); }

namespace {
std::error_code lastError()
{
#ifdef _WIN32
  return {static_cast<int>(GetLastError()), std::system_category()};
#else
  return {errno, std::system_category()};
#endif
}

std::error_code makeParent(const std::string& path)
{
  std::error_code error;
  const auto parent = toPath(path).parent_path();
  if (!parent.empty()) {
    fs::create_directories(parent, error);
  }
  return error;
}
} // namespace

#ifdef _WIN32

File& File::operator=(File&& other) noexcept
{
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, nullptr);
  }
  return *this;
}

std::error_code File::open(const std::string& path, bool truncate)
{
  close();
  if (auto error = makeParent(path)) {
    return error;
  }
  const auto handle = CreateFileW(
      toPath(path).c_str(), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      truncate ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return lastError();
  }
  handle_ = handle;
  return {};
}

std::error_code File::openReadOnly(const std::string& path)
{
  close();
  const auto handle =
      CreateFileW(toPath(path).c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return lastError();
  }
  handle_ = handle;
  return {};
}

bool File::isOpen() const { return handle_ != nullptr; }

void File::close()
{
  if (handle_) {
    CloseHandle(handle_);
    handle_ = nullptr;
  }
}

std::error_code File::write(int64_t offset, const char* data, size_t size)
{
  while (size > 0) {
    OVERLAPPED position{};
    position.Offset = static_cast<DWORD>(offset);
    position.OffsetHigh = static_cast<DWORD>(offset >> 32);
    const auto chunk = static_cast<DWORD>(std::min<size_t>(size, 1u << 30));
    DWORD written = 0;
    if (!WriteFile(handle_, data, chunk, &written, &position)) {
      return lastError();
    }
    data += written;
    size -= written;
    offset += written;
  }
  return {};
}

std::error_code File::read(int64_t offset, char* data, size_t size,
                           size_t& count)
{
  OVERLAPPED position{};
  position.Offset = static_cast<DWORD>(offset);
  position.OffsetHigh = static_cast<DWORD>(offset >> 32);
  DWORD read = 0;
  if (!ReadFile(handle_, data,
                static_cast<DWORD>(std::min<size_t>(size, 1u << 30)), &read,
                &position) &&
      GetLastError() != ERROR_HANDLE_EOF) {
    return lastError();
  }
  count = read;
  return {};
}

std::error_code File::resize(int64_t length)
{
  FILE_END_OF_FILE_INFO end{};
  end.EndOfFile.QuadPart = length;
  if (!SetFileInformationByHandle(handle_, FileEndOfFileInfo, &end,
                                  sizeof(end))) {
    return lastError();
  }
  return {};
}

std::error_code File::allocate(int64_t length)
{
  FILE_ALLOCATION_INFO allocation{};
  allocation.AllocationSize.QuadPart = length;
  if (!SetFileInformationByHandle(handle_, FileAllocationInfo, &allocation,
                                  sizeof(allocation))) {
    return lastError();
  }
  return resize(length);
}

void File::makeSparse()
{
  DWORD returned = 0;
  // Without this NTFS zero-fills everything before a far offset on write.
  DeviceIoControl(handle_, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned,
                  nullptr);
}

std::error_code File::sync()
{
  return FlushFileBuffers(handle_) ? std::error_code() : lastError();
}

int64_t File::size() const
{
  LARGE_INTEGER value{};
  return GetFileSizeEx(handle_, &value) ? value.QuadPart : -1;
}

std::error_code removeFile(const std::string& path)
{
  std::error_code error;
  fs::remove(toPath(path), error);
  return error;
}

void setModificationTime(const std::string& path, int64_t unixSeconds)
{
  const auto handle =
      CreateFileW(toPath(path).c_str(), FILE_WRITE_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }
  const auto ticks = (unixSeconds + 11644473600LL) * 10000000LL;
  FILETIME time{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
  SetFileTime(handle, nullptr, &time, &time);
  CloseHandle(handle);
}

#else

File& File::operator=(File&& other) noexcept
{
  if (this != &other) {
    close();
    fd_ = std::exchange(other.fd_, -1);
  }
  return *this;
}

std::error_code File::open(const std::string& path, bool truncate)
{
  close();
  if (auto error = makeParent(path)) {
    return error;
  }
  fd_ = ::open(path.c_str(),
               O_RDWR | O_CREAT | O_CLOEXEC | (truncate ? O_TRUNC : 0), 0666);
  return fd_ < 0 ? lastError() : std::error_code();
}

std::error_code File::openReadOnly(const std::string& path)
{
  close();
  fd_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  return fd_ < 0 ? lastError() : std::error_code();
}

bool File::isOpen() const { return fd_ >= 0; }

void File::close()
{
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

std::error_code File::write(int64_t offset, const char* data, size_t size)
{
  while (size > 0) {
    const auto written = ::pwrite(fd_, data, size, static_cast<off_t>(offset));
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return lastError();
    }
    data += written;
    size -= static_cast<size_t>(written);
    offset += written;
  }
  return {};
}

std::error_code File::read(int64_t offset, char* data, size_t size,
                           size_t& count)
{
  ssize_t result;
  do {
    result = ::pread(fd_, data, size, static_cast<off_t>(offset));
  } while (result < 0 && errno == EINTR);
  if (result < 0) {
    return lastError();
  }
  count = static_cast<size_t>(result);
  return {};
}

std::error_code File::resize(int64_t length)
{
  return ::ftruncate(fd_, static_cast<off_t>(length)) == 0 ? std::error_code()
                                                           : lastError();
}

std::error_code File::allocate(int64_t length)
{
#  if defined(__APPLE__)
  fstore_t store{F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, static_cast<off_t>(length),
                 0};
  if (fcntl(fd_, F_PREALLOCATE, &store) == -1) {
    store.fst_flags = F_ALLOCATEALL;
    fcntl(fd_, F_PREALLOCATE, &store);
  }
  return resize(length);
#  elif defined(__linux__) || defined(__ANDROID__)
  const auto result = ::posix_fallocate(fd_, 0, static_cast<off_t>(length));
  return result == 0 ? std::error_code()
                     : std::error_code(result, std::system_category());
#  else
  return resize(length);
#  endif
}

void File::makeSparse() {}

std::error_code File::sync()
{
  return ::fsync(fd_) == 0 ? std::error_code() : lastError();
}

int64_t File::size() const
{
  struct stat info{};
  return ::fstat(fd_, &info) == 0 ? static_cast<int64_t>(info.st_size) : -1;
}

std::error_code removeFile(const std::string& path)
{
  std::error_code error;
  fs::remove(toPath(path), error);
  return error;
}

void setModificationTime(const std::string& path, int64_t unixSeconds)
{
  const timespec times[2] = {{unixSeconds, 0}, {unixSeconds, 0}};
  ::utimensat(AT_FDCWD, path.c_str(), times, 0);
}

#endif

std::error_code renameFile(const std::string& from, const std::string& to)
{
  std::error_code error;
  fs::rename(toPath(from), toPath(to), error);
  return error;
}

bool fileExists(const std::string& path)
{
  std::error_code error;
  return fs::exists(toPath(path), error);
}

std::optional<std::string> numberedPath(const std::string& path)
{
  const auto full = toPath(path);
  const auto stem = toUtf8(full.parent_path() / full.stem());
  const auto extension = toUtf8(full.extension());
  for (int i = 1; i < 10000; ++i) {
    auto candidate = stem + '.' + std::to_string(i) + extension;
    if (!fileExists(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

int64_t fileSize(const std::string& path)
{
  std::error_code error;
  const auto size = fs::file_size(toPath(path), error);
  return error ? -1 : static_cast<int64_t>(size);
}

} // namespace aria2
