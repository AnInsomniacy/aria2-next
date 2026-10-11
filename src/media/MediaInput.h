/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aria2::media {
struct InputTrack {
  std::string id, type;
  std::vector<std::string> urls;
  int64_t offsetMs = 0;
};
struct InputKey {
  std::string url, key, iv;
};
struct InputPlan {
  std::map<std::string, std::string> manifests;
  std::vector<InputTrack> tracks;
  std::vector<InputKey> keys;
};
InputPlan parseInputPlan(const std::string& json);
std::array<unsigned char, 16> decodeMediaKey(const std::string& hex);
} // namespace aria2::media
