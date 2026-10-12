/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Log.h"
#include "options/Options.h"
#include "media/MediaTransport.h"
#include <cstdint>
#include <gpac/dash.h>
#include <gpac/setup.h>
#include <gpac/tools.h>
#include <map>
#include <stdexcept>
#include <string>
#include "media/MediaJob.h"
#include "media/MediaFiles.h"
#include <algorithm>
#include <filesystem>

namespace aria2::media {

void MediaJob::resumeLive()
{
  if (!control->finish && live && !gf_dash_is_m3u8(dash) &&
      !lastSegments.empty()) {
    std::map<std::string, int64_t> ends;
    for (const auto& entry : lastSegments) {
      const auto& segment = entry.second;
      for (const auto* type : {"audio", "video"})
        if (segment.type == type || segment.type == "muxed")
          ends[type] = std::max(ends[type], segment.period + segment.start +
                                                segment.duration);
    }
    if (!ends.empty()) {
      const auto target = std::min_element(
          ends.begin(), ends.end(),
          [](const auto& a, const auto& b) { return a.second < b.second; });
      if (gf_dash_resume_at(dash, target->second) != GF_OK)
        throw std::runtime_error(
            "Live media left the server's retention window; "
            "the recording has a gap");
    }
  }
}

MediaJob::SegmentResult MediaJob::consumeSegment(int group)
{
  const char* url = nullptr;
  const char* key = nullptr;
  const char* name = nullptr;
  const char* switchingInit = nullptr;
  u64 first = 0, last = 0;
  u64 initFirst = 0, initLast = 0;
  bin128 iv{};
  u32 discontinuity = 0;
  auto status = gf_dash_group_get_next_segment_location(
      dash, group, 0, &url, &first, &last, nullptr, &switchingInit, &initFirst,
      &initLast, nullptr, nullptr, &key, &iv, nullptr, &discontinuity);
  if (status == GF_EOS) {
    gf_dash_set_group_done(dash, group, GF_TRUE);
    return SegmentResult::Complete;
  }
  if (status == GF_BUFFER_TOO_SMALL || status == GF_NOT_READY)
    return SegmentResult::Waiting;
  if (status < 0)
    throw std::runtime_error(gf_error_to_string(status));
  u32 number = 0, duration = 0;
  GF_Fraction64 start{};
  auto info = gf_dash_group_next_seg_info(dash, group, 0, &name, &number,
                                          &start, &duration, nullptr);
  if (info < 0)
    throw std::runtime_error(gf_error_to_string(info));
  const auto endTime = option->integer("media-end-time");
  const auto position =
      start.den ? gf_timestamp_rescale_signed(start.num, start.den, 1000) : 0;
  log::debug("media: segment {} of group {} at {} ms (offset {} us, period {} ms)",
             number, group, position, groups[group].timeOffset,
             gf_dash_get_period_start(dash));
  if (!live && endTime > 0 &&
      position + gf_dash_get_period_start(dash) >= endTime * 1000) {
    gf_dash_set_group_done(dash, group, GF_TRUE);
    return SegmentResult::Complete;
  }
  if (switchingInit && *switchingInit)
    groups[group].init =
        localResource(switchingInit, initFirst,
                      initLast ? static_cast<int64_t>(initLast) : -1);
  std::string path;
  try {
    path = localResource(url, first, last ? static_cast<int64_t>(last) : -1);
  }
  catch (const HttpError& error) {
    if (!live || error.status != 404)
      throw;
    gf_dash_set_group_download_state(dash, group, 0, GF_URL_ERROR);
    return SegmentResult::Waiting;
  }
  if (!path.empty()) {
    if ((key && *key) || transport.hasCustomKeys()) {
      if (!key || !*key) {
        auto sequence = number;
        for (int index = 15; index >= 0 && sequence; --index) {
          iv[index] = sequence & 0xff;
          sequence >>= 8;
        }
      }
      path = transport.decrypt(path, key ? key : "", iv, !live,
                               groups[group].init);
    }
    commit(describe(group, number, start, duration, path, discontinuity));
    gf_dash_set_group_download_state(dash, group, 0, GF_OK);
    gf_dash_group_store_stats(dash, group, 0, 0,
                              std::filesystem::file_size(nativePath(path)),
                              GF_FALSE, 0);
    gf_dash_group_discard_segment(dash, group);
    return SegmentResult::Advanced;
  }
  return SegmentResult::Waiting;
}

void MediaJob::run()
{
  if (recoverPublication())
    return;
  if (option->get("media") == "collection") {
    runCollection();
    return;
  }
  auto result = control->finish ? GF_OK : gf_dash_open(dash, uri.c_str());
  if (result < 0)
    throw Failure(failureKind,
                  failure.empty() ? gf_error_to_string(result) : failure);
  resumeLive();
  while (!control->cancel && !control->finish && !completed && !awaiting) {
    result = gf_dash_process(dash);
    if (awaiting)
      return;
    if (!failure.empty())
      throw Failure(failureKind, failure);
    if (result < 0 && result != GF_IP_NETWORK_EMPTY && result != GF_NOT_READY)
      throw std::runtime_error(gf_error_to_string(result));
    if (result == GF_EOS) {
      completed = true;
      break;
    }
    if (gf_dash_is_in_setup(dash)) {
      waitForNext();
      continue;
    }
    bool allDone = !selected.empty(), advanced = false;
    for (int group : selected) {
      const auto segment = consumeSegment(group);
      allDone &= segment == SegmentResult::Complete;
      advanced |= segment == SegmentResult::Advanced;
    }
    if (advanced)
      progress();
    const auto limit = option->integer("media-record-time");
    if (limit > 0 && snapshot().live &&
        snapshot().completedDuration >= limit * 1000)
      control->requestFinish();
    if (allDone) {
      if (gf_dash_in_last_period(dash, GF_TRUE) &&
          gf_dash_is_dynamic_mpd(dash) && !control->finish)
        throw std::runtime_error("Live source stopped updating without "
                                 "signaling the end of the recording");
      if (gf_dash_in_last_period(dash, GF_TRUE))
        completed = true;
      else
        gf_dash_request_period_switch(dash);
    }
    if (!advanced && !completed)
      waitForNext();
  }
  if (control->cancel || awaiting)
    return;
  finalize();
}

} // namespace aria2::media
