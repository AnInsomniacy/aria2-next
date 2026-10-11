/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/defs.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
}

#include "MuxOutput.h"
#include "media/MediaDownload.h"
#include "media/MuxInput.h"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "MediaFiles.h"
#include <algorithm>

namespace aria2::media::muxing {

Output::~Output()
{
  if (context) {
    if (context->pb)
      avio_closep(&context->pb);
    avformat_free_context(context);
  }
}

namespace {
bool keep(AVMediaType type, bool video, bool audio, bool subtitle)
{
  return (type == AVMEDIA_TYPE_VIDEO && video) ||
         (type == AVMEDIA_TYPE_AUDIO && audio) ||
         (type == AVMEDIA_TYPE_SUBTITLE && subtitle);
}
} // namespace

void remux(Output& out, std::vector<std::unique_ptr<Input>>& inputs,
           const std::string& path, const std::string& format, bool video,
           bool audio, bool subtitles, Control* control)
{
  const bool firstEpoch = !out.context;
  if (firstEpoch) {
    check(avformat_alloc_output_context2(&out.context, nullptr,
                                         format == "mkv"   ? "matroska"
                                         : format == "vtt" ? "webvtt"
                                                           : "mp4",
                                         path.c_str()));
    if (!out.context)
      throw std::bad_alloc();
    // Native edit lists and codec delay preserve encoder priming; forcing
    // every timestamp to zero would make those samples audible.
  }
  unsigned outputIndex = 0;
  std::map<int, unsigned> streamOrdinals;
  const bool separateAudio =
      std::any_of(inputs.begin(), inputs.end(),
                  [](const auto& i) { return i->type == "audio"; });
  for (auto& input : inputs) {
    input->allowPreroll = firstEpoch;
    input->mapping.assign(input->context->nb_streams, -1);
    for (unsigned i = 0; i < input->context->nb_streams; ++i) {
      auto source = input->context->streams[i];
      auto type = source->codecpar->codec_type;
      if (!keep(type, video, audio, subtitles) ||
          (separateAudio &&
           (input->type == "video" || input->type == "muxed") &&
           type == AVMEDIA_TYPE_AUDIO))
        continue;
      if (avformat_query_codec(out.context->oformat, source->codecpar->codec_id,
                               FF_COMPLIANCE_NORMAL) == 0)
        throw std::runtime_error(
            "Selected codec is not supported by the output container; choose "
            "another media-format");
      const auto key =
          std::make_pair(static_cast<int>(type), streamOrdinals[type]++);
      const auto found = out.mapping.find(key);
      auto stream = firstEpoch ? avformat_new_stream(out.context, nullptr)
                               : (found != out.mapping.end()
                                      ? out.context->streams[found->second]
                                      : nullptr);
      if (!stream)
        throw std::runtime_error("Media track layout changed between periods");
      ++outputIndex;
      input->mapping[i] = stream->index;
      if (firstEpoch) {
        out.mapping[key] = stream->index;
        check(avcodec_parameters_copy(stream->codecpar, source->codecpar));
        stream->codecpar->codec_tag = 0;
        stream->time_base = source->time_base;
        av_dict_copy(&stream->metadata, source->metadata, 0);
      }
      else {
        const auto a = stream->codecpar;
        const auto b = source->codecpar;
        if (!sameCodec(a, b))
          throw std::runtime_error(
              "Media codec parameters changed between periods");
      }
    }
  }
  if (!out.context->nb_streams)
    throw std::runtime_error("No selected media tracks can be saved");
  if (outputIndex != out.context->nb_streams)
    throw std::runtime_error("Media track layout changed between periods");
  if (firstEpoch) {
    check(avio_open(&out.context->pb, nativePath(path).u8string().c_str(),
                    AVIO_FLAG_WRITE));
    check(avformat_write_header(out.context, nullptr));
  }
  for (auto& input : inputs)
    input->next();
  for (;;) {
    if (control->cancel)
      throw std::runtime_error("Media remux interrupted");
    Input* next = nullptr;
    for (auto& input : inputs) {
      if (!input->ready)
        continue;
      if (!next || input->packet->dts < next->packet->dts)
        next = input.get();
    }
    if (!next)
      break;
    auto packet = next->packet;
    packet->stream_index = next->mapping[packet->stream_index];
    av_packet_rescale_ts(packet, AV_TIME_BASE_Q,
                         out.context->streams[packet->stream_index]->time_base);
    packet->pos = -1;
    const auto index = packet->stream_index;
    // Segment boundaries can repeat or step back a decoding timestamp. Muxers
    // reject that for audio and video; like FFmpeg's own remuxer, advance it
    // by one tick and keep presentation at or after decoding. Simultaneous
    // subtitle cues legitimately share a timestamp.
    if (packet->dts != AV_NOPTS_VALUE) {
      const auto type =
          out.context->streams[index]->codecpar->codec_type;
      const int64_t step =
          type == AVMEDIA_TYPE_SUBTITLE || type == AVMEDIA_TYPE_DATA ? 0 : 1;
      const auto last = out.lastDts.find(index);
      if (last != out.lastDts.end() && packet->dts < last->second + step) {
        packet->dts = last->second + step;
        if (packet->pts != AV_NOPTS_VALUE && packet->pts < packet->dts)
          packet->pts = packet->dts;
      }
      out.lastDts[index] = packet->dts;
    }
    check(av_interleaved_write_frame(out.context, packet));
    ++out.written[index];
    next->next();
  }
}

} // namespace aria2::media::muxing
