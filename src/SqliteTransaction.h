/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef ARIA2_SQLITE_TRANSACTION_H
#define ARIA2_SQLITE_TRANSACTION_H
#include <sqlite3.h>
#include <stdexcept>

namespace aria2::sqlite {
class Transaction {
public:
  explicit Transaction(sqlite3* db) : db_(db) { execute("BEGIN IMMEDIATE"); }
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  ~Transaction()
  {
    if (!committed_)
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
  }
  void commit()
  {
    execute("COMMIT");
    committed_ = true;
  }

private:
  void execute(const char* sql)
  {
    if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
      throw std::runtime_error(sqlite3_errmsg(db_));
  }
  sqlite3* db_;
  bool committed_ = false;
};
} // namespace aria2::sqlite
#endif
