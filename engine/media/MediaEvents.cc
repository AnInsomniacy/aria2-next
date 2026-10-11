/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include <cstdint>
#include <gpac/dash.h>
#include <gpac/setup.h>
#include <gpac/tools.h>
#include <stdexcept>
#include "media/MediaJob.h"
#include "media/MediaFiles.h"
#include <filesystem>

namespace aria2::media {

GF_Err MediaJob::event(GF_DASHEventType event, int detail, GF_Err error)
{
  if (error < 0) {
    if (failure.empty())
      failure = gf_error_to_string(error);
    return error;
  }
  if (event == GF_DASH_EVENT_SELECT_GROUPS)
    return selectGroups();
  if (event == GF_DASH_EVENT_CREATE_PLAYBACK)
    createPlayback();
  // GPAC also reports -1 when the next segment is ahead of live. That is
  // an ordinary availability wait, not lost recording data. Joining at the
  // live edge may skip old segments before recording has started.
  if (event == GF_DASH_EVENT_TIMESHIFT_OVERFLOW && detail > 0 &&
      snapshot().completedDuration > 0)
    throw std::runtime_error("Live media left the server's retention window; "
                             "the recording has a gap");
  return GF_OK;
}

void MediaJob::createPlayback()
{
  for (int group : selected) {
    u64 first = 0, last = 0;
    auto init =
        gf_dash_group_get_segment_init_url(dash, group, &first, &last, nullptr);
    u32 crypto = 0;
    bin128 iv{};
    auto key = gf_dash_group_get_segment_init_keys(dash, group, &crypto, &iv);
    if (crypto > 1)
      throw Failure(FailureCode::ProtectedMedia,
                    "Encrypted sample media is not supported");
    auto path =
        localResource(init, first, last ? static_cast<int64_t>(last) : -1);
    if (!path.empty() && ((crypto == 1 && key) || transport.hasCustomKeys()))
      path = transport.decrypt(path, key ? key : "", iv, !live);
    if (gf_dash_group_init_segment_is_media(dash, group)) {
      u32 number = 0, duration = 0, discontinuity = 0;
      GF_Fraction64 start{};
      const char* segmentUrl = nullptr;
      if (gf_dash_group_next_seg_info(dash, group, 0, nullptr, &number, &start,
                                      &duration, nullptr) < 0 ||
          gf_dash_group_get_next_segment_location(
              dash, group, 0, &segmentUrl, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              &discontinuity) < 0)
        throw std::runtime_error("Cannot read initial media segment timing");
      commit(describe(group, number, start, duration, path, discontinuity));
      gf_dash_group_store_stats(dash, group, 0, 0,
                                std::filesystem::file_size(nativePath(path)),
                                GF_FALSE, 0);
    }
    else
      groups[group].init = path;
    transport.retain(path);
    // GPAC queues the initialization resource as the first cache entry.
    // It has now been consumed, exactly as in the native dashin filter.
    gf_dash_group_discard_segment(dash, group);
  }
  progress();
}

} // namespace aria2::media
