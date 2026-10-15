/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "disk/File.h"

#include "base/Path.h"

#include <spdlog/fmt/fmt.h>

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
  return option == "prealloc" || option == "falloc" ? Allocation::Reserve
                                                    : Allocation::Sparse;
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

std::error_code File::setLength(int64_t length)
{
  FILE_END_OF_FILE_INFO end{};
  end.EndOfFile.QuadPart = length;
  if (!SetFileInformationByHandle(handle_, FileEndOfFileInfo, &end,
                                  sizeof(end))) {
    return lastError();
  }
  return {};
}

std::error_code File::extend(int64_t length, Allocation allocation)
{
  if (allocation == Allocation::Sparse) {
    // FAT and exFAT have no sparse files; they zero-fill as before.
    DWORD returned = 0;
    DeviceIoControl(handle_, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0,
                    &returned, nullptr);
  }
  const auto filled = size();
  if (filled < 0) {
    return lastError();
  }
  if (filled >= length) {
    return {};
  }
  if (allocation == Allocation::Sparse) {
    return setLength(length);
  }
  FILE_ALLOCATION_INFO reserved{};
  reserved.AllocationSize.QuadPart = length;
  if (!SetFileInformationByHandle(handle_, FileAllocationInfo, &reserved,
                                  sizeof(reserved))) {
    return lastError();
  }
  static const std::string zeros(4 << 20, '\0');
  for (auto offset = filled; offset < length;
       offset += static_cast<int64_t>(zeros.size())) {
    if (stopExtending_) {
      // Ends where the zeros end, so the next opening fills the rest.
      return setLength(offset);
    }
    const auto count =
        std::min<int64_t>(static_cast<int64_t>(zeros.size()), length - offset);
    if (auto error = write(offset, zeros.data(), static_cast<size_t>(count))) {
      return error;
    }
  }
  return {};
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

std::error_code File::setLength(int64_t length)
{
  return ::ftruncate(fd_, static_cast<off_t>(length)) == 0 ? std::error_code()
                                                           : lastError();
}

// Files on the supported file systems are sparse without asking, and their
// reservations take no zero-filling.
std::error_code File::extend(int64_t length, Allocation allocation)
{
  if (size() >= length) {
    return {};
  }
  if (allocation == Allocation::Sparse) {
    return setLength(length);
  }
#  if defined(__APPLE__)
  fstore_t store{F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, static_cast<off_t>(length),
                 0};
  if (fcntl(fd_, F_PREALLOCATE, &store) == -1) {
    store.fst_flags = F_ALLOCATEALL;
    fcntl(fd_, F_PREALLOCATE, &store);
  }
  return setLength(length);
#  elif defined(__linux__) || defined(__ANDROID__)
  const auto result = ::posix_fallocate(fd_, 0, static_cast<off_t>(length));
  return result == 0 ? std::error_code()
                     : std::error_code(result, std::system_category());
#  else
  return setLength(length);
#  endif
}

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

Numbering numbering(std::string_view style)
{
  return style == "parentheses" ? Numbering::Parentheses : Numbering::Dot;
}

std::optional<std::string>
numberedPath(const std::string& path, Numbering numbering,
             const std::function<bool(const std::string&)>& taken)
{
  const auto full = toPath(path);
  const auto stem = toUtf8(full.parent_path() / full.stem());
  const auto extension = toUtf8(full.extension());
  for (int i = 1; i < 10000; ++i) {
    auto candidate = numbering == Numbering::Parentheses
                         ? stem + " (" + std::to_string(i) + ")" + extension
                         : stem + '.' + std::to_string(i) + extension;
    if (!fileExists(candidate) && !(taken && taken(candidate))) {
      return candidate;
    }
  }
  return std::nullopt;
}

namespace {
// rename(), or a copy where rename cannot cross volumes.
std::error_code moveFile(const fs::path& from, const fs::path& to)
{
  std::error_code error;
  fs::create_directories(to.parent_path(), error);
  if (error) {
    return error;
  }
  fs::rename(from, to, error);
  if (error != std::errc::cross_device_link) {
    return error;
  }
  error.clear();
  fs::copy_file(from, to, fs::copy_options::none, error);
  if (!error) {
    fs::remove(from, error);
  }
  return error;
}
} // namespace

std::string
moveFiles(const std::vector<std::pair<std::string, std::string>>& moves,
          const std::string& root)
{
  for (const auto& [from, to] : moves) {
    if (fileExists(to)) {
      return fmt::format("{} already exists", to);
    }
  }
  for (size_t i = 0; i < moves.size(); ++i) {
    const auto& [from, to] = moves[i];
    if (const auto error = moveFile(toPath(from), toPath(to))) {
      for (size_t j = i; j-- > 0;) {
        moveFile(toPath(moves[j].second), toPath(moves[j].first));
      }
      return fmt::format("Cannot move {} to {}: {}", from, to, error.message());
    }
  }
  const auto top = fs::absolute(toPath(root));
  for (const auto& [from, to] : moves) {
    std::error_code error;
    for (auto dir = fs::absolute(toPath(from)).parent_path();
         dir != top && dir.has_relative_path() &&
         dir.native().starts_with(top.native()) && fs::is_empty(dir, error) &&
         !error;
         dir = dir.parent_path()) {
      fs::remove(dir, error);
    }
  }
  return {};
}

int64_t fileSize(const std::string& path)
{
  std::error_code error;
  const auto size = fs::file_size(toPath(path), error);
  return error ? -1 : static_cast<int64_t>(size);
}

} // namespace aria2
