/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "media/MediaStore.h"
#include "media/Control.h"
#include <gpac/list.h>
extern "C" {
#include <libavformat/avformat.h>
}
#include <fstream>
#include <optional>

namespace aria2::media::muxing {
// Owns all demuxer resources. A native AVIO cursor joins the initialization
// resource and compatible fragments without copying their media payload.
struct Input {
  Input() = default;
  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;
  AVFormatContext* context = nullptr;
  AVIOContext* io = nullptr;
  AVPacket* packet = av_packet_alloc();
  AVPacket* subtitlePacket = av_packet_alloc();
  GF_List* cues = nullptr;
  std::vector<std::string> files;
  std::vector<int64_t> starts;
  std::vector<int> mapping;
  std::ifstream file;
  int64_t position = 0, size = 0;
  size_t index = 0;
  bool ready = false;
  bool prefetched = false;
  std::string type;
  Control* control = nullptr;
  std::vector<Segment> segments;
  size_t nextSegment = 0;
  // Packet offsets, clipping and boundaries use microseconds.
  int64_t shift = 0, presentationStart = 0, clockOffset = 0;
  int64_t clipStart = 0, clipEnd = 0;
  int64_t boundary = 0;
  int64_t end = INT64_MAX;
  bool webvtt = false;
  bool boxedWebvtt = false;
  std::optional<int64_t> transportClock;
  bool allowPreroll = false;
  bool trimAudio = false;
  std::vector<AVCodecParameters*> parameters;

  ~Input();
  static int read(void*, uint8_t*, int) noexcept;
  static int64_t seek(void*, int64_t, int) noexcept;
  void open();
  bool openRun();
  int readPacket();
  void next();
};
using Inputs = std::vector<std::unique_ptr<Input>>;
void check(int result);
bool sameCodec(const AVCodecParameters*, const AVCodecParameters*);
int64_t subtitleOffset(const std::string& path);
int64_t packedAudioClock(AVFormatContext*);
} // namespace aria2::media::muxing
