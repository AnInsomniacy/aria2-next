/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace aria2::sqlite {

// Thin RAII wrappers. Errors throw Error(ErrorCode::FileIo).
class Database {
public:
  Database() = default;
  explicit Database(const std::string& path);
  ~Database();
  Database(Database&& other) noexcept;
  Database& operator=(Database&& other) noexcept;

  void execute(std::string_view sql);
  sqlite3* handle() const { return db_; }
  explicit operator bool() const { return db_ != nullptr; }

private:
  sqlite3* db_ = nullptr;
};

class Statement {
public:
  Statement(const Database& db, std::string_view sql);
  ~Statement();
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  Statement& bind(int index, std::string_view value);
  Statement& bind(int index, int64_t value);
  // Returns true while a row is available.
  bool step();
  void run() { step(); }
  std::string text(int column) const;
  int64_t integer(int column) const;
  std::string blob(int column) const;

private:
  sqlite3* db_;
  sqlite3_stmt* statement_ = nullptr;
};

class Transaction {
public:
  explicit Transaction(Database& db);
  ~Transaction();
  void commit();

private:
  Database& db_;
  bool done_ = false;
};

} // namespace aria2::sqlite
