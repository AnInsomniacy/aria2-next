/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Path.h"
#include "state/Sqlite.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace aria2;

TEST_CASE("connections opening one new file wait for each other's writes")
{
  const auto directory =
      std::filesystem::temp_directory_path() / "aria2-next-sqlite-test";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  // Like media tasks starting together: every connection may create the file.
  const auto path = toUtf8(directory / "state.db");

  std::mutex mutex;
  std::vector<std::string> errors;
  std::vector<std::thread> writers;
  for (int i = 0; i < 4; ++i) {
    writers.emplace_back([&] {
      try {
        sqlite::Database db(path);
        db.execute("CREATE TABLE IF NOT EXISTS counter(value INTEGER)");
        for (int round = 0; round < 50; ++round) {
          sqlite::Transaction transaction(db);
          int64_t count = 0;
          {
            sqlite::Statement query(db, "SELECT count(*) FROM counter");
            if (query.step()) {
              count = query.integer(0);
            }
          }
          sqlite::Statement(db, "INSERT INTO counter VALUES (?)")
              .bind(1, count)
              .run();
          transaction.commit();
        }
      }
      catch (const std::exception& error) {
        std::lock_guard lock(mutex);
        errors.emplace_back(error.what());
      }
    });
  }
  for (auto& writer : writers) {
    writer.join();
  }

  CHECK(errors == std::vector<std::string>{});
  {
    sqlite::Database db(path);
    sqlite::Statement distinct(db, "SELECT count(DISTINCT value) FROM counter");
    REQUIRE(distinct.step());
    CHECK(distinct.integer(0) == 200);
  }
  std::filesystem::remove_all(directory);
}
