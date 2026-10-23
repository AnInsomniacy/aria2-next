/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "base/Error.h"
#include "core/Gid.h"
#include "core/Speed.h"
#include "options/Options.h"

#include <boost/json/object.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aria2 {

class Engine;

enum class TaskState { Waiting, Active, Paused, Complete, Error, Removed };

std::string_view stateName(TaskState state);

struct UriState {
  std::string uri;
  bool used = false;
  std::string current; // where redirects lead, when known; aria2's currentUri
};

struct TaskFile {
  int index = 1; // 1-based, as in aria2
  std::string path;
  int64_t length = 0;
  int64_t completed = 0;
  bool selected = true;
  std::vector<UriState> uris;
};

// The subset of status keys a caller asked for; empty means all.
class StatusKeys {
public:
  StatusKeys() = default;
  explicit StatusKeys(std::vector<std::string> keys) : keys_(std::move(keys)) {}
  bool want(std::string_view key) const;

private:
  std::vector<std::string> keys_;
};

// One download from creation to its stopped result. All methods run on the
// control thread.
class Task : public std::enable_shared_from_this<Task> {
public:
  Task(Engine& engine, Gid gid, std::shared_ptr<Options> options);
  virtual ~Task();
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  Gid gid() const { return gid_; }
  TaskState state() const { return state_; }
  bool stopping() const { return stopping_; }
  Options& options() { return *options_; }
  const Options& options() const { return *options_; }
  const std::shared_ptr<Options>& sharedOptions() const { return options_; }
  ErrorCode errorCode() const { return errorCode_; }
  const std::string& errorMessage() const { return errorMessage_; }

  // Begins transfers. Called when the task becomes active.
  virtual void start() = 0;
  // Releases network work and flushes data, then calls `done`. A forced stop
  // skips courtesy steps such as tracker announces.
  virtual void stop(bool force, std::function<void()> done) = 0;

  virtual int64_t totalLength() const = 0;
  virtual int64_t completedLength() const = 0;
  virtual int64_t downloadSpeed() const { return 0; }
  virtual int64_t uploadSpeed() const { return 0; }
  virtual int64_t uploadLength() const { return 0; }
  virtual int connections() const { return 0; }
  virtual std::vector<TaskFile> files() const = 0;
  // Writes the aria2 status object. Overrides add protocol sections.
  virtual void status(boost::json::object& out, const StatusKeys& keys) const;

  // Session entry: the URIs line; options come from options().own().
  virtual std::vector<std::string> sessionUris() const = 0;
  // Persists resume data; called periodically and when stopping.
  virtual void checkpoint() {}
  // Shows stored progress of a queued task, before it ever starts.
  virtual void loadProgress() {}
  // Called once when the result is discarded or the task is removed.
  virtual void discardState() {}
  // An option in scope::Live changed while active.
  virtual void optionChanged(std::string_view) {}
  // The files a paused task keeps on disk and where they go when its
  // download directory becomes `dir`, as (from, to) pairs. nullopt: this kind
  // of download cannot move.
  using Moves = std::vector<std::pair<std::string, std::string>>;
  virtual std::optional<Moves> relocation(const std::string& dir) const;
  // The files are in `dir` now.
  virtual void relocated(const std::string& dir);
  // Seeding tasks may be excluded from max-concurrent-downloads.
  virtual bool countsTowardConcurrency() const { return true; }
  // The URI-specific RPC views (getUris/getServers) use these.
  virtual bool changeUris(int fileIndex, const std::vector<std::string>& remove,
                          const std::vector<std::string>& add, int position);

  Gid following = 0;
  Gid belongsTo = 0;
  std::vector<Gid> followedBy;

protected:
  // Reports the outcome of an active task to the engine.
  void complete();
  void failed(ErrorCode code, std::string message);
  Engine& engine() const { return engine_; }

private:
  friend class Engine;

  Engine& engine_;
  Gid gid_;
  std::shared_ptr<Options> options_;
  TaskState state_ = TaskState::Waiting;
  bool stopping_ = false;
  ErrorCode errorCode_ = ErrorCode::Finished;
  std::string errorMessage_;
};

} // namespace aria2
