/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "disk/File.h"
#include "http/Request.h"
#include "http/SegmentPlanner.h"

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
  CHECK(http::parseContentRange("bytes=0-9/10")->last == 9);
  // A 206 without Content-Range: the open tail, or exactly the bounded span.
  const auto tail = http::impliedContentRange(0, http::unbounded, 1000);
  REQUIRE(tail);
  CHECK(tail->last == 999);
  CHECK(tail->total == 1000);
  const auto span = http::impliedContentRange(100, 200, 100);
  REQUIRE(span);
  CHECK(span->first == 100);
  CHECK(span->last == 199);
  CHECK(span->total == -1);
  CHECK_FALSE(http::impliedContentRange(100, 200, 1000));
  CHECK_FALSE(http::impliedContentRange(0, http::unbounded, -1));
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

TEST_CASE("Thunder links carry the URI they wrap")
{
  const auto wrap = [](std::string_view inner) {
    return "thunder://" + text::base64Encode("AA" + std::string(inner) + "ZZ");
  };
  CHECK(uri::unwrapThunder(wrap("http://host/a.zip")) == "http://host/a.zip");
  CHECK(uri::unwrapThunder("THUNDER://" +
                           wrap("ed2k://|file|a|1|0|/").substr(10)) ==
        "ed2k://|file|a|1|0|/");
  // Pages append a slash, drop or escape the padding.
  CHECK(uri::unwrapThunder("thunder://"
                           "QUFodHRwczovL3NlY3VyZS1hcHBsZG5sZC5hcHBsZS5jb20vaVB"
                           "ob25lLzA2MS03NDgxLjIwMTAwMjAyLjRvcm90L2lQaG9uZTEsMV"
                           "8zLjEuM183RTE4X1Jlc3RvcmUuaXBzd1pa/") ==
        "https://secure-appldnld.apple.com/iPhone/061-7481.20100202.4orot/"
        "iPhone1,1_3.1.3_7E18_Restore.ipsw");
  CHECK(uri::unwrapThunder("thunder://QUFodHRwOi8vaC9hWlo%3D") == "http://h/a");
  CHECK(uri::unwrapThunder("thunder://QUFodHRwOi8vaC9hWlo") == "http://h/a");
  // A GBK name is sent as it was written.
  CHECK(uri::unwrapThunder(wrap("http://h/\xCD\xB7 1.png")) ==
        "http://h/%CD%B7%201.png");
  CHECK_FALSE(uri::unwrapThunder("thunder://"));
  CHECK_FALSE(uri::unwrapThunder("thunder://not*base64"));
  CHECK_FALSE(uri::unwrapThunder("thunder://" +
                                 text::base64Encode("http://host/a.zip")));
  CHECK_FALSE(uri::unwrapThunder(wrap("")));
  CHECK_FALSE(uri::unwrapThunder("http://host/a.zip"));
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
  CHECK(numberedPath(base, Numbering::Dot, [&](const std::string& path) {
          return samePath(path, second);
        }) == toUtf8(directory / "video.3.mp4"));
  // The style browsers and file managers use, chosen by the desktop app.
  CHECK(numberedPath(base, Numbering::Parentheses) ==
        toUtf8(directory / "video (1).mp4"));
  CHECK(numbering("parentheses") == Numbering::Parentheses);
  CHECK(numbering("dot") == Numbering::Dot);
  std::filesystem::remove_all(directory);
}

TEST_CASE("moving files is all or nothing and tidies the old directory")
{
  const auto root =
      std::filesystem::temp_directory_path() / "aria2-engine-test-move";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "old" / "album");
  std::filesystem::create_directories(root / "new");
  std::ofstream(root / "old" / "album" / "a.flac") << "a";
  std::ofstream(root / "old" / "album" / "b.flac") << "b";
  std::ofstream(root / "new" / "b.flac") << "taken";
  const auto at = [&](const std::filesystem::path& path) {
    return toUtf8(root / path);
  };
  // A target that exists fails the move before anything moves.
  CHECK_FALSE(moveFiles({{at("old/album/a.flac"), at("new/a.flac")},
                         {at("old/album/b.flac"), at("new/b.flac")}},
                        at("old"))
                  .empty());
  CHECK(std::filesystem::exists(root / "old" / "album" / "a.flac"));
  CHECK_FALSE(std::filesystem::exists(root / "new" / "a.flac"));
  CHECK(moveFiles({{at("old/album/a.flac"), at("new/album/a.flac")},
                   {at("old/album/b.flac"), at("new/album/b.flac")}},
                  at("old"))
            .empty());
  CHECK(std::filesystem::exists(root / "new" / "album" / "b.flac"));
  // The emptied album folder goes; the download directory itself stays.
  CHECK_FALSE(std::filesystem::exists(root / "old" / "album"));
  CHECK(std::filesystem::exists(root / "old"));
  std::filesystem::remove_all(root);
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
