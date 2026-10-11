/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Gid.h"
#include "state/Sqlite.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aria2::ed2k {

// Progress of one ED2K download. Verified parts are complete; bytes inside
// other parts are kept as ranges until their part hash verifies.
struct DownloadRecord {
  Gid gid = 0;
  std::string fileHash;
  int64_t size = 0;
  std::string link;
  std::string path;
  int64_t modified =
      0; // the file's mtime when saved; a change voids the record
  bool complete = false;
  int64_t sharingSeconds = 0;
  std::vector<bool> parts;
  std::vector<std::pair<int64_t, int64_t>> received;
};

struct Credit {
  std::string userHash;
  uint64_t uploaded = 0;
  uint64_t downloaded = 0;
};

// ed2k/state.db. The schema is shared with earlier releases, so the user hash
// (and with it the credit other clients granted us) and unfinished downloads
// survive the upgrade. Errors are logged and leave the caller without state.
class Store {
public:
  explicit Store(const std::string& path);

  // Our user hash, created on first use.
  std::string userHash();
  std::string meta(const std::string& key);
  void setMeta(const std::string& key, const std::string& value);

  std::vector<Credit> credits();
  void saveCredits(const std::vector<Credit>& credits);

  std::optional<DownloadRecord> loadDownload(Gid gid);
  void saveDownload(const DownloadRecord& record);
  void removeDownload(Gid gid);

private:
  void open();

  sqlite::Database db_;
};

} // namespace aria2::ed2k
