/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "disk/File.h"
#include "http/Request.h"

#include <doctest/doctest.h>

#include <fstream>

using namespace aria2;

TEST_CASE("content ranges")
{
  const auto range = http::parseContentRange("bytes 100-199/1000");
  REQUIRE(range);
  CHECK(range->first == 100);
  CHECK(range->last == 199);
  CHECK(range->total == 1000);
  CHECK(http::parseContentRange("bytes 0-9/*")->total == -1);
  CHECK_FALSE(http::parseContentRange("bytes */1000"));
  CHECK_FALSE(http::parseContentRange("bytes 9-0/10"));
  CHECK_FALSE(http::parseContentRange("items 0-9/10"));
}

TEST_CASE("origins and redaction")
{
  CHECK(uri::sameOrigin("http://Example.com/a", "http://example.com:80/b"));
  CHECK_FALSE(uri::sameOrigin("http://example.com/", "https://example.com/"));
  CHECK_FALSE(
      uri::sameOrigin("http://example.com/", "http://example.com:8080/"));
  CHECK(uri::redact("https://user:pw@host/path?token=1") ==
        "https://host/path");
}

TEST_CASE("file names from URLs")
{
  CHECK(uri::fileName("http://host/dir/na%20me.iso?x=1") == "na me.iso");
  CHECK(uri::fileName("http://host/get?filename=report%20final.pdf") ==
        "report final.pdf");
  CHECK(uri::fileName("http://host/a%2F..%2Fb") == "a_.._b");
  CHECK(uri::fileName("http://host/").empty());
}

TEST_CASE("sizes")
{
  CHECK(text::toSize("1K") == 1024);
  CHECK(text::toSize(" 2M ") == 2 << 20);
  CHECK_FALSE(text::toSize("-1"));
  CHECK_FALSE(text::toSize("K"));
}

TEST_CASE("numbered names skip existing files")
{
  const auto directory =
      std::filesystem::temp_directory_path() / "aria2-engine-test-numbered";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  const auto base = toUtf8(directory / "video.mp4");
  std::ofstream(directory / "video.mp4") << "x";
  std::ofstream(directory / "video.1.mp4") << "x";
  CHECK(numberedPath(base) == toUtf8(directory / "video.2.mp4"));
  const auto second = toUtf8(directory / "video.2.mp4");
  CHECK(numberedPath(base, [&](const std::string& path) {
          return samePath(path, second);
        }) == toUtf8(directory / "video.3.mp4"));
  std::filesystem::remove_all(directory);
}

TEST_CASE("same paths")
{
  CHECK(samePath("/downloads/a/../file.zip", "/downloads/file.zip"));
  CHECK_FALSE(samePath("/downloads/file.zip", "/downloads/file.zip.1"));
#ifdef _WIN32
  CHECK(samePath("C:/Downloads/File.ZIP", "c:/downloads/file.zip"));
#endif
}
