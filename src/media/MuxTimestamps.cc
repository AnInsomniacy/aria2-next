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
// Packed audio segments start with an ID3v2 tag whose PRIV frame carries
// the MPEG-2 transport timestamp of the first sample. FFmpeg exposes it only
// for AAC and hands the tags of later segments to other decoders as data,
// so the tag is read here and skipped by Input::read.
PackedAudioTag packedAudioTag(const std::string& path)
{
  std::ifstream input(nativePath(path), std::ios::binary);
  std::array<unsigned char, 10> header{};
  if (!input.read(reinterpret_cast<char*>(header.data()), header.size()) ||
      std::memcmp(header.data(), "ID3", 3) != 0)
    return {};
  const auto syncsafe = [](const unsigned char* b) {
    return int64_t{b[0] & 0x7f} << 21 | int64_t{b[1] & 0x7f} << 14 |
           int64_t{b[2] & 0x7f} << 7 | int64_t{b[3] & 0x7f};
  };
  const auto version = header[3];
  const auto body = syncsafe(&header[6]);
  PackedAudioTag tag{10 + body + ((header[5] & 0x10) ? 10 : 0)};
  if (body > (1 << 20))
    return tag;
  std::string frames(static_cast<size_t>(body), '\0');
  if (!input.read(frames.data(), body))
    return tag;
  constexpr std::string_view owner =
      "com.apple.streaming.transportStreamTimestamp";
  for (size_t at = 0; at + 10 <= frames.size();) {
    const auto* frame = reinterpret_cast<const unsigned char*>(&frames[at]);
    const auto length = static_cast<size_t>(
        version >= 4 ? syncsafe(frame + 4) : int64_t{AV_RB32(frame + 4)});
    if (frame[0] == 0 || length > frames.size() - at - 10)
      break;
    const std::string_view data(&frames[at + 10], length);
    if (std::memcmp(frame, "PRIV", 4) == 0 &&
        data.size() == owner.size() + 9 &&
        data.substr(0, owner.size()) == owner && data[owner.size()] == '\0') {
      const auto clock = AV_RB64(data.data() + owner.size() + 1);
      if (clock < (uint64_t{1} << 33))
        tag.clock = static_cast<int64_t>(clock);
    }
    at += 10 + length;
  }
  return tag;
}

} // namespace aria2::media::muxing
