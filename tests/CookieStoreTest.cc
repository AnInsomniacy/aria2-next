#include "transport/CookieStore.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "File.h"
#include "a2doctest.h"

namespace aria2 {
namespace {
void createStore(const std::string& path, const char* schema)
{
  File(path).remove();
  sqlite3* db = nullptr;
  REQUIRE_EQ(SQLITE_OK, sqlite3_open(path.c_str(), &db));
  REQUIRE_EQ(SQLITE_OK, sqlite3_exec(db, schema, nullptr, nullptr, nullptr));
  sqlite3_close(db);
}

bool contains(const std::vector<std::string>& lines, const std::string& line)
{
  return std::find(lines.begin(), lines.end(), line) != lines.end();
}
} // namespace

TEST_CASE("Firefox cookie stores load as Netscape cookie lines")
{
  File(A2_TEST_OUT_DIR).mkdirs();
  const std::string path = A2_TEST_OUT_DIR "/firefox-cookies.sqlite";
  createStore(path,
              "CREATE TABLE moz_cookies (host TEXT, path TEXT, isSecure INT, "
              "isHttpOnly INT, expiry INT, name TEXT, value TEXT);"
              "INSERT INTO moz_cookies VALUES "
              "('.example.org', '/', 1, 0, 1900000000, 'sid', 'a1'),"
              "('example.org', '/dl', 0, 1, 1900000000000, 'tok', 'b2'),"
              "('example.org', '/', 0, 0, 0, 'empty', '');");
  CHECK(http::isSqliteCookieFile(path));
  const auto cookies = http::sqliteCookies(path);
  REQUIRE(cookies.lines);
  CHECK_EQ(2, cookies.lines->size());
  CHECK(contains(*cookies.lines,
                 ".example.org\tTRUE\t/\tTRUE\t1900000000\tsid\ta1"));
  // Recent Firefox versions store milliseconds.
  CHECK(contains(*cookies.lines,
                 "#HttpOnly_example.org\tFALSE\t/dl\tFALSE\t1900000000\ttok\tb2"));
}

TEST_CASE("Chromium cookie stores convert expiry and skip encrypted values")
{
  File(A2_TEST_OUT_DIR).mkdirs();
  const std::string path = A2_TEST_OUT_DIR "/chromium-cookies.sqlite";
  createStore(path,
              "CREATE TABLE cookies (host_key TEXT, path TEXT, is_secure INT, "
              "is_httponly INT, expires_utc INT, name TEXT, value TEXT, "
              "encrypted_value BLOB);"
              "INSERT INTO cookies VALUES "
              "('.example.org', '/', 0, 0, 13544473600000000, 'a', 'v', x''),"
              "('.example.org', '/', 0, 0, 0, 'session', 's', x''),"
              "('.example.org', '/', 0, 0, 0, 'secret', '', x'01');");
  const auto cookies = http::sqliteCookies(path);
  REQUIRE(cookies.lines);
  CHECK_EQ(2, cookies.lines->size());
  CHECK(contains(*cookies.lines,
                 ".example.org\tTRUE\t/\tFALSE\t1900000000\ta\tv"));
  CHECK(contains(*cookies.lines, ".example.org\tTRUE\t/\tFALSE\t0\tsession\ts"));
}

TEST_CASE("Text cookie files are left to libcurl")
{
  File(A2_TEST_OUT_DIR).mkdirs();
  const std::string path = A2_TEST_OUT_DIR "/cookies.txt";
  std::ofstream(path) << ".example.org\tTRUE\t/\tFALSE\t0\ta\tv\n";
  CHECK_FALSE(http::isSqliteCookieFile(path));
}

} // namespace aria2
