/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "state/StateStore.h"

#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "core/Engine.h"

#include <cstdlib>

#ifdef _WIN32
#  include <shlobj.h>
#  include <windows.h>
#endif

namespace aria2 {

std::string encodeRanges(const ByteRanges& ranges)
{
  std::string result;
  for (const auto& [begin, end] : ranges) {
    if (!result.empty()) {
      result += ',';
    }
    result += std::to_string(begin) + '-' + std::to_string(end);
  }
  return result;
}

std::optional<ByteRanges> decodeRanges(std::string_view text)
{
  ByteRanges ranges;
  for (const auto& item : text::split(text, ',')) {
    const auto dash = item.find('-');
    const auto begin = text::toInt(std::string_view(item).substr(0, dash));
    const auto end = dash == std::string::npos
                         ? std::nullopt
                         : text::toInt(std::string_view(item).substr(dash + 1));
    if (!begin || !end || *end <= *begin ||
        (!ranges.empty() && *begin < ranges.back().second)) {
      return std::nullopt;
    }
    ranges.emplace_back(*begin, *end);
  }
  return ranges;
}

std::string defaultStateDirectory()
{
#ifdef _WIN32
  PWSTR path = nullptr;
  std::string root;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
    root = toUtf8(fs::path(path));
  }
  CoTaskMemFree(path);
  return root.empty() ? std::string() : root + "/aria2-next";
#elif defined(__APPLE__)
  return homeDirectory() + "/Library/Application Support/aria2-next";
#else
  const auto* xdg = std::getenv("XDG_STATE_HOME");
  const std::string root = xdg && fs::path(xdg).is_absolute()
                               ? std::string(xdg)
                               : homeDirectory() + "/.local/state";
  return root + "/aria2-next";
#endif
}

StateStore::StateStore(Engine& engine)
    : root_(engine.global().get("state-dir"))
{
  if (root_.empty()) {
    root_ = defaultStateDirectory();
  }
  root_ = toUtf8(fs::absolute(toPath(root_)));
}

std::string StateStore::directory(std::string_view protocol) const
{
  const auto path = joinPath(root_, protocol);
  std::error_code error;
  fs::create_directories(toPath(path), error);
  return path;
}

sqlite::Database& StateStore::transfers()
{
  if (!transfers_) {
    transfers_.emplace(joinPath(directory("stream"), "state.db"));
    // Schema shared with earlier releases so unfinished downloads resume.
    transfers_->execute(
        "CREATE TABLE IF NOT EXISTS downloads("
        "gid TEXT PRIMARY KEY, uri TEXT NOT NULL, path TEXT NOT NULL, "
        "etag TEXT NOT NULL, last_modified TEXT NOT NULL, "
        "total_length INTEGER NOT NULL, completed_length INTEGER NOT NULL, "
        "completed_ranges TEXT NOT NULL, "
        "updated_at INTEGER NOT NULL DEFAULT (unixepoch()));");
  }
  return *transfers_;
}

std::optional<TransferRecord> StateStore::loadTransfer(Gid gid)
try {
  sqlite::Statement query(
      transfers(), "SELECT uri,etag,last_modified,total_length,"
                   "completed_ranges,path FROM downloads WHERE gid=?1");
  query.bind(1, gidText(gid));
  if (!query.step()) {
    return std::nullopt;
  }
  auto ranges = decodeRanges(query.text(4));
  if (!ranges) {
    return std::nullopt;
  }
  return TransferRecord{gid,
                        query.text(0),
                        query.text(5),
                        query.text(1),
                        query.text(2),
                        query.integer(3),
                        std::move(*ranges)};
}
catch (const std::exception& error) {
  log::warn("Resume data unavailable: {}", error.what());
  return std::nullopt;
}

void StateStore::saveTransfer(const TransferRecord& record)
try {
  int64_t completed = 0;
  for (const auto& [begin, end] : record.completed) {
    completed += end - begin;
  }
  auto& db = transfers();
  sqlite::Transaction transaction(db);
  sqlite::Statement stale(db, "DELETE FROM downloads WHERE path=?1 AND gid<>?2");
  stale.bind(1, record.path).bind(2, gidText(record.gid)).run();
  sqlite::Statement upsert(
      db, "INSERT INTO downloads(gid,uri,path,etag,last_modified,total_length,"
          "completed_length,completed_ranges,updated_at) "
          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,unixepoch()) "
          "ON CONFLICT(gid) DO UPDATE SET uri=excluded.uri,path=excluded.path,"
          "etag=excluded.etag,last_modified=excluded.last_modified,"
          "total_length=excluded.total_length,"
          "completed_length=excluded.completed_length,"
          "completed_ranges=excluded.completed_ranges,"
          "updated_at=excluded.updated_at");
  upsert.bind(1, gidText(record.gid))
      .bind(2, record.uri)
      .bind(3, record.path)
      .bind(4, record.etag)
      .bind(5, record.lastModified)
      .bind(6, record.totalLength)
      .bind(7, completed)
      .bind(8, encodeRanges(record.completed))
      .run();
  transaction.commit();
}
catch (const std::exception& error) {
  log::warn("Could not save resume data: {}", error.what());
}

std::optional<TransferRecord> StateStore::claimTransfer(const std::string& path,
                                                        Gid gid)
try {
  auto& db = transfers();
  sqlite::Transaction transaction(db);
  sqlite::Statement query(db, "SELECT gid,path FROM downloads WHERE gid<>?1");
  query.bind(1, gidText(gid));
  std::string owner;
  while (owner.empty() && query.step()) {
    if (samePath(query.text(1), path)) {
      owner = query.text(0);
    }
  }
  if (owner.empty()) {
    return std::nullopt;
  }
  sqlite::Statement mine(db, "DELETE FROM downloads WHERE gid=?1");
  mine.bind(1, gidText(gid)).run();
  sqlite::Statement claim(db, "UPDATE downloads SET gid=?1 WHERE gid=?2");
  claim.bind(1, gidText(gid)).bind(2, owner).run();
  transaction.commit();
  return loadTransfer(gid);
}
catch (const std::exception& error) {
  log::warn("Resume data unavailable: {}", error.what());
  return std::nullopt;
}

void StateStore::removeTransfer(Gid gid)
try {
  sqlite::Statement remove(transfers(), "DELETE FROM downloads WHERE gid=?1");
  remove.bind(1, gidText(gid)).run();
}
catch (const std::exception& error) {
  log::warn("Could not remove resume data: {}", error.what());
}

} // namespace aria2
