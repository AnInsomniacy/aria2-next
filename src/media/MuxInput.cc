/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/mem.h>
}

#include "media/MuxInput.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <gpac/list.h>
#include <ios>
#include <stdexcept>
#include "media/MediaFiles.h"
#include <gpac/webvtt.h>
#include <algorithm>
#include <filesystem>

namespace aria2::media::muxing {

Input::~Input()
{
  if (cues) {
    while (auto cue = static_cast<GF_WebVTTCue*>(gf_list_pop_front(cues)))
      gf_webvtt_cue_del(cue);
    gf_list_del(cues);
  }
  av_packet_free(&subtitlePacket);
  av_packet_free(&packet);
  avformat_close_input(&context);
  if (io) {
    av_freep(&io->buffer);
    avio_context_free(&io);
  }
  for (auto parameter : parameters)
    avcodec_parameters_free(&parameter);
}
int Input::read(void* opaque, uint8_t* data, int length) noexcept
{
  auto& s = *static_cast<Input*>(opaque);

  if (s.control->cancel)
    return AVERROR_EXIT;
  try {
    while (s.index < s.files.size()) {
      if (!s.file.is_open()) {
        s.file.open(nativePath(s.files[s.index]), std::ios::binary);
        if (!s.file)
          return AVERROR(EIO);
        s.file.seekg(s.position - s.starts[s.index]);
      }
      s.file.read(reinterpret_cast<char*>(data), length);
      const auto count = static_cast<int>(s.file.gcount());
      s.position += count;
      if (count > 0)
        return count;
      if (!s.file.eof())
        return AVERROR(EIO);
      s.file.close();
      s.file.clear();
      ++s.index;
    }
    return AVERROR_EOF;
  }
  catch (...) {
    return AVERROR(EIO);
  }
}

int64_t Input::seek(void* opaque, int64_t offset, int whence) noexcept
{
  auto& s = *static_cast<Input*>(opaque);
  if (whence == AVSEEK_SIZE)
    return s.size;
  whence &= ~AVSEEK_FORCE;
  if (whence == SEEK_CUR)
    offset += s.position;
  else if (whence == SEEK_END)
    offset += s.size;
  else if (whence != SEEK_SET)
    return AVERROR(EINVAL);
  if (offset < 0 || offset > s.size)
    return AVERROR(EINVAL);
  s.position = offset;
  s.index = static_cast<size_t>(
      std::upper_bound(s.starts.begin(), s.starts.end(), offset) -
      s.starts.begin());
  if (s.index)
    --s.index;
  s.file.close();
  s.file.clear();
  return offset;
}

void Input::open()
{
  if (!packet || !subtitlePacket)
    throw std::bad_alloc();
  for (const auto& path : files) {
    starts.push_back(size);
    size += std::filesystem::file_size(nativePath(path));
  }
  auto buffer = static_cast<unsigned char*>(av_malloc(65536));
  if (!buffer)
    throw std::bad_alloc();
  io = avio_alloc_context(buffer, 65536, 0, this, read, nullptr, seek);
  if (!io) {
    av_free(buffer);
    throw std::bad_alloc();
  }
  context = avformat_alloc_context();
  if (!context)
    throw std::bad_alloc();
  context->pb = io;
  context->flags |= AVFMT_FLAG_CUSTOM_IO;
  context->io_open = [](AVFormatContext*, AVIOContext**, const char*, int,
                        AVDictionary**) -> int { return AVERROR(EPERM); };
  context->interrupt_callback = {
      [](void* data) -> int { return static_cast<Control*>(data)->cancel; },
      control};
  check(avformat_open_input(&context, nullptr, nullptr, nullptr));
  check(avformat_find_stream_info(context, nullptr));
  boxedWebvtt = type == "subtitle" && context->nb_streams == 1 &&
                context->streams[0]->codecpar->codec_tag == AV_RL32("wvtt");
  if (boxedWebvtt) {
    auto codec = context->streams[0]->codecpar;
    codec->codec_type = AVMEDIA_TYPE_SUBTITLE;
    codec->codec_id = AV_CODEC_ID_WEBVTT;
    codec->codec_tag = 0;
  }
  if (type == "subtitle" &&
      (context->nb_streams != 1 ||
       context->streams[0]->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE))
    throw std::runtime_error("Selected subtitle format cannot be remuxed");
}

} // namespace aria2::media::muxing
