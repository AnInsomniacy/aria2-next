/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "options/CommandLine.h"
#include "options/Options.h"

#include <doctest/doctest.h>

using namespace aria2;

TEST_CASE("task options fall back to the global layer and the catalogue")
{
  auto global = std::make_shared<Options>();
  global->set("max-tries", "7");
  Options task(global);
  CHECK(task.integer("max-tries") == 7);
  CHECK(task.get("dir") == ".");
  task.set("max-tries", "2");
  CHECK(task.integer("max-tries") == 2);
  CHECK(global->integer("max-tries") == 7);
  CHECK(task.hasOwn("max-tries"));
  CHECK_FALSE(task.hasOwn("dir"));
}

TEST_CASE("values are validated and canonicalised")
{
  Options options;
  options.set("max-download-limit", "1M");
  CHECK(options.get("max-download-limit") == "1048576");
  CHECK_THROWS_AS(options.set("max-tries", "-1"), Error);
  CHECK_THROWS_AS(options.set("check-certificate", "yes"), Error);
  CHECK_THROWS_AS(options.set("media", "flv"), Error);
  CHECK_THROWS_AS(options.set("no-such-option", "1"), Error);
  options.set("listen-port", "6881-6999");
  CHECK(options.get("listen-port") == "6881");
}

TEST_CASE("retired aria2 options are mapped or ignored")
{
  Options options;
  options.set("stream-max-connections", "4");
  CHECK(options.integer("split") == 4);
  CHECK(options.integer("max-connection-per-server") == 4);
  options.set("bt-require-crypto", "true");
  CHECK(options.get("bt-encryption") == "required");
  options.set("async-dns", "false"); // ignored with a warning
  CHECK(options.own().count("async-dns") == 0);
}

TEST_CASE("list options accumulate")
{
  Options options;
  options.add("header", "A: 1");
  options.add("header", "B: 2");
  CHECK(options.get("header") == "A: 1\nB: 2");
}

TEST_CASE("a snapshot is detached from later changes, secrets included")
{
  auto global = std::make_shared<Options>();
  global->set("http-passwd", "secret");
  auto task = std::make_shared<Options>(global);
  task->set("out", "a.mp4");
  const auto frozen = task->snapshot();
  global->set("http-passwd", "changed");
  task->set("out", "b.mp4");
  CHECK(frozen->parent() == nullptr);
  CHECK(frozen->get("http-passwd") == "secret");
  CHECK(frozen->get("out") == "a.mp4");
  CHECK(task->visible().count("http-passwd") == 0);
}

TEST_CASE("the command line follows getopt_long conventions")
{
  const auto line =
      parseCommandLine({"-x16", "--dir", "/tmp", "--continue", "-s", "4",
                        "http://a/b", "--", "--literal"});
  CHECK(line.options == std::vector<std::pair<std::string, std::string>>{
                            {"max-connection-per-server", "16"},
                            {"dir", "/tmp"},
                            {"continue", "true"},
                            {"split", "4"}});
  CHECK(line.uris == std::vector<std::string>{"http://a/b", "--literal"});
  CHECK_THROWS_AS(parseCommandLine({"--max-tries"}), Error);
}

TEST_CASE("config files report the failing line")
{
  const auto pairs =
      parseConfigFile("# comment\ndir=/data\n\nsplit = 4\n", "test.conf");
  CHECK(pairs.size() == 2);
  CHECK(pairs[0] == std::pair<std::string, std::string>{"dir", "/data"});
  try {
    parseConfigFile("dir=/data\nbroken\n", "test.conf");
    FAIL("expected an error");
  }
  catch (const Error& error) {
    CHECK(std::string(error.what()).find(":2") != std::string::npos);
  }
}

TEST_CASE("config files read a flag named alone as true, as aria2 does")
{
  const auto pairs =
      parseConfigFile("continue\nbt-save-metadata\nsplit=4\n", "test.conf");
  CHECK(pairs == std::vector<std::pair<std::string, std::string>>{
                     {"continue", "true"},
                     {"bt-save-metadata", "true"},
                     {"split", "4"}});
  CHECK_THROWS_AS(parseConfigFile("split\n", "test.conf"), Error);
}
