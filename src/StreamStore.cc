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
#include "StreamStore.h"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include <sqlite3.h>

#include <utility>
#include <vector>

#include "File.h"
#include "Log.h"
#include "SqliteDiagnostics.h"
#include "SqliteTransaction.h"
#include <limits>
#include "fmt.h"

namespace aria2 {

namespace {
bool validState(const StreamState& state)
{
  if (state.uri.empty() || state.path.empty() || state.totalLength < 0 ||
      state.completedLength < 0)
    return false;
  int64_t length = 0, previous = 0;
  for (const auto& range : state.completedRanges) {
    if (range.first < previous || range.second <= range.first ||
        (state.totalLength > 0 && range.second > state.totalLength) ||
        range.second - range.first >
            std::numeric_limits<int64_t>::max() - length)
      return false;
    length += range.second - range.first;
    previous = range.second;
  }
  return length == state.completedLength &&
         (state.totalLength == 0 || state.completedLength <= state.totalLength);
}

class Statement {
public:
  Statement(sqlite3* db, const char* sql)
  {
    const auto result = sqlite3_prepare_v2(db, sql, -1, &value_, nullptr);
    if (result != SQLITE_OK) {
      A2_LOG_ERROR(fmt("component=storage store=stream event=sqlite_failed %s",
                       sqlite::diagnostic(db, result, "prepare").c_str()));
      value_ = nullptr;
    }
  }

  ~Statement()
  {
    if (value_) {
      sqlite3_finalize(value_);
    }
  }

