/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "disk/Output.h"

#include "base/Log.h"

#include <chrono>

namespace aria2 {

// Disk work that keeps a download waiting is logged with its queue time, so
// slow or stalled storage shows up next to the download it holds up; at most
// every ten seconds per file, as a backlog slows every queued write.
Output::Done Output::timed(const char* operation, Done done)
{
  return [this, self = shared_from_this(), operation, done = std::move(done),
          since = std::chrono::steady_clock::now()](std::error_code error) {
    const auto now = std::chrono::steady_clock::now();
    const auto took =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - since)
            .count();
    if (took >= 1000 && now - lastSlow_ >= std::chrono::seconds(10)) {
      lastSlow_ = now;
      log::info("Disk: {} {} took {} ms", operation, path_, took);
    }
    done(error);
  };
}

Output::Output(DiskPool& pool, std::string path)
    : pool_(pool), strand_(pool.strand()), path_(std::move(path))
{
}

void Output::open(bool truncate, Allocation allocation, int64_t length,
                  Done done)
{
  pool_.run(
      strand_,
      [file = file_, path = path_, truncate, allocation, length] {
        if (auto error = file->open(path, truncate)) {
          return error;
        }
        return length > 0 ? file->extend(length, allocation)
                          : std::error_code();
      },
      timed("opening", std::move(done)));
}

void Output::write(int64_t offset, std::string data, Done done)
{
  pool_.run(
      strand_,
      [file = file_, offset, data = std::move(data)] {
        return file->isOpen() ? file->write(offset, data.data(), data.size())
                              : std::make_error_code(std::errc::bad_file_descriptor);
      },
      timed("writing to", std::move(done)));
}

void Output::close(Done done)
{
  stopExtending();
  pool_.run(
      strand_,
      [file = file_] {
        file->close();
        return std::error_code();
      },
      timed("closing", std::move(done)));
}

} // namespace aria2
