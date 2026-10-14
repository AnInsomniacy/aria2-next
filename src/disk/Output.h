/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "disk/DiskPool.h"
#include "disk/File.h"

#include <functional>
#include <memory>
#include <string>

namespace aria2 {

// One output file. Operations run in order on the disk pool; callbacks run on
// the control thread. Writes take ownership of their buffers.
class Output : public std::enable_shared_from_this<Output> {
public:
  using Done = std::function<void(std::error_code)>;

  Output(DiskPool& pool, std::string path);
  const std::string& path() const { return path_; }

  // Opens the file. `truncate` discards existing content; a known `length`
  // is applied according to `allocation`.
  void open(bool truncate, Allocation allocation, int64_t length, Done done);
  void write(int64_t offset, std::string data, Done done);
  // Ends a reservation still writing its zeros, ahead of queued writes.
  void stopExtending() { file_->stopExtending(); }
  void close(Done done);

private:
  DiskPool& pool_;
  DiskPool::Strand strand_;
  std::string path_;
  std::shared_ptr<File> file_ = std::make_shared<File>();
};

} // namespace aria2
