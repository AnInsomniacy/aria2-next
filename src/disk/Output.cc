/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "disk/Output.h"

namespace aria2 {

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
      std::move(done));
}

void Output::write(int64_t offset, std::string data, Done done)
{
  pool_.run(
      strand_,
      [file = file_, offset, data = std::move(data)] {
        return file->isOpen() ? file->write(offset, data.data(), data.size())
                              : std::make_error_code(std::errc::bad_file_descriptor);
      },
      std::move(done));
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
      std::move(done));
}

} // namespace aria2
