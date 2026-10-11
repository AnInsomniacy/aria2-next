/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "state/Sqlite.h"

#include "base/Error.h"
#include "base/Path.h"

#include <utility>

namespace aria2::sqlite {

namespace {
[[noreturn]] void raise(sqlite3* db, std::string_view what)
{
  fail(ErrorCode::FileIo, "SQLite {}: {}", what,
       db ? sqlite3_errmsg(db) : "out of memory");
}
} // namespace

Database::Database(const std::string& path)
{
  std::error_code error;
  fs::create_directories(toPath(path).parent_path(), error);
  if (sqlite3_open_v2(path.c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                          SQLITE_OPEN_FULLMUTEX,
                      nullptr) != SQLITE_OK) {
    const auto db = std::exchange(db_, nullptr);
    std::string message = db ? sqlite3_errmsg(db) : "out of memory";
    sqlite3_close_v2(db);
    fail(ErrorCode::FileIo, "Cannot open {}: {}", path, message);
  }
  sqlite3_busy_timeout(db_, 5000);
  execute("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;");
}

Database::~Database()
{
  if (db_) {
    sqlite3_close_v2(db_);
  }
}

Database::Database(Database&& other) noexcept
    : db_(std::exchange(other.db_, nullptr))
{
}

Database& Database::operator=(Database&& other) noexcept
{
  if (this != &other) {
    if (db_) {
      sqlite3_close_v2(db_);
    }
    db_ = std::exchange(other.db_, nullptr);
  }
  return *this;
}

void Database::execute(std::string_view sql)
{
  char* message = nullptr;
  if (sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &message) !=
      SQLITE_OK) {
    std::string text = message ? message : "";
    sqlite3_free(message);
    fail(ErrorCode::FileIo, "SQLite: {}", text);
  }
}

Statement::Statement(const Database& db, std::string_view sql)
    : db_(db.handle())
{
  if (sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()),
                         &statement_, nullptr) != SQLITE_OK) {
    raise(db_, "prepare");
  }
}

Statement::~Statement() { sqlite3_finalize(statement_); }

Statement& Statement::bind(int index, std::string_view value)
{
  if (sqlite3_bind_text(statement_, index, value.data(),
                        static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    raise(db_, "bind");
  }
  return *this;
}

Statement& Statement::bindBlob(int index, std::string_view value)
{
  if (sqlite3_bind_blob(statement_, index, value.data(),
                        static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    raise(db_, "bind");
  }
  return *this;
}

Statement& Statement::bind(int index, int64_t value)
{
  if (sqlite3_bind_int64(statement_, index, value) != SQLITE_OK) {
    raise(db_, "bind");
  }
  return *this;
}

bool Statement::step()
{
  const auto result = sqlite3_step(statement_);
  if (result == SQLITE_ROW) {
    return true;
  }
  if (result != SQLITE_DONE) {
    raise(db_, "step");
  }
  return false;
}

std::string Statement::text(int column) const
{
  const auto* value = sqlite3_column_text(statement_, column);
  return value ? reinterpret_cast<const char*>(value) : "";
}

int64_t Statement::integer(int column) const
{
  return sqlite3_column_int64(statement_, column);
}

std::string Statement::blob(int column) const
{
  const auto* data =
      static_cast<const char*>(sqlite3_column_blob(statement_, column));
  return data ? std::string(data, sqlite3_column_bytes(statement_, column))
              : std::string();
}

Transaction::Transaction(Database& db) : db_(db) { db_.execute("BEGIN"); }

Transaction::~Transaction()
{
  if (!done_) {
    sqlite3_exec(db_.handle(), "ROLLBACK", nullptr, nullptr, nullptr);
  }
}

void Transaction::commit()
{
  db_.execute("COMMIT");
  done_ = true;
}

} // namespace aria2::sqlite
