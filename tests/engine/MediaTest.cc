/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "media/MediaError.h"
#include "media/MediaInput.h"
#include "media/MediaRequestContext.h"
#include "media/MediaTask.h"
#include "options/Options.h"

#include <doctest/doctest.h>

using namespace aria2;

TEST_CASE("media is recognised by option, path or content type")
{
  Options options;
  CHECK(media::MediaTask::handles("https://cdn/live/Index.M3U8?token=1",
                                  options));
  CHECK(media::MediaTask::handles("https://cdn/vod/manifest.mpd", options));
  CHECK_FALSE(media::MediaTask::handles("https://cdn/video.mp4", options));
  options.set("media", "hls");
  CHECK(media::MediaTask::handles("https://cdn/manifest", options));
  options.set("media", "file");
  CHECK_FALSE(media::MediaTask::handles("https://cdn/index.m3u8", options));
  CHECK(media::MediaTask::manifestType("application/vnd.apple.mpegurl"));
  CHECK(media::MediaTask::manifestType("Application/X-MpegURL; charset=utf-8"));
  CHECK(media::MediaTask::manifestType("application/dash+xml"));
  CHECK_FALSE(media::MediaTask::manifestType("video/mp2t"));
}

TEST_CASE("request contexts are scoped to distinct origins")
{
  const auto contexts = media::parseRequestContexts(
      R"([{"url":"https://a.example/x","headers":[{"name":"X-Token","value":"1"},{"name":"Cookie","value":"c=1"}]}])");
  REQUIRE(contexts.size() == 1);
  CHECK(contexts[0].headers.at("x-token") == "1");
  CHECK(contexts[0].headers.at("cookie") == "c=1");
  const auto rejected = [](const char* payload) {
    CHECK_THROWS_AS(media::parseRequestContexts(payload), media::Failure);
  };
  rejected(
      R"([{"url":"https://a.example/","headers":[]},{"url":"https://a.example/b","headers":[]}])");
  rejected(
      R"([{"url":"https://a.example/","headers":[{"name":"Host","value":"x"}]}])");
  rejected(
      R"([{"url":"https://a.example/","headers":[{"name":"X","value":"a\nb"}]}])");
  rejected(R"([{"url":"file:///etc/passwd","headers":[]}])");
  rejected(R"({"url":"https://a.example/"})");
  CHECK(media::parseRequestContexts("").empty());
}

TEST_CASE("captured input plans are validated")
{
  const auto plan = media::parseInputPlan(
      R"({"manifests":[{"url":"https://a/m.m3u8","content":"#EXTM3U"}],)"
      R"("tracks":[{"id":"v","type":"video","urls":["https://a/1.ts"],"offsetMs":20}],)"
      R"("keys":[{"url":"https://a/key","key":"00112233445566778899aabbccddeeff","iv":""}]})");
  CHECK(plan.manifests.at("https://a/m.m3u8") == "#EXTM3U");
  REQUIRE(plan.tracks.size() == 1);
  CHECK(plan.tracks[0].offsetMs == 20);
  CHECK(plan.keys.size() == 1);
  CHECK_THROWS_AS(
      media::parseInputPlan(
          R"({"manifests":[],"tracks":[],"keys":[{"url":"","key":"zz","iv":""}]})"),
      media::Failure);
  CHECK_THROWS_AS(
      media::parseInputPlan(
          R"({"manifests":[],"tracks":[{"id":"v","type":"image","urls":["https://a"]}],"keys":[]})"),
      media::Failure);
}
