/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

#include "options/Options.h"
#include "media/MediaJob.h"
#include "media/MediaInput.h"
#include "media/MuxInput.h"
#include "base/Log.h"
#include "base/Text.h"
#include "http/Request.h"
#include <gpac/isomedia.h>
#include <algorithm>
#include <optional>
#include <tuple>

namespace aria2::media {
namespace {
// Enough for the header of a fragmented MP4 track: its moov and segment index.
constexpr int64_t headBytes = 64 * 1024;

int64_t totalLength(const Resource& head)
{
  for (const auto& [name, value] : head.headers)
    if (text::iequals(name, "content-range")) {
      const auto range = http::parseContentRange(value);
      return range && range->total > 0 ? range->total : 0;
    }
  return 0;
}

// The duration a track declares in its segment index, fragment header or
// movie header. FFmpeg would estimate one from the first fragment alone.
int64_t declaredDurationMs(const std::string& path)
{
  GF_ISOFile* file = nullptr;
  u64 missing = 0;
  if (gf_isom_open_progressive(path.c_str(), 0, 0, GF_FALSE, &file, &missing) !=
          GF_OK ||
      !file)
    return 0;
  int64_t result = 0;
  u64 duration = 0;
  u32 timescale = 0;
  if (gf_isom_get_sidx_duration(file, &duration, &timescale) == GF_OK &&
      timescale)
    result = static_cast<int64_t>(duration * 1000 / timescale);
  else if (const auto movie = gf_isom_get_timescale(file))
    result =
        static_cast<int64_t>(std::max(gf_isom_get_fragmented_duration(file),
                                      gf_isom_get_original_duration(file)) *
                             1000 / movie);
  gf_isom_close(file);
  return result;
}

// What a single-file track holds, read from its first bytes. The browser's
// MIME labels cannot tell a video-only MP4 from an audio-only one.
std::optional<Track> inspect(const InputTrack& input, Transport& transport,
                             Control* control, int64_t& durationMs)
{
  try {
    const auto head = transport.get(input.urls.front(), 0, headBytes - 1);
    muxing::Input demuxer;
    demuxer.files = {head.path};
    demuxer.type = input.type;
    demuxer.control = control;
    demuxer.open();
    Track track;
    track.id = input.id;
    bool video = false, audio = false;
    for (unsigned i = 0; i < demuxer.context->nb_streams; ++i) {
      const auto stream = demuxer.context->streams[i];
      const auto codec = stream->codecpar;
      if (codec->codec_type == AVMEDIA_TYPE_VIDEO && !video) {
        video = true;
        track.codec = avcodec_get_name(codec->codec_id);
        track.width = codec->width;
        track.height = codec->height;
        const auto rate = stream->avg_frame_rate.num ? stream->avg_frame_rate
                                                     : stream->r_frame_rate;
        track.frameRate = rate.num && rate.den ? av_q2d(rate) : 0;
      }
      else if (codec->codec_type == AVMEDIA_TYPE_AUDIO && !audio) {
        audio = true;
        if (!video)
          track.codec = avcodec_get_name(codec->codec_id);
      }
    }
    if (!video && !audio)
      return std::nullopt;
    track.type = video && audio ? "muxed" : video ? "video" : "audio";
    const auto duration = declaredDurationMs(head.path);
    const auto total = totalLength(head);
    if (duration > 0) {
      durationMs = std::max(durationMs, duration);
      if (total > 0)
        track.bandwidth = total * 8 * 1000 / duration;
    }
    return track;
  }
  catch (const std::exception& error) {
    log::debug("media: cannot inspect collection track {}: {}", input.id,
               error.what());
    return std::nullopt;
  }
}

// The option names a track ID, "best" or "none".
const Track* choose(const std::vector<Track>& tracks,
                    const std::vector<std::string>& types,
                    const std::string& selection)
{
  const Track* best = nullptr;
  for (const auto& track : tracks) {
    if (std::find(types.begin(), types.end(), track.type) == types.end())
      continue;
    if (selection == track.id)
      return &track;
    if (selection == "best" &&
        (!best || std::tie(track.height, track.bandwidth) >
                      std::tie(best->height, best->bandwidth)))
      best = &track;
  }
  if (selection != "best" && selection != "none")
    throw Failure(FailureCode::UnsupportedSelection,
                  "Requested media track is unavailable: " + selection);
  return best;
}
} // namespace

void MediaJob::runCollection()
{
  auto plan = parseInputPlan(option->get("media-input"));
  if (plan.tracks.empty())
    throw Failure(FailureCode::UnsupportedSource, "No captured media tracks");
  if (option->integer("media-start-time") || option->integer("media-end-time"))
    throw Failure(FailureCode::UnsupportedSelection,
                  "Time ranges require an HLS or DASH presentation");
  auto value = snapshot();
  value.protocol = "collection";
  value.live = false;
  value.tracks.clear();
  // Separate single-file tracks, such as a video-only and an audio-only MP4,
  // are identified by content and offered like manifest representations.
  std::vector<Track> inspected;
  const bool single =
      plan.tracks.size() > 1 &&
      std::all_of(plan.tracks.begin(), plan.tracks.end(),
                  [](const auto& track) {
                    return track.urls.size() == 1 && track.type != "subtitle";
                  });
  int64_t durationMs = 0;
  for (const auto& input : single ? plan.tracks : std::vector<InputTrack>{}) {
    auto track = inspect(input, transport, control.get(), durationMs);
    if (!track) {
      inspected.clear();
      break;
    }
    inspected.push_back(std::move(*track));
  }
  if (!inspected.empty()) {
    const auto video =
        choose(inspected, {"video", "muxed"}, option->get("media-video"));
    const auto audio =
        video && video->type == "muxed" &&
                (option->get("media-audio") == "best" ||
                 option->get("media-audio") == video->id)
            ? nullptr
            : choose(inspected, {"audio"}, option->get("media-audio"));
    if (!video && !audio)
      throw Failure(FailureCode::UnsupportedSelection, "Select output content");
    std::vector<InputTrack> chosen;
    for (auto& track : inspected) {
      track.selected = &track == video || &track == audio;
      if (!track.selected)
        continue;
      auto input =
          *std::find_if(plan.tracks.begin(), plan.tracks.end(),
                        [&](const auto& item) { return item.id == track.id; });
      input.type = track.type;
      chosen.push_back(std::move(input));
    }
    value.tracks = inspected;
    value.duration = durationMs;
    plan.tracks = std::move(chosen);
  }
  else {
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
  }
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
