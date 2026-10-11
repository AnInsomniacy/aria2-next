/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aria2 {

class Task;

// One task of an aria2 input file: a line of tab-separated URIs followed by
// indented name=value option lines.
struct SessionEntry {
  std::vector<std::string> uris;
  std::vector<std::pair<std::string, std::string>> options;
};

std::vector<SessionEntry> parseSession(std::string_view content);

// Unfinished tasks, plus finished ones with force-save, in input-file format.
std::string serializeSession(const std::vector<std::shared_ptr<Task>>& tasks);

} // namespace aria2
