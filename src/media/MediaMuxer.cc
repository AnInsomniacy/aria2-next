/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
}

#include "media/MediaMuxer.h"
#include "media/Control.h"
#include "media/MediaStore.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>
#include "media/MediaFiles.h"
#include "media/MediaTransport.h"
#include "media/MuxInput.h"
#include "media/MuxOutput.h"
#include "media/MuxTimeline.h"
#include <algorithm>
#include <filesystem>
#include <map>
#ifdef _WIN32
#  include <windows.h>
#endif

namespace aria2::media {

using namespace muxing;

int64_t Muxer::startTime(const Segment& segment,
                         const std::shared_ptr<Control>& control,
                         std::optional<int64_t> reference)
{
  Input input;
  input.control = control.get();
  input.type = segment.type;
  input.segments = {segment};
  input.openRun();
  const AVRational clock{1, 90000};
  auto timestamp = input.transportClock ? av_rescale_q(*input.transportClock,
                                                       clock, AV_TIME_BASE_Q)
                                        : input.context->start_time;
  if (timestamp == AV_NOPTS_VALUE)
    throw std::runtime_error("Live media has no presentation timestamp");
  if (reference)
    timestamp =
        *reference * 1000 +
        av_rescale_q(
            av_compare_mod(av_rescale_q(timestamp, AV_TIME_BASE_Q, clock),
                           av_rescale_q(*reference, AVRational{1, 1000}, clock),
                           uint64_t{1} << 33),
            clock, AV_TIME_BASE_Q);
  return av_rescale_q(timestamp, AV_TIME_BASE_Q, AVRational{1, 1000});
}
std::string
Muxer::stage(const std::vector<Segment>& segments, const std::string& output,
             const std::string& directory, const std::string& format,
             bool video, bool audio, bool subtitles,
             const std::shared_ptr<Control>& control,
             int64_t presentationDuration, bool live, bool collection,
             bool ranged)
{
  if (segments.empty())
    throw std::runtime_error("No complete media segments were received");
  using Epoch = std::pair<int64_t, int64_t>;
  std::map<Epoch, std::map<std::string, std::vector<Segment>>> periods;
  std::map<std::string, std::string> verified;
  int64_t presentationStart = INT64_MAX;
  for (const auto& segment : segments) {
    for (const auto& resource :
         {std::make_pair(segment.path, segment.digest),
          std::make_pair(segment.init, segment.initDigest)}) {
      if (resource.first.empty())
        continue;
      auto found = verified.find(resource.first);
      if (found == verified.end())
        found =
            verified.emplace(resource.first, Transport::digest(resource.first))
                .first;
      if (found->second != resource.second)
        throw std::runtime_error(
            "Media recovery data is damaged; resume to fetch it again");
    }
    periods[{segment.period, segment.discontinuity}][segment.track].push_back(
        segment);
    if (segment.hls)
      presentationStart = std::min(presentationStart, segment.start);
  }
  const auto temporary =
      output + "." + utf8(nativePath(directory).filename()) + ".media-partial";
  struct Cleanup {
    std::string path;
    bool committed = false;
    ~Cleanup()
    {
      if (!committed) {
        std::error_code ignored;
        std::filesystem::remove(nativePath(path), ignored);
      }
    }
  } cleanup{temporary};
  Output out;
  int64_t liveOrigin = INT64_MIN;
  for (const auto& period : periods) {
    std::vector<std::unique_ptr<Input>> inputs;
    int64_t end = 0;
    int64_t commonEnd = INT64_MAX; // where the first audio or video track ends
    for (const auto& track : period.second) {
      auto input = std::make_unique<Input>();
      input->control = control.get();
      input->type = track.second.front().type;
      input->segments = track.second;
      input->presentationStart = presentationStart;
      input->openRun();
      int64_t trackEnd = 0;
      for (const auto& segment : track.second)
        trackEnd = std::max(trackEnd, segment.start + segment.duration);
      end = std::max(end, trackEnd);
      if (input->type != "subtitle")
        commonEnd = std::min(commonEnd, trackEnd);
      inputs.push_back(std::move(input));
    }
    // Segments are chosen whole, so in a range the tracks with longer
    // segments overhang the others by up to one segment.
    if (ranged && !live && commonEnd != INT64_MAX)
      end = commonEnd;
    if (collection) {
      int64_t origin = INT64_MAX;
      for (const auto& input : inputs)
        if (input->context->start_time != AV_NOPTS_VALUE)
          origin =
              std::min(origin, input->context->start_time + input->clockOffset);
      if (origin == INT64_MAX)
        origin = 0;
      for (auto& input : inputs) {
        input->shift = -origin;
        input->boundary = 0;
        input->end = INT64_MAX;
      }
    }
    else if (inputs.front()->segments.front().hls) {
      int64_t nextBoundary = INT64_MAX;
      const auto next = periods.upper_bound(period.first);
      if (next != periods.end())
        for (const auto& track : next->second)
          nextBoundary =
              std::min(nextBoundary,
                       (track.second.front().start - presentationStart) * 1000);
      alignHls(inputs, presentationStart, nextBoundary, liveOrigin,
               period.first == periods.begin()->first, live, format, ranged);
    }
    else {
      // Live recordings and ranges start the output at zero, as a range
      // keeps the source clock and players would show the time before it.
      const bool zeroBased = live || ranged;
      if (zeroBased && liveOrigin == INT64_MIN) {
        int64_t origin = INT64_MAX;
        for (const auto& input : inputs)
          if (!input->webvtt && input->context->start_time != AV_NOPTS_VALUE)
            origin = std::min(origin,
                              input->context->start_time + input->clockOffset);
        if (origin == INT64_MAX && live)
          throw std::runtime_error("Live media has no presentation timestamp");
        liveOrigin = origin == INT64_MAX ? 0
                                         : av_rescale_q(origin, AV_TIME_BASE_Q,
                                                        AVRational{1, 1000});
      }
      const auto next = periods.upper_bound({period.first.first, INT64_MAX});
      const auto boundary = next == periods.end()
                                ? (live ? 0 : presentationDuration)
                                : next->first.first;
      for (auto& input : inputs) {
        input->end =
            (boundary > 0 ? std::min(period.first.first + end, boundary)
                          : period.first.first + end) *
            1000;
        if (boundary <= 0 && end <= 0)
          input->end = INT64_MAX;
        if (zeroBased) {
          input->shift = -liveOrigin * 1000;
          input->boundary -= liveOrigin * 1000;
          input->end -= liveOrigin * 1000;
        }
      }
    }
    remux(out, inputs, temporary, format, video, audio, subtitles,
          control.get());
  }
  if (out.written.empty())
    throw std::runtime_error("The selected media contained no samples");
  for (unsigned i = 0; i < out.context->nb_streams; ++i) {
    const auto type = out.context->streams[i]->codecpar->codec_type;
    if ((type == AVMEDIA_TYPE_AUDIO || type == AVMEDIA_TYPE_VIDEO) &&
        !out.written[i])
      throw std::runtime_error(
          "A selected audio or video track contained no samples");
  }
  check(av_write_trailer(out.context));
  check(avio_closep(&out.context->pb));
  syncFile(temporary);
  syncParent(temporary);
  if (control->cancel)
    throw std::runtime_error("Media finalization interrupted");
  cleanup.committed = true;
  return temporary;
}
void Muxer::publish(const std::string& staging, const std::string& output,
                    bool overwrite)
{
  const auto source = nativePath(staging);
  const auto destination = nativePath(output);
#ifdef _WIN32
  if (!MoveFileExW(source.c_str(), destination.c_str(),
                   overwrite
                       ? MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
                       : MOVEFILE_WRITE_THROUGH))
    throw std::system_error(GetLastError(), std::system_category(),
                            "Cannot publish media output");
#else
  if (overwrite)
    std::filesystem::rename(source, destination);
  else {
    std::filesystem::create_hard_link(source, destination);
    std::filesystem::remove(source);
  }
#endif
}

} // namespace aria2::media
