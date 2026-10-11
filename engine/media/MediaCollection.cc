/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"
#include "media/MediaJob.h"
#include "media/MediaInput.h"
#include <algorithm>

namespace aria2::media {
void MediaJob::runCollection()
{
  const auto plan = parseInputPlan(option->get("media-input"));
  if (plan.tracks.empty())
    throw Failure(FailureCode::UnsupportedSource, "No captured media tracks");
  if (option->integer("media-start-time") || option->integer("media-end-time"))
    throw Failure(FailureCode::UnsupportedSelection,
                  "Time ranges require an HLS or DASH presentation");
  auto value = snapshot();
  value.protocol = "collection";
  value.live = false;
  value.tracks.clear();
  // The browser has already selected these resources as one composition.
  // Source MIME labels are hints; FFmpeg identifies the actual contained
  // tracks.
  const bool onlySubtitles =
      std::all_of(plan.tracks.begin(), plan.tracks.end(),
                  [](const auto& track) { return track.type == "subtitle"; });
  const bool subtitles =
      std::any_of(plan.tracks.begin(), plan.tracks.end(),
                  [](const auto& track) { return track.type == "subtitle"; });
  const bool audio = std::all_of(
      plan.tracks.begin(), plan.tracks.end(), [](const auto& track) {
        return track.type == "audio" || track.type == "subtitle";
      });
  Track composition;
  composition.id = "composition";
  composition.type = onlySubtitles ? "subtitle" : audio ? "audio" : "muxed";
  composition.selected = true;
  value.tracks.push_back(composition);
  if (subtitles && !onlySubtitles) {
    Track text;
    text.id = "composition-subtitles";
    text.type = "subtitle";
    text.selected = option->get("media-subtitles") != "none";
    value.tracks.push_back(text);
  }
  if (option->get("media-video") == "none" &&
      option->get("media-audio") == "none" &&
      option->get("media-subtitles") == "none")
    throw Failure(FailureCode::UnsupportedSelection, "Select output content");
  store.saveTracks(value.tracks);
  if (option->flag("media-pause-after-probe")) {
    value.state = "awaiting-selection";
    awaiting = true;
    publish(value);
    return;
  }
  value.state = "downloading";
  publish(value);
  for (const auto& input : plan.tracks) {
    int64_t number = 0;
    for (const auto& url : input.urls) {
      if (control->cancel)
        return;
      Segment segment;
      segment.track = input.id;
      segment.type = input.type;
      segment.hls = false;
      segment.timeOffset = -input.offsetMs * 1000;
      segment.number = number++;
      segment.path = transport.get(url).path;
      if (transport.hasCustomKeys()) {
        std::array<unsigned char, 16> iv{};
        auto sequence = static_cast<uint64_t>(segment.number);
        for (int i = 15; i >= 0 && sequence; --i) {
          iv[i] = sequence & 0xff;
          sequence >>= 8;
        }
        segment.path = transport.decrypt(segment.path, "", iv.data(), true);
      }
      commit(segment);
      progress();
    }
  }
  finalize();
}
} // namespace aria2::media
