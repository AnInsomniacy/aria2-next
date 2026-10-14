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
  CHECK(http::unsatisfiedRangeLength("bytes */0") == 0);
  CHECK(http::unsatisfiedRangeLength("bytes */1000") == 1000);
  CHECK_FALSE(http::unsatisfiedRangeLength("bytes 0-9/10"));
  CHECK_FALSE(http::unsatisfiedRangeLength("bytes */*"));
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
  CHECK(text::toSize("1.5K") == 1536);
  CHECK_FALSE(text::toSize("1.5.K"));
}

TEST_CASE("decimal numbers")
{
  CHECK(text::toDouble("0.5") == 0.5);
  CHECK(text::toDouble("1e3") == 1000);
  CHECK(text::toDouble("-2") == -2);
  CHECK_FALSE(text::toDouble(""));
  CHECK_FALSE(text::toDouble("+1"));
  CHECK_FALSE(text::toDouble(" 1"));
  CHECK_FALSE(text::toDouble("1x"));
  CHECK_FALSE(text::toDouble("1,5"));
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

TEST_CASE("files grow sparse or reserved, keeping their content")
{
  CHECK(parseAllocation("none") == Allocation::Sparse);
  CHECK(parseAllocation("trunc") == Allocation::Sparse);
  CHECK(parseAllocation("prealloc") == Allocation::Reserve);
  CHECK(parseAllocation("falloc") == Allocation::Reserve);

  const auto directory =
      std::filesystem::temp_directory_path() / "aria2-engine-test-extend";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  // Not a multiple of the zero-filling chunk, and larger than one.
  const int64_t length = (int64_t{9} << 20) + 5;
  for (const auto allocation : {Allocation::Sparse, Allocation::Reserve}) {
    File file;
    REQUIRE_FALSE(file.open(toUtf8(directory / "grown.bin"), true));
    REQUIRE_FALSE(file.write(0, "abc", 3));
    REQUIRE_FALSE(file.extend(length, allocation));
    CHECK(file.size() == length);
    std::string head(3, ' ');
    size_t read = 0;
    REQUIRE_FALSE(file.read(0, head.data(), head.size(), read));
    CHECK(head == "abc");
    std::string tail(5, 'x');
    REQUIRE_FALSE(file.read(length - 5, tail.data(), tail.size(), read));
    CHECK(read == 5);
    CHECK(tail == std::string(5, '\0'));
    // Never shrinks or rewrites what a resumed download holds.
    REQUIRE_FALSE(file.extend(length - 1, allocation));
    CHECK(file.size() == length);
  }
  File stopped;
  REQUIRE_FALSE(stopped.open(toUtf8(directory / "stopped.bin"), true));
  stopped.stopExtending();
  REQUIRE_FALSE(stopped.extend(length, Allocation::Reserve));
#ifdef _WIN32
  // Stopped before writing any zeros; the next opening fills them.
  CHECK(stopped.size() == 0);
#else
  CHECK(stopped.size() == length);
#endif
  stopped.close();
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

TEST_CASE("text from the system is repaired into UTF-8")
{
  // GBK for the Chinese word for "error", as an ANSI Windows message has it.
  const std::string ansi = "open failed: \xB4\xED\xCE\xF3";
  CHECK_FALSE(text::isUtf8(ansi));
  const auto repaired = text::repairUtf8(ansi);
  CHECK(text::isUtf8(repaired));
  CHECK(repaired.rfind("open failed: ", 0) == 0);
  CHECK(text::repairUtf8("caf\xC3\xA9 \xF0\x9F\x8E\xAC") ==
        "caf\xC3\xA9 \xF0\x9F\x8E\xAC");
  CHECK(text::repairUtf8("\xED\xA0\x80") == // a UTF-16 surrogate
        "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
}
