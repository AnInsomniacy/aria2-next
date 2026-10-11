/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Task.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aria2 {

class DiskPool;
class StateStore;
namespace http {
class Client;
}
namespace bt {
class Session;
}
namespace ed2k {
class Network;
}

enum class Event { Start, Pause, Stop, Complete, Error, BtComplete };
std::string_view eventMethod(Event event); // "aria2.onDownloadStart", ...

struct GlobalStat {
  int64_t downloadSpeed = 0;
  int64_t uploadSpeed = 0;
  size_t numActive = 0;
  size_t numWaiting = 0;
  size_t numStopped = 0;
  size_t numStoppedTotal = 0;
};

enum class Position { Set, Current, End };

// Owns the event loop, the task queue and the protocol subsystems. Every
// method runs on the control thread.
class Engine {
public:
  explicit Engine(std::shared_ptr<Options> global);
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  boost::asio::io_context& io() { return io_; }
  Options& global() { return *global_; }
  const std::shared_ptr<Options>& sharedGlobal() const { return global_; }
  // Option changes that need subsystem reconfiguration.
  void globalOptionsChanged(const std::vector<std::string>& names);

  // Runs until shutdown completes. Returns the process exit status.
  int run();
  void shutdown(bool force);
  bool shuttingDown() const { return shuttingDown_; }

  Gid add(std::shared_ptr<Task> task, std::optional<size_t> position = {});
  std::shared_ptr<Task> find(Gid gid) const;
  // Accepts a full GID or a unique prefix.
  std::shared_ptr<Task> find(std::string_view text) const;

  void pause(const std::shared_ptr<Task>& task, bool force);
  void unpause(const std::shared_ptr<Task>& task);
  void remove(const std::shared_ptr<Task>& task, bool force);
  void pauseAll(bool force);
  void unpauseAll();
  size_t move(const std::shared_ptr<Task>& task, int64_t offset, Position how);
  bool removeResult(Gid gid);
  // Moves a stopped task back to the end of the queue, keeping its GID.
  void requeue(const std::shared_ptr<Task>& task);
  // An active task gives its slot to a task of another kind with the same
  // GID, e.g. HTTP to media once a response reveals a manifest.
  void handOver(const std::shared_ptr<Task>& current,
                std::shared_ptr<Task> next);
  void purgeResults();

  // Whether an unfinished task other than `except` writes, or will write,
  // the file at `path`.
  bool writing(const std::string& path, Gid except) const;

  std::vector<std::shared_ptr<Task>> active() const;
  const std::deque<std::shared_ptr<Task>>& waiting() const { return waiting_; }
  const std::deque<std::shared_ptr<Task>>& stopped() const { return stopped_; }
  GlobalStat stat() const;

  using Listener = std::function<void(Event, const Task&)>;
  void listen(Listener listener) { listeners_.push_back(std::move(listener)); }
  void emit(Event event, const Task& task);

  // Writes the session file now (also done periodically and on exit).
  bool saveSession();
  // Tasks created after a download completes, such as a fetched .torrent.
  using Follower = std::function<void(Task& finished)>;
  void setFollower(Follower follower) { follower_ = std::move(follower); }

  DiskPool& disk();
  StateStore& state();
  http::Client& http();
  bt::Session& bt();
  ed2k::Network& ed2k();
  bool btStarted() const { return static_cast<bool>(bt_); }
  bool ed2kStarted() const { return static_cast<bool>(ed2k_); }

  // Called by Task.
  void taskCompleted(Task& task);
  void taskFailed(Task& task);

private:
  void schedule();
  void fill();
  void startTask(const std::shared_ptr<Task>& task);
  void stopTask(const std::shared_ptr<Task>& task, bool force, TaskState next);
  void retire(const std::shared_ptr<Task>& task, TaskState state);
  void tick();
  void finishShutdown();
  void stopLoop();
  size_t activeCount() const;

  boost::asio::io_context io_;
  std::shared_ptr<Options> global_;
  std::map<Gid, std::shared_ptr<Task>> tasks_;
  std::vector<std::shared_ptr<Task>> active_;
  std::deque<std::shared_ptr<Task>> waiting_;
  std::deque<std::shared_ptr<Task>> stopped_;
  size_t stoppedTotal_ = 0;
  std::vector<Listener> listeners_;
  Follower follower_;
  boost::asio::steady_timer ticker_;
  bool scheduled_ = false;
  bool shuttingDown_ = false;
  bool forceShutdown_ = false;
  bool finished_ = false;
  int exitStatus_ = 0;
  Clock::time_point started_ = Clock::now();
  Clock::time_point lastSessionSave_ = Clock::now();
  Clock::time_point lastCheckpoint_ = Clock::now();

  std::unique_ptr<DiskPool> disk_;
  std::unique_ptr<StateStore> state_;
  std::unique_ptr<http::Client> http_;
  std::unique_ptr<bt::Session> bt_;
  std::unique_ptr<ed2k::Network> ed2k_;
};

} // namespace aria2
