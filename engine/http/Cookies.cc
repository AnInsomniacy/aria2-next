/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/Cookies.h"

#include "base/Error.h"
#include "base/Path.h"

#include <sqlite3.h>

#include <cstring>
#include <fstream>
#include <memory>

namespace aria2::http {

bool isSqliteCookieFile(const std::string& path)
{
  static constexpr char header[] = "SQLite format 3";
  std::ifstream input(toPath(path), std::ios::binary);
  char buffer[sizeof(header)] = {};
  return input.read(buffer, sizeof(buffer)) &&
         std::memcmp(buffer, header, sizeof(header)) == 0;
}

std::string fileRevision(const std::string& path)
{
  std::error_code error;
  const auto size = fs::file_size(toPath(path), error);
  const auto time = fs::last_write_time(toPath(path), error);
  return std::to_string(size) + ':' +
         std::to_string(static_cast<long long>(time.time_since_epoch().count()));
}

std::vector<std::string> sqliteCookieLines(const std::string& path)
{
  // immutable=1 reads the store while the browser holds it locked.
  const auto absolute = toUtf8(fs::absolute(toPath(path)));
  std::string uri = absolute.starts_with('/') ? "file:" : "file:/";
  for (const char c : absolute) {
    uri += c == '%'   ? std::string("%25")
           : c == '?' ? std::string("%3f")
           : c == '#' ? std::string("%23")
                      : std::string(1, c);
  }
  uri += "?immutable=1";
  sqlite3* raw = nullptr;
  const auto opened = sqlite3_open_v2(
      uri.c_str(), &raw,
      SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
  std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> db(raw,
                                                           sqlite3_close_v2);
  if (opened != SQLITE_OK) {
    fail(ErrorCode::FileOpen, "Cannot open cookie database {}: {}", path,
         sqlite3_errstr(opened));
  }
  // Firefox stores expiry in seconds (newer versions: milliseconds); Chromium
  // stores microseconds since 1601 with zero for session cookies.
  constexpr const char* queries[] = {
      "SELECT host, path, isSecure, isHttpOnly, "
      "CASE WHEN expiry > 100000000000 THEN expiry / 1000 ELSE expiry END, "
      "name, value FROM moz_cookies",
      "SELECT host_key, path, is_secure, is_httponly, "
      "CASE WHEN expires_utc <= 0 THEN 0 "
      "ELSE max(1, expires_utc / 1000000 - 11644473600) END, "
      "name, value FROM cookies"};
  for (const auto* query : queries) {
    sqlite3_stmt* raw_statement = nullptr;
    if (sqlite3_prepare_v2(db.get(), query, -1, &raw_statement, nullptr) !=
        SQLITE_OK) {
      continue;
    }
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(
        raw_statement, sqlite3_finalize);
    std::vector<std::string> lines;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
      auto column = [&](int index) {
        const auto* value = sqlite3_column_text(statement.get(), index);
        return std::string(value ? reinterpret_cast<const char*>(value) : "");
      };
      const auto domain = column(0);
      const auto name = column(5);
      const auto value = column(6);
      // Encrypted Chromium values are unavailable without the browser's key.
      if (domain.empty() || name.empty() || value.empty()) {
        continue;
      }
      auto path = column(1);
      lines.push_back((sqlite3_column_int(statement.get(), 3) ? "#HttpOnly_" : "") +
                      domain + (domain.front() == '.' ? "\tTRUE\t" : "\tFALSE\t") +
                      (path.empty() ? "/" : path) +
                      (sqlite3_column_int(statement.get(), 2) ? "\tTRUE\t"
                                                              : "\tFALSE\t") +
                      std::to_string(sqlite3_column_int64(statement.get(), 4)) +
                      '\t' + name + '\t' + value);
    }
    return lines;
  }
  fail(ErrorCode::FileOpen,
       "Unsupported cookie database {}: no Firefox or Chromium cookie table",
       path);
}

} // namespace aria2::http
