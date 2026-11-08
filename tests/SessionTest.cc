/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "state/Session.h"

#include <doctest/doctest.h>

using namespace aria2;

TEST_CASE("input files keep option values verbatim")
{
  const auto entries = parseSession("# comment\r\n"
                                    "http://a/x\thttp://b/x\r\n"
                                    " dir=C:/Downloads\r\n"
                                    "\theader=X-Token:  padded \n"
                                    "\n"
                                    "C:/torrents/linux.torrent\n"
                                    " select-file=1-3\n"
                                    " not an option\n");
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].uris ==
        std::vector<std::string>{"http://a/x", "http://b/x"});
  CHECK(entries[0].options ==
        std::vector<std::pair<std::string, std::string>>{
            {"dir", "C:/Downloads"}, {"header", "X-Token:  padded"}});
  CHECK(entries[1].uris ==
        std::vector<std::string>{"C:/torrents/linux.torrent"});
  CHECK(entries[1].options.size() == 1);
}

TEST_CASE("input files may start with a UTF-8 byte order mark")
{
  const auto entries = parseSession("\xEF\xBB\xBFhttp://a/x\r\n out=x.bin\r\n");
  REQUIRE(entries.size() == 1);
  CHECK(entries[0].uris == std::vector<std::string>{"http://a/x"});
}

TEST_CASE("a saved failure restores the task as failed without leaking options")
{
  auto entries = parseSession("http://a/x\n"
                              " gid=2089b05ecca3d829\n"
                              " error-code=22\n"
                              " error-message=HTTP 403 Forbidden\n"
                              " dir=/downloads\n"
                              "http://a/y\n"
                              " error-code=999\n"
                              "http://a/z\n"
                              " dir=/downloads\n");
  REQUIRE(entries.size() == 3);

  const auto failed = takeFailure(entries[0]);
  REQUIRE(failed);
  CHECK(failed->code == ErrorCode::HttpProtocol);
  CHECK(failed->message == "HTTP 403 Forbidden");
  CHECK(entries[0].options ==
        std::vector<std::pair<std::string, std::string>>{
            {"gid", "2089b05ecca3d829"}, {"dir", "/downloads"}});

  const auto newer = takeFailure(entries[1]);
  REQUIRE(newer);
  CHECK(newer->code == ErrorCode::Unknown);
  CHECK(entries[1].options.empty());

  CHECK_FALSE(takeFailure(entries[2]));
  CHECK(entries[2].options.size() == 1);
}
