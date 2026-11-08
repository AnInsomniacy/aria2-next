/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "base/Error.h"

#include <memory>
#include <optional>
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

// How a task had failed when the session was saved. It is restored as failed
// rather than started again.
struct SessionFailure {
  ErrorCode code;
  std::string message;
};

// Removes the failure an entry records from its options.
std::optional<SessionFailure> takeFailure(SessionEntry& entry);

// Unfinished and failed tasks, plus finished ones with force-save, in
// input-file format.
std::string serializeSession(const std::vector<std::shared_ptr<Task>>& tasks);

} // namespace aria2
