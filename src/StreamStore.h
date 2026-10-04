/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_STREAM_STORE_H
#define D_STREAM_STORE_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <mutex>
#include <unordered_map>

struct sqlite3;

namespace aria2 {

struct StreamState {
  std::string gid;
  std::string uri;
  std::string path;
  std::string etag;
  std::string lastModified;
  int64_t totalLength = 0;
  int64_t completedLength = 0;
  std::vector<std::pair<int64_t, int64_t>> completedRanges;
};

class StreamStore {
public:
  explicit StreamStore(std::string path);
  ~StreamStore();

  bool open();
  bool load(StreamState& state, const std::string& gid,
            const std::string& path) const;
  bool save(const StreamState& state);
  bool remove(const std::string& gid);
  const std::string& path() const { return path_; }

private:
  void pruneMissingFiles();
  bool loadStored(StreamState& state, const std::string& gid,
                  const std::string& path) const;
  mutable std::mutex cacheMutex_;
  std::mutex writeMutex_;
  std::unordered_map<std::string, StreamState> states_;

  std::string path_;
  sqlite3* db_ = nullptr;
};

} // namespace aria2

#endif // D_STREAM_STORE_H