  sqlite3_stmt* get() const { return value_; }
  explicit operator bool() const { return value_ != nullptr; }

private:
  sqlite3_stmt* value_ = nullptr;
};

bool bindText(sqlite3_stmt* statement, int index, const std::string& value)
{
  const auto result =
      sqlite3_bind_text(statement, index, value.data(),
                        static_cast<int>(value.size()), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    A2_LOG_ERROR(fmt(
        "component=storage store=stream event=sqlite_failed index=%d %s", index,
        sqlite::diagnostic(sqlite3_db_handle(statement), result, "bind_text")
            .c_str()));
  }
  return result == SQLITE_OK;
}

bool bindInt64(sqlite3_stmt* statement, int index, int64_t value)
{
  const auto result = sqlite3_bind_int64(statement, index, value);
  if (result != SQLITE_OK) {
    A2_LOG_ERROR(fmt(
        "component=storage store=stream event=sqlite_failed index=%d %s", index,
        sqlite::diagnostic(sqlite3_db_handle(statement), result, "bind_int64")
            .c_str()));
  }
  return result == SQLITE_OK;
}

int step(sqlite3_stmt* statement, const char* operation)
{
  const auto result = sqlite3_step(statement);
  if (result != SQLITE_ROW && result != SQLITE_DONE) {
    A2_LOG_ERROR(
        fmt("component=storage store=stream event=sqlite_failed %s",
            sqlite::diagnostic(sqlite3_db_handle(statement), result, operation)
                .c_str()));
  }
  return result;
}

std::string textColumn(sqlite3_stmt* statement, int index)
{
  const auto value = sqlite3_column_text(statement, index);
  const auto size = sqlite3_column_bytes(statement, index);
  return value && size > 0 ? std::string(reinterpret_cast<const char*>(value),
                                         static_cast<size_t>(size))
                           : std::string();
}

std::string encodeRanges(const std::vector<std::pair<int64_t, int64_t>>& ranges)
{
  std::string result;
  for (const auto& range : ranges) {
    if (!result.empty()) {
      result += ',';
    }
    result += std::to_string(range.first);
    result += '-';
    result += std::to_string(range.second);
  }
  return result;
}

bool decodeRanges(std::vector<std::pair<int64_t, int64_t>>& ranges,
                  const std::string& value)
{
  ranges.clear();
  size_t offset = 0;
  while (offset < value.size()) {
    const auto separator = value.find('-', offset);
    const auto end = value.find(',', separator);
    if (separator == std::string::npos) {
      return false;
    }
    try {
      const auto first = std::stoll(value.substr(offset, separator - offset));
      const auto last =
          std::stoll(value.substr(separator + 1, end - separator - 1));
      if (first < 0 || last <= first ||
          (!ranges.empty() && ranges.back().second > first)) {
        return false;
      }
      ranges.emplace_back(first, last);
    }
    catch (const std::exception&) {
      return false;
    }
    if (end == std::string::npos) {
      break;
    }
    offset = end + 1;
  }
  return true;
}

} // namespace

StreamStore::StreamStore(std::string path) : path_(std::move(path)) {}

StreamStore::~StreamStore()
{
  if (db_) {
    sqlite3_close_v2(db_);
  }
}

bool StreamStore::open()
{
  if (db_) {
    return true;
  }
  if (path_.empty()) {
    return false;
  }
  sqlite::configureNativeLogging();
  File directory(File(path_).getDirname());
  if (!directory.isDir() && !directory.mkdirs()) {
    A2_LOG_ERROR(fmt("component=storage store=stream "
                     "event=database_directory_failed path=%s",
                     logging::sanitizeText(directory.getPath()).c_str()));
    return false;
  }
  const auto openResult = sqlite3_open_v2(
      path_.c_str(), &db_,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
      nullptr);
  if (openResult != SQLITE_OK) {
    A2_LOG_ERROR(fmt(
        "component=storage store=stream event=sqlite_failed path=%s %s",
        path_.c_str(), sqlite::diagnostic(db_, openResult, "open").c_str()));
    if (db_) {
      sqlite3_close_v2(db_);
      db_ = nullptr;
    }
    return false;
  }
  sqlite::configureConnection(db_);
  const auto timeoutResult = sqlite3_busy_timeout(db_, 5000);
  if (timeoutResult != SQLITE_OK) {
    A2_LOG_ERROR(
        fmt("component=storage store=stream event=sqlite_failed %s",
            sqlite::diagnostic(db_, timeoutResult, "busy_timeout").c_str()));
    sqlite3_close_v2(db_);
    db_ = nullptr;
    return false;
  }
  int version = 0;
  {
    Statement versionQuery(db_, "PRAGMA user_version");
    if (versionQuery && step(versionQuery.get(), "read_schema") == SQLITE_ROW) {
      version = sqlite3_column_int(versionQuery.get(), 0);
    }
  }
  const auto resetResult =
      version != 0 && version != 2
          ? sqlite3_exec(db_, "DROP TABLE IF EXISTS downloads", nullptr,
                         nullptr, nullptr)
          : SQLITE_OK;
  if (resetResult != SQLITE_OK) {
    A2_LOG_ERROR(
        fmt("component=storage store=stream event=sqlite_failed %s",
            sqlite::diagnostic(db_, resetResult, "reset_schema").c_str()));
    sqlite3_close_v2(db_);
    db_ = nullptr;
    return false;
  }
  const char* schema =
      "PRAGMA journal_mode=WAL;"
      "PRAGMA synchronous=FULL;"
      "PRAGMA auto_vacuum=INCREMENTAL;"
      "CREATE TABLE IF NOT EXISTS downloads("
      "gid TEXT PRIMARY KEY, uri TEXT NOT NULL, path TEXT NOT NULL, "
      "etag TEXT NOT NULL, last_modified TEXT NOT NULL, "
      "total_length INTEGER NOT NULL, completed_length INTEGER NOT NULL, "
      "completed_ranges TEXT NOT NULL, "
      "updated_at INTEGER NOT NULL DEFAULT (unixepoch()));"
      "PRAGMA user_version=2;";
  char* message = nullptr;
  const auto schemaResult =
      sqlite3_exec(db_, schema, nullptr, nullptr, &message);
  if (schemaResult != SQLITE_OK) {
    A2_LOG_ERROR(
        fmt("component=storage store=stream event=sqlite_failed %s "
            "detail=%s",
            sqlite::diagnostic(db_, schemaResult, "initialize_schema").c_str(),
            logging::sanitizeText(message ? message : "").c_str()));
    sqlite3_free(message);
    sqlite3_close_v2(db_);
    db_ = nullptr;
    return false;
  }
  pruneMissingFiles();
  Statement rows(db_, "SELECT gid,path FROM downloads");
  if (!rows)
    return false;
  while (step(rows.get(), "restore_cache") == SQLITE_ROW) {
    StreamState state;
    if (loadStored(state, textColumn(rows.get(), 0), textColumn(rows.get(), 1)))
      states_.emplace(state.gid, std::move(state));
  }
  return true;
}

void StreamStore::pruneMissingFiles()
{
  Statement query(db_, "SELECT gid,path FROM downloads");
  if (!query) {
    return;
  }
  std::vector<std::string> missing;
  int queryResult;
  while ((queryResult = step(query.get(), "prune_scan")) == SQLITE_ROW) {
    auto path = textColumn(query.get(), 1);
    if (!path.empty() && !File(path).exists()) {
      missing.push_back(textColumn(query.get(), 0));
    }
  }
  for (const auto& gid : missing) {
    remove(gid);
  }
  if (!missing.empty()) {
    sqlite3_exec(db_,
                 "PRAGMA wal_checkpoint(PASSIVE);"
                 "PRAGMA incremental_vacuum;",
                 nullptr, nullptr, nullptr);
  }
}

bool StreamStore::load(StreamState& state, const std::string& gid,
                       const std::string& path) const
{
  // RPC-triggered task creation must not wait for another task's SQLite fsync.
  std::lock_guard<std::mutex> lock(cacheMutex_);
  const auto found = states_.find(gid);
  if (found == states_.end() || found->second.path != path)
    return false;
  state = found->second;
  return true;
}

bool StreamStore::loadStored(StreamState& state, const std::string& gid,
                             const std::string& path) const
{
  if (!db_) {
    return false;
  }
  Statement statement(
      db_,
      "SELECT gid,uri,path,etag,last_modified,total_length,completed_length "
      ",completed_ranges "
      "FROM downloads WHERE gid=?1 AND path=?2");
  if (!statement || !bindText(statement.get(), 1, gid) ||
      !bindText(statement.get(), 2, path) ||
      step(statement.get(), "load") != SQLITE_ROW) {
    return false;
  }
  StreamState value;
  value.gid = textColumn(statement.get(), 0);
  value.uri = textColumn(statement.get(), 1);
  value.path = textColumn(statement.get(), 2);
  value.etag = textColumn(statement.get(), 3);
  value.lastModified = textColumn(statement.get(), 4);
  value.totalLength = sqlite3_column_int64(statement.get(), 5);
  value.completedLength = sqlite3_column_int64(statement.get(), 6);
  if (!decodeRanges(value.completedRanges, textColumn(statement.get(), 7))) {
    return false;
  }
  if (!validState(value))
    return false;
  state = std::move(value);
  return true;
}

bool StreamStore::save(const StreamState& state)
try {
  std::lock_guard<std::mutex> writeLock(writeMutex_);
  if (!db_ || state.gid.empty() || !validState(state)) {
    return false;
  }
  sqlite::Transaction transaction(db_);
  Statement removeStale(db_, "DELETE FROM downloads WHERE path=?1 AND gid<>?2");
  if (!removeStale || !bindText(removeStale.get(), 1, state.path) ||
      !bindText(removeStale.get(), 2, state.gid) ||
      step(removeStale.get(), "remove_stale") != SQLITE_DONE) {
    return false;
  }
  Statement statement(
      db_,
      "INSERT INTO downloads(gid,uri,path,etag,last_modified,total_length,"
      "completed_length,completed_ranges,updated_at) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,unixepoch()) "
      "ON CONFLICT(gid) DO UPDATE SET uri=excluded.uri,path=excluded.path,"
      "etag=excluded.etag,last_modified=excluded.last_modified,"
      "total_length=excluded.total_length,"
      "completed_length=excluded.completed_length,"
      "completed_ranges=excluded.completed_ranges,updated_at=excluded.updated_"
      "at");
  const bool saved =
      statement && bindText(statement.get(), 1, state.gid) &&
      bindText(statement.get(), 2, state.uri) &&
      bindText(statement.get(), 3, state.path) &&
      bindText(statement.get(), 4, state.etag) &&
      bindText(statement.get(), 5, state.lastModified) &&
      bindInt64(statement.get(), 6, state.totalLength) &&
      bindInt64(statement.get(), 7, state.completedLength) &&
      bindText(statement.get(), 8, encodeRanges(state.completedRanges)) &&
      step(statement.get(), "save") == SQLITE_DONE;
  if (saved) {
    transaction.commit();
    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (auto it = states_.begin(); it != states_.end();) {
      if (it->second.path == state.path)
        it = states_.erase(it);
      else
        ++it;
    }
    states_[state.gid] = state;
  }
  return saved;
}
catch (const std::exception& error) {
  A2_LOG_ERROR(
      fmt("component=storage store=stream event=checkpoint_failed message=%s",
          error.what()));
  return false;
}

bool StreamStore::remove(const std::string& gid)
{
  std::lock_guard<std::mutex> writeLock(writeMutex_);
  if (!db_) {
    return false;
  }
  Statement statement(db_, "DELETE FROM downloads WHERE gid=?1");
  const bool removed = statement && bindText(statement.get(), 1, gid) &&
                       step(statement.get(), "remove_gid") == SQLITE_DONE;
  if (removed) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    states_.erase(gid);
  }
  return removed;
}

} // namespace aria2
