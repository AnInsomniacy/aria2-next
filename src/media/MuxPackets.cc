/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
}

#include <cstdint>
#include <gpac/list.h>
#include <memory>
#include <stdexcept>
#include <utility>
#include "media/MuxInput.h"
#include <gpac/webvtt.h>
#include <algorithm>
#include <cstring>

namespace aria2::media::muxing {

bool Input::openRun()
{
  if (nextSegment == segments.size())
    return false;
  avformat_close_input(&context);
  if (io) {
    av_freep(&io->buffer);
    avio_context_free(&io);
  }
  file.close();
  file.clear();
  files.clear();
  starts.clear();
  position = size = 0;
  index = 0;
  const auto& first = segments[nextSegment];
  if (!first.init.empty())
    files.push_back(first.init);
  files.push_back(first.path);
  ++nextSegment;
  // A WebVTT header applies only to its own segment. Other compatible
  // fragments share one native demuxer so timestamp unwrapping is retained.
  if (type != "subtitle") {
    while (nextSegment < segments.size() &&
           segments[nextSegment].init == first.init &&
           segments[nextSegment].timeOffset == first.timeOffset) {
      files.push_back(segments[nextSegment++].path);
    }
  }
  open();
  webvtt = context->nb_streams == 1 &&
           context->streams[0]->codecpar->codec_id == AV_CODEC_ID_WEBVTT;
  clockOffset = first.hls ? (webvtt ? subtitleOffset(first.path) : 0)
                          : first.period * 1000 - first.timeOffset;
  if (first.hls && std::strcmp(context->iformat->name, "aac") == 0)
    transportClock = packedAudioClock(context);
  if (!first.hls)
    boundary = first.period * 1000 + shift;
  clipStart = first.hls ? (first.start - presentationStart) * 1000 : 0;
  clipEnd = clipStart + first.duration * 1000;
  if (!parameters.empty()) {
    if (parameters.size() != context->nb_streams)
      throw std::runtime_error("Media track layout changed during download");
    for (unsigned i = 0; i < context->nb_streams; ++i) {
      const auto a = parameters[i];
      const auto b = context->streams[i]->codecpar;
      if (!sameCodec(a, b))
        throw std::runtime_error("Media codec parameters changed; lossless "
                                 "concatenation is unavailable");
    }
  }
  else {
    for (unsigned i = 0; i < context->nb_streams; ++i) {
      auto parameter = avcodec_parameters_alloc();
      if (!parameter)
        throw std::bad_alloc();
      parameters.push_back(parameter);
      check(avcodec_parameters_copy(parameter, context->streams[i]->codecpar));
    }
  }
  return true;
}

int Input::readPacket()
{
  for (;;) {
    if (cues && gf_list_count(cues)) {
      std::unique_ptr<GF_WebVTTCue, decltype(&gf_webvtt_cue_del)> cue(
          static_cast<GF_WebVTTCue*>(gf_list_pop_front(cues)),
          gf_webvtt_cue_del);
      if (!cue->text || !*cue->text)
        continue;
      check(av_new_packet(packet, static_cast<int>(std::strlen(cue->text))));
      std::memcpy(packet->data, cue->text, packet->size);
      check(av_packet_copy_props(packet, subtitlePacket));
      packet->stream_index = subtitlePacket->stream_index;
      for (const auto& property :
           {std::make_pair(AV_PKT_DATA_WEBVTT_IDENTIFIER, cue->id),
            std::make_pair(AV_PKT_DATA_WEBVTT_SETTINGS, cue->settings)}) {
        if (!property.second || !*property.second)
          continue;
        const auto length = std::strlen(property.second);
        auto data = av_packet_new_side_data(packet, property.first, length);
        if (!data)
          throw std::bad_alloc();
        std::memcpy(data, property.second, length);
      }
      return 0;
    }
    if (cues) {
      gf_list_del(cues);
      cues = nullptr;
    }
    const auto result = av_read_frame(context, packet);
    if (result < 0 || !boxedWebvtt)
      return result;
    // GPAC owns ISO WebVTT boxes; FFmpeg owns sample timing and muxing.
    cues = gf_webvtt_parse_cues_from_data(packet->data, packet->size, 0, 0);
    if (!cues)
      throw std::runtime_error("Invalid ISO WebVTT sample");
    av_packet_unref(subtitlePacket);
    av_packet_move_ref(subtitlePacket, packet);
  }
}

void Input::next()
{
  if (!prefetched)
    av_packet_unref(packet);
  for (;;) {
    auto result = prefetched ? 0 : readPacket();
    prefetched = false;
    if (result == AVERROR_EOF) {
      if (openRun())
        continue;
      ready = false;
      return;
    }
    check(result);
    if (packet->stream_index >= 0 &&
        static_cast<size_t>(packet->stream_index) < mapping.size() &&
        mapping[packet->stream_index] >= 0) {
      av_packet_rescale_ts(packet,
                           context->streams[packet->stream_index]->time_base,
                           AV_TIME_BASE_Q);
      const auto offset = shift + clockOffset;
      if (packet->pts != AV_NOPTS_VALUE)
        packet->pts += offset;
      if (packet->dts != AV_NOPTS_VALUE)
        packet->dts += offset;
      const bool preroll =
          allowPreroll &&
          !(trimAudio &&
            context->streams[packet->stream_index]->codecpar->codec_type ==
                AVMEDIA_TYPE_AUDIO);
      if (!(webvtt && segments.front().hls) && packet->pts != AV_NOPTS_VALUE &&
          ((!preroll && packet->pts + packet->duration <= boundary) ||
           packet->pts >= end)) {
        // Initialization can expose a complete encoder-priming packet before
        // the Period. It must not overlap the preceding Period's audio.
        av_packet_unref(packet);
        continue;
      }
      if (webvtt && segments.front().hls) {
        const AVRational clock{1, 90000};
        const auto timestamp =
            av_rescale_q(packet->pts - shift, AV_TIME_BASE_Q, clock);
        const auto reference =
            av_rescale_q(clipStart - shift, AV_TIME_BASE_Q, clock);
        packet->pts =
            clipStart + av_rescale_q(av_compare_mod(timestamp, reference,
                                                    uint64_t{1} << 33),
                                     clock, AV_TIME_BASE_Q);
        const auto end = std::min(packet->pts + packet->duration, clipEnd);
        packet->pts = packet->dts = std::max(packet->pts, clipStart);
        packet->duration = end - packet->pts;
        if (packet->duration <= 0) {
          av_packet_unref(packet);
          continue;
        }
      }
      ready = true;
      return;
    }
    av_packet_unref(packet);
  }
}

} // namespace aria2::media::muxing
