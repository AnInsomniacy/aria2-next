/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "state/Session.h"

#include "base/Text.h"
#include "core/Task.h"

#include <algorithm>

namespace aria2 {

std::vector<SessionEntry> parseSession(std::string_view content)
{
  std::vector<SessionEntry> entries;
  // Windows editors and PowerShell 5 write UTF-8 with a byte order mark,
  // which would otherwise become part of the first URI.
  if (content.starts_with("\xEF\xBB\xBF")) {
    content.remove_prefix(3);
  }
  // Leading whitespace marks option lines, so lines are not trimmed.
  for (size_t begin = 0; begin < content.size();) {
    const auto end = std::min(content.find('\n', begin), content.size());
    auto line = content.substr(begin, end - begin);
    begin = end + 1;
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    if (line.empty() || line.front() == '#') {
      continue;
    }
    if (line.front() == ' ' || line.front() == '\t') {
      const auto option = text::trim(line);
      const auto equals = option.find('=');
      if (!entries.empty() && equals != std::string_view::npos) {
        entries.back().options.emplace_back(option.substr(0, equals),
                                            option.substr(equals + 1));
      }
      continue;
    }
    entries.push_back({text::split(line, '\t'), {}});
  }
  return entries;
}

namespace {
bool saved(const Task& task)
{
  switch (task.state()) {
  case TaskState::Complete:
    return task.options().flag("force-save");
  case TaskState::Removed:
    return false;
  case TaskState::Error:
    return task.errorCode() != ErrorCode::ResourceNotFound ||
           task.options().flag("save-not-found");
  default:
    return true;
  }
}
} // namespace

std::string serializeSession(const std::vector<std::shared_ptr<Task>>& tasks)
{
  std::string out;
  for (const auto& task : tasks) {
    // A fetched .torrent or .metalink lives on as the tasks it started.
    if (!task->followedBy.empty() || !saved(*task)) {
      continue;
    }
    const auto uris = task->sessionUris();
    if (uris.empty()) {
      continue;
    }
    out += text::join(uris, "\t") + '\n';
    out += " gid=" + gidText(task->gid()) + '\n';
    if (task->state() == TaskState::Paused) {
      out += " pause=true\n";
    }
    for (const auto& [name, value] : task->options().own()) {
      if (name == "gid" || name == "pause") {
        continue;
      }
      for (const auto& line : text::split(value, '\n')) {
        out += ' ' + name + '=' + line + '\n';
      }
    }
  }
  return out;
}

} // namespace aria2
