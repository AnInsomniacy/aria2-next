/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "app/Console.h"

#include <doctest/doctest.h>

using namespace aria2;

namespace {
TaskFile file(std::string path, int64_t length, int64_t completed)
{
  TaskFile entry;
  entry.path = std::move(path);
  entry.length = length;
  entry.completed = completed;
  return entry;
}
} // namespace

TEST_CASE("results print in the layout frontends parse")
{
  // Scoop reads "(OK):" and "Download Results:" lines from aria2's output.
  const std::vector<ResultRow> rows{
      {0x2089b05ecca3d829,
       TaskState::Complete,
       1153434,
       {file("C:/scoop/cache/app.zip", 10, 10)}},
      {0x1a2b3c4d5e6f7081,
       TaskState::Error,
       -1,
       {file("a.bin", 0, 0), file("b.bin", 0, 0)}},
  };
  CHECK(formatResults(rows, false) ==
        "\nDownload Results:\n"
        "gid   |stat|avg speed  |path/URI\n"
        "======+====+===========+" +
            std::string(55, '=') +
            "\n"
            "2089b0|OK  |   1.1MiB/s|C:/scoop/cache/app.zip\n"
            "1a2b3c|ERR |        n/a|a.bin (1more)\n"
            "\nStatus Legend:\n"
            "(OK):download completed.\n"
            "(ERR):error occurred.\n");
}

TEST_CASE("full results list each file with its progress")
{
  const std::vector<ResultRow> rows{
      {0x2089b05ecca3d829,
       TaskState::Active,
       2048,
       {file("a.bin", 200, 50), file("b.bin", 0, 0)}},
  };
  const auto out = formatResults(rows, true);
  CHECK(out.find("2089b0|INPR|   2.0KiB/s| 25|a.bin\n") != std::string::npos);
  CHECK(out.find("      |    |           |  -|b.bin\n") != std::string::npos);
  CHECK(out.find("(INPR):download in-progress.\n") != std::string::npos);
}
