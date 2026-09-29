/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef ARIA2_STREAM_ASYNC_DISK_WRITER_H
#define ARIA2_STREAM_ASYNC_DISK_WRITER_H

#include "DiskWriter.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace aria2 {
namespace stream {

// Runs native file writes on a worker thread instead of the caller's thread.
//
// The HTTP stream path writes from inside the libcurl write callback, which runs
// inside curl_multi_socket_action(), which runs inside the engine loop. A
// synchronous write there holds up every command in that loop, including
// JSON-RPC, so a slow device shows up as an unresponsive client.
//
// Ordering is preserved (chunks are written in submission order), so overlapping
// ranges keep the same last-writer-wins result as writing directly.
//
// Backpressure does not block: writeData() only enqueues, because the caller is
// the engine thread and waiting there is the defect this class removes. Bounded
// memory is therefore the caller's responsibility - it stops feeding the writer
// while isBacklogged() is true, which the stream path does by pausing the
// transfer in the libcurl write callback.
//
// Durability is explicit: flush() waits until nothing is outstanding. Any caller
// that records resume state or closes the output must call it first, so recorded
// ranges never describe bytes still in the queue.
class AsyncDiskWriter : public DiskWriter {
public:
  AsyncDiskWriter(std::unique_ptr<DiskWriter> inner, size_t queueLimit);
  ~AsyncDiskWriter() override;

  AsyncDiskWriter(const AsyncDiskWriter&) = delete;
  AsyncDiskWriter& operator=(const AsyncDiskWriter&) = delete;

  // Waits for all outstanding writes, then rethrows the first worker failure.
  void flush();

  // DiskWriter
  void initAndOpenFile(int64_t totalLength = 0) override;
  void openNewFile() override;
  void openFile(int64_t totalLength = 0) override;
  void closeFile() override;
  void openExistingFile(int64_t totalLength = 0) override;
  int64_t size() override;
  void writeData(const unsigned char* data, size_t len, int64_t offset) override;
  ssize_t readData(unsigned char* data, size_t len, int64_t offset) override;
  void truncate(int64_t length) override;
  void allocate(int64_t offset, int64_t length, bool sparse) override;
  void enableSparse() override;
  void enableReadOnly() override;
  void disableReadOnly() override;
  void enableMmap() override;
  void dropCache(int64_t len, int64_t offset) override;
  void flushOSBuffers() override;
  bool isFileBacked() const override;
  bool isWriteOffloaded() const override { return true; }
  void flushPendingWrites() override;
  bool hasWriteFailed() const override;
  bool hasPendingWrites() const override;

  // True when the backlog reached the queue limit, so the caller should stop
  // feeding this writer (the stream path pauses the transfer) until a later
  // poll observes it drained. This is the backpressure channel: writeData never
  // blocks, so bounded memory has to be enforced by the caller.
  bool isBacklogged() const override;

  // Bytes accepted but not yet completed by the worker. Diagnostics and tests.
  size_t pendingBytes() const;

private:
  struct Chunk {
    std::vector<unsigned char> data;
    int64_t offset = 0;
  };

  void run();
  void stop();

  std::unique_ptr<DiskWriter> inner_;
  const size_t queueLimit_;

  mutable std::mutex mutex_;
  std::condition_variable work_;
  std::condition_variable progress_;
  std::deque<Chunk> queue_;
  // Queued plus in-flight bytes, so flush() cannot return while the worker is
  // still inside a device call whose chunk already left the queue.
  size_t pendingBytes_ = 0;
  bool stopping_ = false;
  bool started_ = false;
  std::exception_ptr error_;
  std::thread worker_;
};

// Wraps a filesystem-backed writer so its writes run off the calling thread.
// Writers that cannot stall on a device (in-memory buffers) are returned as-is.
std::unique_ptr<DiskWriter> wrapAsyncWriter(std::unique_ptr<DiskWriter> writer);

} // namespace stream
} // namespace aria2

#endif // ARIA2_STREAM_ASYNC_DISK_WRITER_H
