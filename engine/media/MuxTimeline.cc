/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
}

#include "media/MuxTimeline.h"
#include "media/MuxInput.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace aria2::media::muxing {

namespace {
int64_t hlsClockOrigin(Inputs& inputs)
{
  int64_t origin = INT64_MAX;
  const AVRational clock{1, 90000};
  int64_t reference = AV_NOPTS_VALUE;
  for (const auto& input : inputs)
    if (!input->webvtt && !input->transportClock &&
        input->context->start_time != AV_NOPTS_VALUE) {
      reference = input->context->start_time;
      break;
    }
  for (const auto& input : inputs) {
    if (input->transportClock) {
      input->clockOffset =
          reference == AV_NOPTS_VALUE
              ? av_rescale_q(*input->transportClock, clock, AV_TIME_BASE_Q)
              : reference +
                    av_rescale_q(
                        av_compare_mod(
                            *input->transportClock,
                            av_rescale_q(reference, AV_TIME_BASE_Q, clock),
                            uint64_t{1} << 33),
                        clock, AV_TIME_BASE_Q);
      origin = std::min(origin, input->clockOffset);
    }
    else if (!input->webvtt && input->context->start_time != AV_NOPTS_VALUE)
      origin = std::min(origin, input->context->start_time);
  }
  if (origin == INT64_MAX)
    origin = 0;
  return origin;
}

void seekLiveKeyframe(Inputs& inputs, int64_t& liveOrigin, int64_t beginning,
                      int64_t origin)
{
  for (auto& input : inputs) {
    if (input->type == "audio" || input->webvtt ||
        input->segments.front().start >= liveOrigin)
      continue;
    const auto target =
        (liveOrigin - beginning) * 1000 + origin - input->clockOffset;
    check(avformat_seek_file(input->context, -1, target, target, INT64_MAX, 0));
    for (;;) {
      check(av_read_frame(input->context, input->packet));
      const auto stream = input->context->streams[input->packet->stream_index];
      if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        const auto timestamp =
            av_rescale_q(input->packet->pts, stream->time_base, AV_TIME_BASE_Q);
        liveOrigin =
            beginning + (timestamp + input->clockOffset - origin) / 1000;
        input->prefetched = true;
        break;
      }
      av_packet_unref(input->packet);
    }
  }
}
} // namespace

void alignHls(Inputs& inputs, int64_t presentationStart, int64_t nextBoundary,
              int64_t& liveOrigin, bool firstEpoch, bool live,
              const std::string& format)
{
  const auto origin = hlsClockOrigin(inputs);
  int64_t beginning = INT64_MAX;
  for (const auto& input : inputs)
    beginning = std::min(beginning, input->segments.front().start);
  int64_t commonEnd = INT64_MAX;
  if (live) {
    for (const auto& input : inputs) {
      if (input->webvtt)
        continue;
      if (firstEpoch)
        liveOrigin = std::max(liveOrigin, input->segments.front().start);
      const auto& last = input->segments.back();
      commonEnd = std::min(commonEnd, last.start + last.duration);
    }
    if (liveOrigin == INT64_MIN)
      throw std::runtime_error("Live media has no audio or video timeline");
    if (format == "mkv")
      seekLiveKeyframe(inputs, liveOrigin, beginning, origin);
    if (commonEnd <= liveOrigin)
      throw std::runtime_error("Live tracks have no common recording window");
  }
  for (auto& input : inputs) {
    input->boundary = (beginning - presentationStart) * 1000;
    // Complete HLS fragments own their packet boundary. Millisecond-rounded
    // playlist durations must not truncate audio after many short segments.
    input->end = nextBoundary;
    input->shift = (beginning - presentationStart) * 1000 - origin;
    if (live) {
      // Native MP4 edit lists retain decoder preroll while exposing only
      // the common recording window. All tracks keep their source clock.
      input->boundary = (beginning - liveOrigin) * 1000;
      input->shift = (beginning - liveOrigin) * 1000 - origin;
      input->end = (commonEnd - liveOrigin) * 1000;
      if (format == "mkv") {
        // Matroska has no edit lists. Start at the native demuxer's next
        // video keyframe and discard preceding audio.
        input->trimAudio = true;
        input->boundary = 0;
      }
    }
  }
}

} // namespace aria2::media::muxing
