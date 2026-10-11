/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "http/Metalink.h"

#include <doctest/doctest.h>

using namespace aria2;

namespace {
std::string document(std::string_view files)
{
  return R"(<?xml version="1.0" encoding="UTF-8"?>
<metalink xmlns="urn:ietf:params:xml:ns:metalink">)" +
         std::string(files) + "</metalink>";
}
} // namespace

TEST_CASE("metalink files keep mirror priority and the strongest hash")
{
  const auto files = http::parseMetalink(document(R"(
  <file name="dir/a.bin">
    <size>1024</size>
    <hash type="md5">AA</hash>
    <hash type="sha-256">BB</hash>
    <pieces length="512" type="sha-1"><hash>cc</hash><hash>dd</hash></pieces>
    <url priority="2">https://two.example/a.bin</url>
    <url>http://unranked.example/a.bin</url>
    <url priority="1">http://one.example/a.bin</url>
    <url priority="1">ftp://legacy.example/a.bin</url>
  </file>
  <file name="b.bin"><url>http://example/b.bin</url></file>
  <file name="torrent-only"><metaurl mediatype="torrent">http://x/t</metaurl></file>)"));
  REQUIRE(files.size() == 2);
  CHECK(files[0].name == "dir/a.bin");
  CHECK(files[0].size == 1024);
  CHECK(files[0].checksum == "sha-256=bb");
  CHECK(files[0].urls ==
        std::vector<std::string>{"http://one.example/a.bin",
                                 "https://two.example/a.bin",
                                 "http://unranked.example/a.bin"});
  CHECK(files[1].checksum.empty());
}

TEST_CASE("unsafe and unsupported metalinks are rejected")
{
  CHECK_THROWS_AS(
      http::parseMetalink(
          document(R"(<file name="../escape"><url>http://e/x</url></file>)")),
      Error);
  CHECK_THROWS_AS(
      http::parseMetalink(
          R"(<metalink version="3.0" xmlns="http://www.metalinker.org/"/>)"),
      Error);
  CHECK_THROWS_AS(http::parseMetalink(R"(<?xml version="1.0"?>
<!DOCTYPE metalink [<!ENTITY a "aaaaaaaaaa">]>
<metalink xmlns="urn:ietf:params:xml:ns:metalink"><file name="&a;"><url>http://e/x</url></file></metalink>)"),
                  Error);
  CHECK_THROWS_AS(http::parseMetalink("<metalink"), Error);
  CHECK_THROWS_AS(http::parseMetalink(document("")), Error);
}

TEST_CASE("metalink documents are recognised by name or type")
{
  CHECK(http::isMetalink("/tmp/x.meta4", ""));
  CHECK(http::isMetalink("/tmp/x.METALINK", ""));
  CHECK(http::isMetalink("/tmp/download",
                         "application/metalink4+xml; charset=utf-8"));
  CHECK_FALSE(http::isMetalink("/tmp/x.xml", "text/xml"));
}
