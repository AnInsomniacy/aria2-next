/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Store.h"

#include "base/Log.h"
#include "ed2k/Hash.h"

#include <openssl/rand.h>

#include <algorithm>

namespace aria2::ed2k {

namespace {

constexpr int schemaVersion = 2;
// Progress inside an unverified part is stored in blocks of this size.
constexpr int64_t blockSize = 16 << 10;

// Bit i of a bitfield is the most significant bit of byte i / 8.
std::string packBits(const std::vector<bool>& bits)
{
  std::string out((bits.size() + 7) / 8, '\0');
  for (size_t i = 0; i < bits.size(); ++i) {
    if (bits[i]) {
      out[i / 8] = static_cast<char>(out[i / 8] | (0x80 >> (i % 8)));
    }
  }
  return out;
}

std::vector<bool> unpackBits(const std::string& data, size_t count)
{
  std::vector<bool> bits(count);
  for (size_t i = 0; i < count && i / 8 < data.size(); ++i) {
    bits[i] = (static_cast<uint8_t>(data[i / 8]) & (0x80 >> (i % 8))) != 0;
  }
  return bits;
}

bool covered(const std::vector<std::pair<int64_t, int64_t>>& ranges,
             int64_t begin, int64_t end)
{
  for (const auto& [first, last] : ranges) {
    if (first <= begin && end <= last) {
      return true;
    }
  }
  return false;
}

} // namespace

Store::Store(const std::string& path)
{
  try {
    db_ = sqlite::Database(path);
    open();
  }
  catch (const std::exception& error) {
    log::warn("ED2K state unavailable: {}", error.what());
    db_ = sqlite::Database();
  }
}

void Store::open()
{
  db_.execute("PRAGMA foreign_keys=ON");
  int version = 0;
  {
    sqlite::Statement query(db_, "PRAGMA user_version");
    if (query.step()) {
      version = static_cast<int>(query.integer(0));
    }
  }
  if (version != 0 && version != schemaVersion) {
    log::warn("Resetting ED2K state of unknown schema version {}", version);
    db_.execute(
        "DROP TABLE IF EXISTS download_pieces; DROP TABLE IF EXISTS downloads;"
        "DROP TABLE IF EXISTS source_seeds; DROP TABLE IF EXISTS credits;"
        "DROP TABLE IF EXISTS servers; DROP TABLE IF EXISTS meta;");
  }
  db_.execute(
      "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value BLOB NOT "
      "NULL);"
      "CREATE TABLE IF NOT EXISTS servers(endpoint TEXT PRIMARY KEY, payload "
      "BLOB NOT NULL);"
      "CREATE TABLE IF NOT EXISTS credits(user_hash BLOB PRIMARY KEY, "
      "uploaded INTEGER NOT NULL, downloaded INTEGER NOT NULL);"
      "CREATE TABLE IF NOT EXISTS source_seeds(file_hash BLOB NOT NULL, "
      "file_size INTEGER NOT NULL, ordinal INTEGER NOT NULL, host TEXT NOT "
      "NULL, "
      "port INTEGER NOT NULL, crypt_options INTEGER NOT NULL, user_hash BLOB "
      "NOT NULL, "
      "PRIMARY KEY(file_hash, file_size, ordinal));"
      "CREATE TABLE IF NOT EXISTS downloads(gid TEXT PRIMARY KEY, file_hash "
      "BLOB NOT NULL, "
      "file_size INTEGER NOT NULL, link TEXT NOT NULL, path TEXT NOT NULL, "
      "modified_time INTEGER NOT NULL, paused INTEGER NOT NULL, complete "
      "INTEGER NOT NULL, "
      "sharing_time INTEGER NOT NULL CHECK(sharing_time >= 0), bitfield BLOB "
      "NOT NULL, "
      "updated_at INTEGER NOT NULL DEFAULT (unixepoch()));"
      "CREATE TABLE IF NOT EXISTS download_pieces(gid TEXT NOT NULL REFERENCES "
      "downloads(gid) ON DELETE CASCADE, piece_index INTEGER NOT NULL, "
      "piece_length INTEGER NOT NULL, bitfield BLOB NOT NULL, PRIMARY KEY(gid, "
      "piece_index));"
      "PRAGMA user_version=2;");
}

std::string Store::meta(const std::string& key)
try {
  if (!db_) {
    return {};
  }
  sqlite::Statement query(db_, "SELECT value FROM meta WHERE key=?1");
  query.bind(1, key);
  return query.step() ? query.blob(0) : std::string();
}
catch (const std::exception& error) {
  log::warn("Cannot read ED2K state: {}", error.what());
  return {};
}

void Store::setMeta(const std::string& key, const std::string& value)
try {
  if (!db_) {
    return;
  }
  sqlite::Statement upsert(
      db_, "INSERT INTO meta(key, value) VALUES(?1, ?2) "
           "ON CONFLICT(key) DO UPDATE SET value=excluded.value");
  upsert.bind(1, key).bindBlob(2, value).run();
}
catch (const std::exception& error) {
  log::warn("Cannot save ED2K state: {}", error.what());
}

std::string Store::userHash()
{
  auto hash = meta("client_hash");
  if (hash.size() == HASH_LENGTH) {
    return hash;
  }
  hash.assign(HASH_LENGTH, '\0');
  RAND_bytes(reinterpret_cast<unsigned char*>(hash.data()),
             static_cast<int>(hash.size()));
  // eMule marks its user hashes with these two bytes.
  hash[5] = 14;
  hash[14] = 111;
  setMeta("client_hash", hash);
  return hash;
}

std::vector<Credit> Store::credits()
try {
  std::vector<Credit> result;
  if (!db_) {
    return result;
  }
  sqlite::Statement query(
      db_, "SELECT user_hash, uploaded, downloaded FROM credits");
  while (query.step()) {
    result.push_back({query.blob(0), static_cast<uint64_t>(query.integer(1)),
                      static_cast<uint64_t>(query.integer(2))});
  }
  return result;
}
catch (const std::exception& error) {
  log::warn("Cannot read ED2K credits: {}", error.what());
  return {};
}

void Store::saveCredits(const std::vector<Credit>& credits)
try {
  if (!db_) {
    return;
  }
  sqlite::Transaction transaction(db_);
  for (const auto& credit : credits) {
    sqlite::Statement row(
        db_, "INSERT INTO credits(user_hash, uploaded, downloaded) "
             "VALUES(?1, ?2, ?3) ON CONFLICT(user_hash) DO UPDATE SET "
             "uploaded=excluded.uploaded, downloaded=excluded.downloaded");
    row.bindBlob(1, credit.userHash)
        .bind(2, static_cast<int64_t>(credit.uploaded))
        .bind(3, static_cast<int64_t>(credit.downloaded))
        .run();
  }
  transaction.commit();
}
catch (const std::exception& error) {
  log::warn("Cannot save ED2K credits: {}", error.what());
}

std::optional<DownloadRecord> Store::loadDownload(Gid gid)
try {
  if (!db_) {
    return std::nullopt;
  }
  sqlite::Statement query(
      db_, "SELECT file_hash, file_size, link, path, modified_time, "
           "complete, sharing_time, bitfield FROM downloads WHERE gid=?1");
  query.bind(1, gidText(gid));
  if (!query.step()) {
    return std::nullopt;
  }
  DownloadRecord record;
  record.gid = gid;
  record.fileHash = query.blob(0);
  record.size = query.integer(1);
  record.link = query.text(2);
  record.path = query.text(3);
  record.modified = query.integer(4);
  record.complete = query.integer(5) != 0;
  record.sharingSeconds = query.integer(6);
  const auto partCount =
      static_cast<size_t>((record.size + PIECE_LENGTH - 1) / PIECE_LENGTH);
  record.parts = unpackBits(query.blob(7), partCount);
  sqlite::Statement pieces(
      db_, "SELECT piece_index, piece_length, bitfield FROM download_pieces "
           "WHERE gid=?1 ORDER BY piece_index");
  pieces.bind(1, gidText(gid));
  while (pieces.step()) {
    const auto index = pieces.integer(0);
    const auto begin = index * PIECE_LENGTH;
    const auto length = pieces.integer(1);
    if (index < 0 || begin >= record.size || length <= 0) {
      continue;
    }
    const auto blocks =
        unpackBits(pieces.blob(2),
                   static_cast<size_t>((length + blockSize - 1) / blockSize));
    for (size_t block = 0; block < blocks.size(); ++block) {
      if (!blocks[block]) {
        continue;
      }
      const auto first = begin + static_cast<int64_t>(block) * blockSize;
      const auto last = std::min(first + blockSize, begin + length);
      if (!record.received.empty() && record.received.back().second == first) {
        record.received.back().second = last;
      }
      else {
        record.received.emplace_back(first, last);
      }
    }
  }
  return record;
}
catch (const std::exception& error) {
  log::warn("Cannot read ED2K progress: {}", error.what());
  return std::nullopt;
}

void Store::saveDownload(const DownloadRecord& record)
try {
  if (!db_) {
    return;
  }
  sqlite::Transaction transaction(db_);
  sqlite::Statement upsert(
      db_,
      "INSERT INTO downloads(gid, file_hash, file_size, link, path, "
      "modified_time, paused, "
      "complete, sharing_time, bitfield, updated_at) "
      "VALUES(?1, ?2, ?3, ?4, ?5, ?6, 0, ?7, ?8, ?9, unixepoch()) "
      "ON CONFLICT(gid) DO UPDATE SET file_hash=excluded.file_hash, "
      "file_size=excluded.file_size, link=excluded.link, path=excluded.path, "
      "modified_time=excluded.modified_time, complete=excluded.complete, "
      "sharing_time=excluded.sharing_time, bitfield=excluded.bitfield, "
      "updated_at=excluded.updated_at");
  upsert.bind(1, gidText(record.gid))
      .bindBlob(2, record.fileHash)
      .bind(3, record.size)
      .bind(4, record.link)
      .bind(5, record.path)
      .bind(6, record.modified)
      .bind(7, int64_t{record.complete})
      .bind(8, record.sharingSeconds)
      .bindBlob(9, packBits(record.parts))
      .run();
  sqlite::Statement clear(db_, "DELETE FROM download_pieces WHERE gid=?1");
  clear.bind(1, gidText(record.gid)).run();
  for (size_t index = 0; index < record.parts.size(); ++index) {
    if (record.parts[index]) {
      continue;
    }
    const auto begin = static_cast<int64_t>(index) * PIECE_LENGTH;
    const auto length = std::min<int64_t>(PIECE_LENGTH, record.size - begin);
    std::vector<bool> blocks(
        static_cast<size_t>((length + blockSize - 1) / blockSize));
    bool any = false;
    for (size_t block = 0; block < blocks.size(); ++block) {
      const auto first = begin + static_cast<int64_t>(block) * blockSize;
      blocks[block] = covered(record.received, first,
                              std::min(first + blockSize, begin + length));
      any = any || blocks[block];
    }
    if (!any) {
      continue;
    }
    sqlite::Statement piece(
        db_, "INSERT INTO download_pieces(gid, piece_index, piece_length, "
             "bitfield) VALUES(?1, ?2, ?3, ?4)");
    piece.bind(1, gidText(record.gid))
        .bind(2, static_cast<int64_t>(index))
        .bind(3, length)
        .bindBlob(4, packBits(blocks))
        .run();
  }
  transaction.commit();
}
catch (const std::exception& error) {
  log::warn("Cannot save ED2K progress: {}", error.what());
}

void Store::removeDownload(Gid gid)
try {
  if (!db_) {
    return;
  }
  sqlite::Statement remove(db_, "DELETE FROM downloads WHERE gid=?1");
  remove.bind(1, gidText(gid)).run();
}
catch (const std::exception& error) {
  log::warn("Cannot remove ED2K progress: {}", error.what());
}

} // namespace aria2::ed2k
