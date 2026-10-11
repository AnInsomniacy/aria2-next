/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/mathematics.h>
#include <libavutil/parseutils.h>
#include <libavutil/rational.h>
}

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include "media/MuxInput.h"
#include "media/MediaFiles.h"
#include <array>
#include <charconv>
#include <cstring>
#include <string_view>

namespace aria2::media::muxing {

void check(int result)
{
  if (result >= 0)
    return;
  char message[AV_ERROR_MAX_STRING_SIZE];
  av_strerror(result, message, sizeof(message));
  throw std::runtime_error(std::string("Media remux failed: ") + message);
}
int64_t subtitleOffset(const std::string& path)
{
  std::ifstream input(nativePath(path));
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      break;
    constexpr const char* prefix = "X-TIMESTAMP-MAP=";
    if (line.compare(0, std::char_traits<char>::length(prefix), prefix))
      continue;
    auto local = line.find("LOCAL:");
    auto mpeg = line.find("MPEGTS:");
    if (local == std::string::npos || mpeg == std::string::npos)
      throw std::runtime_error("Invalid HLS subtitle timestamp map");
    auto cue = line.substr(local + 6, line.find(',', local) - local - 6);
    auto clock = line.substr(mpeg + 7, line.find(',', mpeg) - mpeg - 7);
    int64_t localTime = 0, timestamp = 0;
    const auto parsed =
        std::from_chars(clock.data(), clock.data() + clock.size(), timestamp);
    if (av_parse_time(&localTime, cue.c_str(), 1) < 0 ||
        parsed.ec != std::errc() || parsed.ptr != clock.data() + clock.size() ||
        timestamp < 0 || timestamp >= (int64_t{1} << 33))
      throw std::runtime_error("Invalid HLS subtitle timestamp map");
    return av_rescale_q(timestamp, AVRational{1, 90000}, AV_TIME_BASE_Q) -
           localTime;
  }
  return 0;
}
bool sameCodec(const AVCodecParameters* a, const AVCodecParameters* b)
{
  return a->codec_type == b->codec_type && a->codec_id == b->codec_id &&
         a->width == b->width && a->height == b->height &&
         a->sample_rate == b->sample_rate &&
         !av_channel_layout_compare(&a->ch_layout, &b->ch_layout) &&
         a->extradata_size == b->extradata_size &&
         (!a->extradata_size ||
          !std::memcmp(a->extradata, b->extradata, a->extradata_size));
}
int64_t packedAudioClock(AVFormatContext* context)
{
  // FFmpeg parses ID3 PRIV frames and exposes their bytes as escaped metadata.
  const auto tag = av_dict_get(
      context->metadata,
      "id3v2_priv.com.apple.streaming.transportStreamTimestamp", nullptr, 0);
  if (!tag)
    throw std::runtime_error("Packed HLS audio has no transport timestamp");
  std::string_view value(tag->value);
  std::array<unsigned char, 8> bytes{};
  for (auto& byte : bytes) {
    if (value.empty())
      throw std::runtime_error("Invalid packed HLS audio timestamp");
    if (value.front() == '\\') {
      unsigned number = 0;
      if (value.size() < 4 || value[1] != 'x')
        throw std::runtime_error("Invalid packed HLS audio timestamp");
      const auto parsed =
          std::from_chars(value.data() + 2, value.data() + 4, number, 16);
      if (parsed.ec != std::errc() || parsed.ptr != value.data() + 4)
        throw std::runtime_error("Invalid packed HLS audio timestamp");
      byte = static_cast<unsigned char>(number);
      value.remove_prefix(4);
    }
    else {
      byte = static_cast<unsigned char>(value.front());
      value.remove_prefix(1);
    }
  }
  const auto clock = AV_RB64(bytes.data());
  if (!value.empty() || clock >= (uint64_t{1} << 33))
    throw std::runtime_error("Invalid packed HLS audio timestamp");
  return static_cast<int64_t>(clock);
}

} // namespace aria2::media::muxing
