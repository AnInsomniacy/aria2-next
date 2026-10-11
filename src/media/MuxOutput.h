/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "media/MuxInput.h"
#include <map>

namespace aria2::media::muxing {
struct Output {
  Output() = default;
  Output(const Output&) = delete;
  Output& operator=(const Output&) = delete;
  AVFormatContext* context = nullptr;
  std::map<std::pair<int, unsigned>, unsigned> mapping;
  std::map<int, size_t> written;
  // Last decoding timestamp written per output stream, in its time base.
  std::map<int, int64_t> lastDts;
  ~Output();
};
void remux(Output&, Inputs&, const std::string& path, const std::string& format,
           bool video, bool audio, bool subtitles, Control*);
} // namespace aria2::media::muxing
