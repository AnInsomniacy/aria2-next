/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Gid.h"
#include "state/Sqlite.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aria2 {

class Engine;

using ByteRanges = std::vector<std::pair<int64_t, int64_t>>; // [begin, end)

// Resume record of one HTTP/SFTP download. Its presence also marks the output
// file as created by this engine.
struct TransferRecord {
  Gid gid = 0;
  std::string uri;
  std::string path;
  std::string etag;
  std::string lastModified;
  int64_t totalLength = -1;
  ByteRanges completed;
};

std::string encodeRanges(const ByteRanges& ranges);
std::optional<ByteRanges> decodeRanges(std::string_view text);

// Engine state below --state-dir:
//   stream/state.db      HTTP and SFTP resume records
//   bittorrent/          libtorrent session and resume data
//   ed2k/state.db        ED2K identity, credits and part files
//   media/               HLS/DASH task state
class StateStore {
public:
  explicit StateStore(Engine& engine);

  // Absolute directory for one protocol; created on demand.
  std::string directory(std::string_view protocol) const;

  std::optional<TransferRecord> loadTransfer(Gid gid);
  void saveTransfer(const TransferRecord& record);
  void removeTransfer(Gid gid);

private:
  sqlite::Database& transfers();

  std::string root_;
  std::optional<sqlite::Database> transfers_;
};

std::string defaultStateDirectory();

} // namespace aria2
