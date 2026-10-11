/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "media/Control.h"
#include "state/Sqlite.h"

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace aria2::media {

struct Segment {
  int64_t period = 0;
  std::string track;
  int64_t number = 0;
  int64_t start = 0, duration = 0;
  std::string path, init, type;
  int64_t discontinuity = 0;
  int64_t timeOffset = 0; // DASH presentationTimeOffset in microseconds.
  bool hls = false;
  int64_t bytes = 0;
  std::string digest, initDigest;
};

struct Publication {
  std::string output, staging, digest;
  int64_t bytes = 0;
};

class Store {
public:
  Store(const std::string& directory, std::string gid);
  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;
  static void discard(const std::string& directory, const std::string& gid);
  void save(const Snapshot& snapshot);
  void saveTracks(const std::vector<Track>& tracks);
  bool load(Snapshot& snapshot);
  void commit(const Segment& segment);
  std::vector<Segment> segments();
  bool identity(const std::string& value);
  bool manifest(const std::string& name, const std::string& digest);
  void select(int64_t period, const std::string& type,
              const std::string& identity);
  void preparePublication(const Publication& publication);
  std::optional<Publication> publication();
  void clearPublication();
  void clearSegments();
  void remove();

private:
  sqlite::Database db_;
  std::string gid_;
};
} // namespace aria2::media
