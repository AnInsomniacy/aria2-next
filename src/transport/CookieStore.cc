/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "transport/CookieStore.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>

#include <sqlite3.h>

#include "DlAbortEx.h"
#include "File.h"
#include "error_code.h"
#include "fmt.h"
#include <filesystem>

namespace aria2::http {
namespace {
struct Cookie {
  std::string domain;
  std::string path;
  bool secure = false;
  bool httpOnly = false;
  int64_t expiry = 0;
  std::string name;
  std::string value;
};


// Chromium stores microseconds since 1601-01-01; zero marks a session cookie.
int64_t chromiumExpiry(int64_t value)
{
  constexpr int64_t epochOffset = 11644473600;
  return value <= 0 ? 0 : std::max<int64_t>(1, value / 1000000 - epochOffset);
}

// Recent Firefox versions store milliseconds; older ones store seconds.
int64_t firefoxExpiry(int64_t value)
{
  return value > 100000000000 ? value / 1000 : value;
}

std::string text(sqlite3_stmt* statement, int column)
{
  const auto* value = sqlite3_column_text(statement, column);
  return value ? reinterpret_cast<const char*>(value) : "";
}

bool hasTable(sqlite3* db, const char* name)
{
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(db,
                         "SELECT 1 FROM sqlite_master WHERE type='table' AND "
                         "name=?",
                         -1, &statement, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(statement, 1, name, -1, SQLITE_STATIC);
  const bool found = sqlite3_step(statement) == SQLITE_ROW;
  sqlite3_finalize(statement);
  return found;
}

std::vector<Cookie> readStore(const std::string& path)
{
  sqlite3* raw = nullptr;
  const auto opened =
      sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
  std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> db(raw,
                                                           sqlite3_close_v2);
  if (opened != SQLITE_OK) {
    throw DL_ABORT_EX2(fmt("Cannot open cookie database %s: %s", path.c_str(),
                           sqlite3_errstr(opened)),
                       error_code::FILE_OPEN_ERROR);
  }
  sqlite3_busy_timeout(db.get(), 1000);
  const bool firefox = hasTable(db.get(), "moz_cookies");
  if (!firefox && !hasTable(db.get(), "cookies")) {
    throw DL_ABORT_EX2(
        fmt("Unsupported cookie database %s: no Firefox or Chromium cookie "
            "table",
            path.c_str()),
        error_code::FILE_OPEN_ERROR);
  }
  const char* query =
      firefox ? "SELECT host, path, isSecure, isHttpOnly, expiry, name, value "
                "FROM moz_cookies"
              : "SELECT host_key, path, is_secure, is_httponly, expires_utc, "
                "name, value FROM cookies";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(db.get(), query, -1, &raw_statement, nullptr) !=
      SQLITE_OK) {
    throw DL_ABORT_EX2(fmt("Cannot read cookie database %s: %s", path.c_str(),
                           sqlite3_errmsg(db.get())),
                       error_code::FILE_OPEN_ERROR);
  }
  std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(
      raw_statement, sqlite3_finalize);
  std::vector<Cookie> cookies;
  int step;
  while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
    Cookie cookie;
    cookie.domain = text(statement.get(), 0);
    cookie.path = text(statement.get(), 1);
    cookie.secure = sqlite3_column_int(statement.get(), 2) != 0;
    cookie.httpOnly = sqlite3_column_int(statement.get(), 3) != 0;
    const auto expiry = sqlite3_column_int64(statement.get(), 4);
    cookie.expiry = firefox ? firefoxExpiry(expiry) : chromiumExpiry(expiry);
    cookie.name = text(statement.get(), 5);
    cookie.value = text(statement.get(), 6);
    // Encrypted Chromium values are unavailable without the browser's key.
    if (cookie.domain.empty() || cookie.name.empty() ||
        cookie.value.empty()) {
      continue;
    }
    if (cookie.path.empty()) {
      cookie.path = "/";
    }
    cookies.push_back(std::move(cookie));
  }
  if (step != SQLITE_DONE) {
    throw DL_ABORT_EX2(fmt("Cannot read cookie database %s: %s", path.c_str(),
                           sqlite3_errmsg(db.get())),
                       error_code::FILE_OPEN_ERROR);
  }
  return cookies;
}

std::string netscapeLine(const Cookie& cookie)
{
  return (cookie.httpOnly ? "#HttpOnly_" : "") + cookie.domain +
         (cookie.domain.front() == '.' ? "\tTRUE\t" : "\tFALSE\t") +
         cookie.path + (cookie.secure ? "\tTRUE\t" : "\tFALSE\t") +
         std::to_string(cookie.expiry) + '\t' + cookie.name + '\t' +
         cookie.value;
}
} // namespace

bool isSqliteCookieFile(const std::string& path)
{
  static constexpr char header[] = "SQLite format 3";
  std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
  char buffer[sizeof(header)] = {};
  return input.read(buffer, sizeof(buffer)) &&
         std::memcmp(buffer, header, sizeof(header)) == 0;
}

SqliteCookies sqliteCookies(const std::string& path)
{
  static std::mutex mutex;
  static std::map<std::string, SqliteCookies> cache;
  File file(path);
  const auto revision =
      std::to_string(file.size()) + ':' +
      std::to_string(file.getModifiedTime().getTimeFromEpoch());
  std::lock_guard<std::mutex> lock(mutex);
  auto& entry = cache[path];
  if (entry.revision != revision) {
    auto lines = std::make_shared<std::vector<std::string>>();
    for (const auto& cookie : readStore(path)) {
      lines->push_back(netscapeLine(cookie));
    }
    entry.lines = std::move(lines);
    entry.revision = revision;
  }
  return entry;
}

} // namespace aria2::http
