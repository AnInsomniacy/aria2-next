/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "base/Path.h"
#include "base/Text.h"
#include "disk/FileName.h"
#include "options/Options.h"

#include <doctest/doctest.h>

using namespace aria2;

TEST_CASE("a derived file name never leaves the download directory")
{
  CHECK(sanitizeFileName("../../etc/passwd", FileNamePolicy{}) ==
        ".._.._etc_passwd");
  CHECK(sanitizeFileName("a/b\\c", FileNamePolicy{}) == "a_b_c");
  CHECK(sanitizeFileName("..", FileNamePolicy{}).empty());
  CHECK(sanitizeFileName(".", FileNamePolicy{}).empty());
  CHECK(sanitizeFileName("name.", FileNamePolicy{}) == "name");
  CHECK(sanitizeFileName(" name ", FileNamePolicy{}) == "name");
  // Reserved device names cannot name a file on Windows.
  CHECK(sanitizeFileName("CON", FileNamePolicy{}) == "_CON");
  CHECK(sanitizeFileName("lpt1.txt", FileNamePolicy{}) == "_lpt1.txt");
}

TEST_CASE("invalid characters follow the name policy")
{
  CHECK(sanitizeFileName("a?b*c", FileNamePolicy{}) == "a?b*c");

  auto remove = FileNamePolicy{};
  remove.invalidCharacters = InvalidFileNameCharacters::Delete;
  CHECK(sanitizeFileName("a?b*c", remove) == "abc");
  CHECK(sanitizeFileName("a\tb", remove) == "ab");

  auto replace = FileNamePolicy{};
  replace.invalidCharacters = InvalidFileNameCharacters::Replace;
  CHECK(sanitizeFileName("a?b*c", replace) == "a_b_c");
  CHECK(sanitizeFileName("a\tb", replace) == "a_b");
}

TEST_CASE("head-tail shortening keeps the first and last characters")
{
  auto policy = FileNamePolicy{};
  policy.truncation = FileNameTruncation::HeadTail;
  policy.maxBytes = 100;
  const auto name = std::string(1000, 'a') + ".ext";
  CHECK(sanitizeFileName(name, policy) ==
        std::string(25, 'a') + "_" + "aaaaaa.ext");

  // A limit shorter than both ends shrinks the head, then the tail, and
  // keeps the extension.
  policy.maxBytes = 20;
  const auto tight = sanitizeFileName(name, policy);
  CHECK(tight.size() == 20);
  CHECK(tight.ends_with(".ext"));
}

TEST_CASE("extension shortening keeps the beginning and the extension")
{
  auto policy = FileNamePolicy{};
  policy.maxBytes = 240;
  const auto name = std::string(1000, 'a') + ".ext";
  CHECK(sanitizeFileName(name, policy) == std::string(236, 'a') + ".ext");

  // A limit too small for the extension cuts the name instead.
  policy.maxBytes = 3;
  CHECK(sanitizeFileName(name, policy) == "aaa");
}

TEST_CASE("truncation keeps UTF-8 characters whole")
{
  auto policy = FileNamePolicy{};
  policy.maxBytes = 30;
  std::string name;
  for (int i = 0; i < 40; ++i) {
    name += "\xe4\xb8\xad"; // U+4E2D, three bytes each
  }
  const auto shortened = sanitizeFileName(name, policy);
  CHECK(text::isUtf8(shortened));
  CHECK(shortened.size() <= 30);
  CHECK(shortened.ends_with("\xe4\xb8\xad"));
}

TEST_CASE("file name options default to the earlier behaviour")
{
  Options options;
  CHECK(options.get("filename-invalid-chars") == "replace");
  CHECK(options.integer("filename-max-length") == 240);
  CHECK(options.get("filename-truncate-style") == "extension");
  CHECK(options.integer("filename-truncate-head") == 25);
  CHECK(options.integer("filename-truncate-tail") == 10);

  const auto unchanged = fileNamePolicy(options);
  CHECK(unchanged.invalidCharacters == InvalidFileNameCharacters::Replace);
  CHECK(unchanged.truncation == FileNameTruncation::Extension);
  CHECK(unchanged.maxBytes == 240);
  CHECK(sanitizeFileName("a?b*c", unchanged) == "a_b_c");

  options.set("filename-invalid-chars", "none");
  options.set("filename-max-length", "0");
  options.set("filename-truncate-style", "head-tail");
  const auto changed = fileNamePolicy(options);
  CHECK(changed.invalidCharacters == InvalidFileNameCharacters::Keep);
  CHECK(changed.truncation == FileNameTruncation::HeadTail);
  CHECK(changed.maxBytes == 0);
  CHECK_THROWS_AS(options.set("filename-invalid-chars", "strip"), Error);
}
