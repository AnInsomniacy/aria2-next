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
#include "RotatingLogFile.h"

#include "BufferedFile.h"
#include "DlAbortEx.h"
#include "File.h"
#include "fmt.h"
#include "util.h"

namespace aria2 {

RotatingLogFile::RotatingLogFile(const std::string& path, int64_t maxFileSize,
                                 size_t maxFiles)
    : path_(path),
      maxFileSize_(maxFileSize),
      maxFiles_(maxFiles),
      currentSize_(0)
{
}

RotatingLogFile::~RotatingLogFile() = default;

std::string RotatingLogFile::rotatedPath(size_t index) const
{
  return path_ + "." + util::uitos(index);
}

bool RotatingLogFile::openActive(const char* mode)
{
  file_.reset(new BufferedFile(path_.c_str(), mode));
  if (!*file_) {
    file_.reset();
    return false;
  }
  currentSize_ = File(path_).size();
  return true;
}

bool RotatingLogFile::removeFile(const std::string& path)
{
  File file(path);
  return !file.exists() || file.remove();
}

bool RotatingLogFile::truncateFile(const std::string& path)
{
  BufferedFile file(path.c_str(), BufferedFile::WRITE);
  return file && file.close() != EOF;
}

bool RotatingLogFile::reconcile()
{
  for (size_t i = maxFiles_; i <= MAX_FILES; ++i) {
    if (!removeFile(rotatedPath(i))) {
      return false;
    }
  }

  for (size_t i = 1; i < maxFiles_; ++i) {
    File file(rotatedPath(i));
    if (file.exists() && file.size() > maxFileSize_ && !file.remove()) {
      return false;
    }
  }

  File active(path_);
  return !active.exists() || active.size() <= maxFileSize_ ||
         truncateFile(path_);
}

void RotatingLogFile::open()
{
  close();
  if (maxFileSize_ <= 0 || maxFiles_ == 0 || maxFiles_ > MAX_FILES) {
    throw DL_ABORT_EX("Invalid log rotation limits");
  }
  if (!reconcile() || !openActive(BufferedFile::APPEND)) {
    close();
    throw DL_ABORT_EX(fmt("Failed to initialize log file %s", path_.c_str()));
  }
}

void RotatingLogFile::close()
{
  file_.reset();
  currentSize_ = 0;
}

bool RotatingLogFile::isOpen() const { return static_cast<bool>(file_); }

std::string RotatingLogFile::boundRecord(const std::string& record) const
{
  if (record.size() <= static_cast<size_t>(maxFileSize_)) {
    return record;
  }

  static const std::string marker = "\n[log record truncated]\n";
  const auto limit = static_cast<size_t>(maxFileSize_);
  if (limit <= marker.size()) {
    return record.substr(0, limit);
  }
  return record.substr(0, limit - marker.size()) + marker;
}

bool RotatingLogFile::rotate()
{
  file_.reset();

  if (maxFiles_ == 1) {
    return truncateFile(path_) && openActive(BufferedFile::APPEND);
  }

  if (!removeFile(rotatedPath(maxFiles_ - 1))) {
    return false;
  }
  for (size_t i = maxFiles_ - 1; i > 1; --i) {
    File source(rotatedPath(i - 1));
    if (source.exists() && !source.renameTo(rotatedPath(i))) {
      return false;
    }
  }

  File active(path_);
  if (active.exists()) {
    if (active.size() > 0) {
      if (!active.renameTo(rotatedPath(1))) {
        return false;
      }
    }
    else if (!active.remove()) {
      return false;
    }
  }
  return openActive(BufferedFile::APPEND);
}

bool RotatingLogFile::write(const std::string& record)
{
  if (!file_) {
    return false;
  }

  const auto bounded = boundRecord(record);
  if (currentSize_ > 0 &&
      currentSize_ + static_cast<int64_t>(bounded.size()) > maxFileSize_ &&
      !rotate()) {
    close();
    return false;
  }

  if (file_->write(bounded.data(), bounded.size()) != bounded.size() ||
      file_->flush() != 0) {
    close();
    return false;
  }
  currentSize_ += bounded.size();
  return true;
}

} // namespace aria2
