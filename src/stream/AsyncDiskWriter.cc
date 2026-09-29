/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "AsyncDiskWriter.h"

#include <algorithm>
#include <utility>

#include "Log.h"
#include "fmt.h"

namespace aria2 {
namespace stream {

AsyncDiskWriter::AsyncDiskWriter(std::unique_ptr<DiskWriter> inner,
                                 size_t queueLimit)
    : inner_(std::move(inner)), queueLimit_(std::max<size_t>(1, queueLimit))
{
}

AsyncDiskWriter::~AsyncDiskWriter() { stop(); }

void AsyncDiskWriter::stop()
{
  // Drain before signalling the worker to exit so no accepted payload is lost
  // when the writer is destroyed.
  try {
    flush();
  }
  catch (const std::exception& error) {
    A2_LOG_ERROR(fmt("Discarding pending writes while closing output: %s",
                     error.what()));
  }
  catch (...) {
    A2_LOG_ERROR("Discarding pending writes while closing output");
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  work_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void AsyncDiskWriter::run()
{
  for (;;) {
    Chunk chunk;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      work_.wait(lock, [this] { return !queue_.empty() || stopping_; });
      if (queue_.empty()) {
        // stopping_ and fully drained
        return;
      }
      chunk = std::move(queue_.front());
      queue_.pop_front();
    }
    try {
      inner_->writeData(chunk.data.data(), chunk.data.size(), chunk.offset);
    }
    catch (...) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!error_) {
        error_ = std::current_exception();
      }
      // Do not keep hammering a failed device; the failure is reported through
      // the engine thread's next write or flush.
      queue_.clear();
      pendingBytes_ = 0;
      progress_.notify_all();
      return;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pendingBytes_ -= chunk.data.size();
    }
    progress_.notify_all();
  }
}

void AsyncDiskWriter::flush()
{
  std::exception_ptr error;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    progress_.wait(lock, [this] { return pendingBytes_ == 0 || error_ != nullptr; });
    error = error_;
  }
  if (error) {
    std::rethrow_exception(error);
  }
}

void AsyncDiskWriter::flushPendingWrites()
{
  // Called from paths that cannot propagate a device failure (status
  // checkpoints). The error is re-raised by the next writeData() or flush()
  // so it still reaches the task's failure handling.
  try {
    flush();
  }
  catch (const std::exception& error) {
    A2_LOG_ERROR(fmt("Pending output write failed: %s", error.what()));
  }
  catch (...) {
    A2_LOG_ERROR("Pending output write failed");
  }
}

size_t AsyncDiskWriter::pendingBytes() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return pendingBytes_;
}

void AsyncDiskWriter::writeData(const unsigned char* data, size_t len,
                                int64_t offset)
{
  if (len == 0) {
    return;
  }
  std::unique_lock<std::mutex> lock(mutex_);
  if (error_) {
    auto error = error_;
    lock.unlock();
    std::rethrow_exception(error);
  }
  if (!started_) {
    // Started on first write so a writer that is opened but never used costs no
    // thread.
    started_ = true;
    worker_ = std::thread([this] { run(); });
  }
  // Never waits: the caller is the engine thread, and blocking it is the defect
  // this class exists to remove. Backpressure is the caller's job, which it
  // applies by pausing the transfer while isBacklogged() is true.
  Chunk chunk;
  chunk.data.assign(data, data + len);
  chunk.offset = offset;
  pendingBytes_ += chunk.data.size();
  queue_.push_back(std::move(chunk));
  lock.unlock();
  work_.notify_one();
}

void AsyncDiskWriter::initAndOpenFile(int64_t totalLength)
{
  flush();
  inner_->initAndOpenFile(totalLength);
}

void AsyncDiskWriter::openNewFile()
{
  flush();
  inner_->openNewFile();
}

void AsyncDiskWriter::openFile(int64_t totalLength)
{
  flush();
  inner_->openFile(totalLength);
}

void AsyncDiskWriter::openExistingFile(int64_t totalLength)
{
  flush();
  inner_->openExistingFile(totalLength);
}

void AsyncDiskWriter::closeFile()
{
  // Payload must reach the file before the handle closes.
  flush();
  inner_->closeFile();
}

int64_t AsyncDiskWriter::size()
{
  flush();
  return inner_->size();
}

ssize_t AsyncDiskWriter::readData(unsigned char* data, size_t len,
                                  int64_t offset)
{
  // Reads must observe previously accepted writes.
  flush();
  return inner_->readData(data, len, offset);
}

void AsyncDiskWriter::truncate(int64_t length)
{
  flush();
  inner_->truncate(length);
}

void AsyncDiskWriter::allocate(int64_t offset, int64_t length, bool sparse)
{
  flush();
  inner_->allocate(offset, length, sparse);
}

void AsyncDiskWriter::enableSparse()
{
  // Shape-only flag; no queued payload depends on it.
  inner_->enableSparse();
}

void AsyncDiskWriter::enableReadOnly() { inner_->enableReadOnly(); }

void AsyncDiskWriter::disableReadOnly() { inner_->disableReadOnly(); }

void AsyncDiskWriter::enableMmap() { inner_->enableMmap(); }

void AsyncDiskWriter::dropCache(int64_t len, int64_t offset)
{
  flush();
  inner_->dropCache(len, offset);
}

void AsyncDiskWriter::flushOSBuffers()
{
  flush();
  inner_->flushOSBuffers();
}

bool AsyncDiskWriter::isFileBacked() const { return inner_->isFileBacked(); }

bool AsyncDiskWriter::hasWriteFailed() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return error_ != nullptr;
}

bool AsyncDiskWriter::hasPendingWrites() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return pendingBytes_ != 0;
}

bool AsyncDiskWriter::isBacklogged() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return pendingBytes_ >= queueLimit_;
}

std::unique_ptr<DiskWriter> wrapAsyncWriter(std::unique_ptr<DiskWriter> writer)
{
  if (!writer || !writer->isFileBacked() || writer->isWriteOffloaded()) {
    return writer;
  }
  // Absorbs ordinary write bursts without holding the engine thread, while a
  // device that has genuinely stopped still bounds memory.
  constexpr size_t DEFAULT_QUEUE_LIMIT = 32 * 1024 * 1024;
  return std::unique_ptr<DiskWriter>(
      new AsyncDiskWriter(std::move(writer), DEFAULT_QUEUE_LIMIT));
}

} // namespace stream
} // namespace aria2
