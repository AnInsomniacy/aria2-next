/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once
#include "media/MediaStore.h"
#include <memory>
#include <string>
#include <vector>
namespace aria2 {
namespace media {
class Muxer {
public:
  static int64_t startTime(const Segment& segment,
                           const std::shared_ptr<Control>& control,
                           std::optional<int64_t> reference = {});
  // ranged: media-start-time or media-end-time selected part of the source.
  static std::string
  stage(const std::vector<Segment>& segments, const std::string& output,
        const std::string& directory, const std::string& format, bool video,
        bool audio, bool subtitles, const std::shared_ptr<Control>& control,
        int64_t presentationDuration = 0, bool live = false,
        bool collection = false, bool ranged = false);
  static void publish(const std::string& staging, const std::string& output,
                      bool overwrite);
};
} // namespace media
} // namespace aria2
