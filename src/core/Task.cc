/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "core/Task.h"

#include "core/Engine.h"

#include <boost/json/array.hpp>

#include <algorithm>

namespace aria2 {

namespace json = boost::json;

std::string_view stateName(TaskState state)
{
  switch (state) {
  case TaskState::Waiting:
    return "waiting";
  case TaskState::Active:
    return "active";
  case TaskState::Paused:
    return "paused";
  case TaskState::Complete:
    return "complete";
  case TaskState::Error:
    return "error";
  case TaskState::Removed:
    return "removed";
  }
  return "error";
}

bool StatusKeys::want(std::string_view key) const
{
  return keys_.empty() || std::find(keys_.begin(), keys_.end(), key) != keys_.end();
}

Task::Task(Engine& engine, Gid gid, std::shared_ptr<Options> options)
    : engine_(engine), gid_(gid), options_(std::move(options))
{
}

Task::~Task() = default;

void Task::complete()
{
  errorCode_ = ErrorCode::Finished;
  errorMessage_.clear();
  engine_.taskCompleted(*this);
}

void Task::failed(ErrorCode code, std::string message)
{
  errorCode_ = code;
  errorMessage_ = std::move(message);
  engine_.taskFailed(*this);
}

bool Task::changeUris(int, const std::vector<std::string>&,
                      const std::vector<std::string>&, int)
{
  return false;
}

namespace {
json::array fileArray(const std::vector<TaskFile>& files)
{
  json::array result;
  for (const auto& file : files) {
    json::array uris;
    for (const auto& uri : file.uris) {
      uris.push_back(json::object{{"uri", uri.uri},
                                  {"status", uri.used ? "used" : "waiting"}});
    }
    result.push_back(json::object{
        {"index", std::to_string(file.index)},
        {"path", file.path},
        {"length", std::to_string(file.length)},
        {"completedLength", std::to_string(file.completed)},
        {"selected", file.selected ? "true" : "false"},
        {"uris", std::move(uris)},
    });
  }
  return result;
}

json::array gidArray(const std::vector<Gid>& gids)
{
  json::array result;
  for (const auto gid : gids) {
    result.push_back(json::string(gidText(gid)));
  }
  return result;
}
} // namespace

void Task::status(json::object& out, const StatusKeys& keys) const
{
  const bool live = state_ == TaskState::Active;
  auto put = [&](std::string_view key, json::value value) {
    if (keys.want(key)) {
      out[key] = std::move(value);
    }
  };
  put("gid", json::string(gidText(gid_)));
  put("status", json::string(stateName(state_)));
  put("totalLength", json::string(std::to_string(totalLength())));
  put("completedLength", json::string(std::to_string(completedLength())));
  put("uploadLength", json::string(std::to_string(uploadLength())));
  put("downloadSpeed",
      json::string(std::to_string(live ? downloadSpeed() : 0)));
  put("uploadSpeed", json::string(std::to_string(live ? uploadSpeed() : 0)));
  put("connections", json::string(std::to_string(live ? connections() : 0)));
  put("dir", json::string(options_->get("dir")));
  if (state_ == TaskState::Error || state_ == TaskState::Removed ||
      state_ == TaskState::Complete) {
    put("errorCode",
        json::string(std::to_string(static_cast<int>(errorCode_))));
    put("errorMessage", json::string(errorMessage_));
  }
  if (!followedBy.empty()) {
    put("followedBy", gidArray(followedBy));
  }
  if (following) {
    put("following", json::string(gidText(following)));
  }
  if (belongsTo) {
    put("belongsTo", json::string(gidText(belongsTo)));
  }
  if (keys.want("files")) {
    out["files"] = fileArray(files());
  }
}

} // namespace aria2
