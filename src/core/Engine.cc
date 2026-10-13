/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "core/Engine.h"

#include "base/Log.h"
#include "base/Path.h"
#include "base/Process.h"
#include "bt/Session.h"
#include "disk/DiskPool.h"
#include "ed2k/Network.h"
#include "http/Client.h"
#include "state/Session.h"
#include "state/StateStore.h"

#include <boost/asio/post.hpp>

#include <algorithm>

namespace aria2 {

namespace asio = boost::asio;

std::string_view eventMethod(Event event)
{
  switch (event) {
  case Event::Start:
    return "aria2.onDownloadStart";
  case Event::Pause:
    return "aria2.onDownloadPause";
  case Event::Stop:
    return "aria2.onDownloadStop";
  case Event::Complete:
    return "aria2.onDownloadComplete";
  case Event::Error:
    return "aria2.onDownloadError";
  case Event::BtComplete:
    return "aria2.onBtDownloadComplete";
  }
  return {};
}

Engine::Engine(std::shared_ptr<Options> global)
    : global_(std::move(global)), ticker_(io_)
{
}

Engine::~Engine()
{
  // Tasks reference subsystems; release them first.
  active_.clear();
  waiting_.clear();
  stopped_.clear();
  tasks_.clear();
}

int Engine::run()
{
  tick();
  schedule();
  // A failing handler is a bug, but it must not take the other downloads
  // and the RPC server down with it.
  for (;;) {
    try {
      io_.run();
      break;
    }
    catch (const std::exception& error) {
      log::error("Unhandled error: {}", error.what());
    }
  }
  return exitStatus_;
}

void Engine::globalOptionsChanged(const std::vector<std::string>& names)
{
  for (const auto& name : names) {
    if (name == "max-concurrent-downloads") {
      schedule();
    }
  }
  // Running tasks that inherit a live option apply its new global value, as
  // they would apply a change of their own (changeOption).
  const auto running = active_;
  for (const auto& name : names) {
    const auto* spec = findOption(name);
    if (!spec || !(spec->scope & scope::Live)) {
      continue;
    }
    for (const auto& task : running) {
      if (!task->options().hasOwn(name)) {
        task->optionChanged(name);
      }
    }
  }
  if (http_) {
    http_->optionsChanged();
  }
  if (bt_) {
    bt_->optionsChanged(names);
  }
  if (ed2k_) {
    ed2k_->globalOptionsChanged();
  }
}

Gid Engine::add(std::shared_ptr<Task> task, std::optional<size_t> position)
{
  const auto gid = task->gid();
  tasks_[gid] = task;
  task->state_ =
      task->options().flag("pause") ? TaskState::Paused : TaskState::Waiting;
  task->options().erase("pause");
  if (task->state_ == TaskState::Paused) {
    task->loadProgress();
  }
  const auto at = std::min(position.value_or(waiting_.size()), waiting_.size());
  waiting_.insert(waiting_.begin() + static_cast<ptrdiff_t>(at), task);
  schedule();
  return gid;
}

std::shared_ptr<Task> Engine::find(Gid gid) const
{
  const auto found = tasks_.find(gid);
  return found == tasks_.end() ? nullptr : found->second;
}

std::shared_ptr<Task> Engine::find(std::string_view text) const
{
  if (const auto gid = parseGid(text)) {
    return find(*gid);
  }
  if (text.empty() || text.size() > 16 ||
      text.find_first_not_of("0123456789abcdefABCDEF") !=
          std::string_view::npos) {
    return nullptr;
  }
  std::shared_ptr<Task> match;
  for (const auto& [gid, task] : tasks_) {
    if (gidText(gid).starts_with(text)) {
      if (match) {
        return nullptr; // ambiguous prefix
      }
      match = task;
    }
  }
  return match;
}

void Engine::pause(const std::shared_ptr<Task>& task, bool force)
{
  switch (task->state_) {
  case TaskState::Waiting:
    task->state_ = TaskState::Paused;
    emit(Event::Pause, *task);
    break;
  case TaskState::Active:
    if (!task->stopping_) {
      stopTask(task, force, TaskState::Paused);
    }
    break;
  default:
    break;
  }
}

void Engine::unpause(const std::shared_ptr<Task>& task)
{
  if (task->state_ == TaskState::Paused) {
    task->state_ = TaskState::Waiting;
    schedule();
  }
}

void Engine::remove(const std::shared_ptr<Task>& task, bool force)
{
  switch (task->state_) {
  case TaskState::Waiting:
  case TaskState::Paused:
    waiting_.erase(std::remove(waiting_.begin(), waiting_.end(), task),
                   waiting_.end());
    task->errorCode_ = ErrorCode::Removed;
    task->errorMessage_ = "Removed";
    retire(task, TaskState::Removed);
    break;
  case TaskState::Active:
    if (!task->stopping_ || force) {
      stopTask(task, force, TaskState::Removed);
    }
    break;
  default:
    break;
  }
}

void Engine::pauseAll(bool force)
{
  for (const auto& task : std::vector(active_)) {
    pause(task, force);
  }
  for (const auto& task : waiting_) {
    if (task->state_ == TaskState::Waiting) {
      task->state_ = TaskState::Paused;
      emit(Event::Pause, *task);
    }
  }
}

void Engine::unpauseAll()
{
  for (const auto& task : waiting_) {
    if (task->state_ == TaskState::Paused) {
      task->state_ = TaskState::Waiting;
    }
  }
  schedule();
}

size_t Engine::move(const std::shared_ptr<Task>& task, int64_t offset,
                    Position how)
{
  const auto found = std::find(waiting_.begin(), waiting_.end(), task);
  if (found == waiting_.end()) {
    fail(ErrorCode::Unknown, "GID {} is not in the waiting queue",
         gidText(task->gid()));
  }
  const auto current = static_cast<int64_t>(found - waiting_.begin());
  waiting_.erase(found);
  const auto size = static_cast<int64_t>(waiting_.size());
  int64_t target = how == Position::Set       ? offset
                   : how == Position::Current ? current + offset
                                              : size + offset;
  target = std::clamp<int64_t>(target, 0, size);
  waiting_.insert(waiting_.begin() + target, task);
  return static_cast<size_t>(target);
}

bool Engine::removeResult(Gid gid)
{
  const auto found =
      std::find_if(stopped_.begin(), stopped_.end(),
                   [gid](const auto& task) { return task->gid() == gid; });
  if (found == stopped_.end()) {
    return false;
  }
  (*found)->discardState();
  tasks_.erase(gid);
  stopped_.erase(found);
  return true;
}

void Engine::requeue(const std::shared_ptr<Task>& task)
{
  const auto found = std::find(stopped_.begin(), stopped_.end(), task);
  if (found == stopped_.end()) {
    fail(ErrorCode::Unknown, "GID {} is not a stopped task",
         gidText(task->gid()));
  }
  stopped_.erase(found);
  task->state_ = TaskState::Waiting;
  task->errorCode_ = ErrorCode::Finished;
  task->errorMessage_.clear();
  waiting_.push_back(task);
  schedule();
}

void Engine::handOver(const std::shared_ptr<Task>& current,
                      std::shared_ptr<Task> next)
{
  const auto found = std::find(active_.begin(), active_.end(), current);
  if (found == active_.end() || next->gid() != current->gid()) {
    return;
  }
  *found = next;
  tasks_[next->gid()] = next;
  next->state_ = TaskState::Active;
  next->following = current->following;
  next->belongsTo = current->belongsTo;
  current->state_ = TaskState::Removed;
  try {
    next->start();
  }
  catch (const Error& error) {
    next->failed(error.code(), error.what());
  }
  catch (const std::exception& error) {
    next->failed(ErrorCode::Unknown, error.what());
  }
}

void Engine::purgeResults()
{
  for (const auto& task : stopped_) {
    task->discardState();
    tasks_.erase(task->gid());
  }
  stopped_.clear();
}

std::vector<std::shared_ptr<Task>> Engine::active() const { return active_; }

bool Engine::writing(const std::string& path, Gid except) const
{
  for (const auto& [gid, task] : tasks_) {
    const auto state = task->state();
    if (gid == except || state == TaskState::Complete ||
        state == TaskState::Error || state == TaskState::Removed) {
      continue;
    }
    for (const auto& file : task->files()) {
      if (!file.path.empty() && samePath(file.path, path)) {
        return true;
      }
    }
  }
  return false;
}

GlobalStat Engine::stat() const
{
  GlobalStat result;
  for (const auto& task : active_) {
    result.downloadSpeed += task->downloadSpeed();
    result.uploadSpeed += task->uploadSpeed();
  }
  result.numActive = active_.size();
  result.numWaiting = waiting_.size();
  result.numStopped = stopped_.size();
  result.numStoppedTotal = stoppedTotal_;
  return result;
}

void Engine::emit(Event event, const Task& task)
{
  for (const auto& listener : listeners_) {
    listener(event, task);
  }
}

void Engine::schedule()
{
  if (!scheduled_) {
    scheduled_ = true;
    asio::post(io_, [this] {
      scheduled_ = false;
      fill();
    });
  }
}

size_t Engine::activeCount() const
{
  return std::count_if(active_.begin(), active_.end(), [](const auto& task) {
    return task->countsTowardConcurrency();
  });
}

void Engine::fill()
{
  if (shuttingDown_) {
    return;
  }
  const auto limit =
      static_cast<size_t>(global_->integer("max-concurrent-downloads"));
  for (auto it = waiting_.begin();
       it != waiting_.end() && activeCount() < limit;) {
    if ((*it)->state_ != TaskState::Waiting) {
      ++it;
      continue;
    }
    auto task = *it;
    it = waiting_.erase(it);
    startTask(task);
  }
  const bool queued =
      std::any_of(waiting_.begin(), waiting_.end(), [](const auto& task) {
        return task->state_ == TaskState::Waiting;
      });
  if (active_.empty() && !queued && !global_->flag("enable-rpc")) {
    shutdown(false); // command-line mode ends when the queue drains
  }
}

void Engine::startTask(const std::shared_ptr<Task>& task)
{
  task->state_ = TaskState::Active;
  active_.push_back(task);
  emit(Event::Start, *task);
  try {
    task->start();
  }
  catch (const Error& error) {
    task->failed(error.code(), error.what());
  }
  catch (const std::exception& error) {
    task->failed(ErrorCode::Unknown, error.what());
  }
}

void Engine::stopTask(const std::shared_ptr<Task>& task, bool force,
                      TaskState next)
{
  task->stopping_ = true;
  task->stop(force, [this, task, next] {
    task->stopping_ = false;
    task->checkpoint();
    active_.erase(std::remove(active_.begin(), active_.end(), task),
                  active_.end());
    switch (next) {
    case TaskState::Paused:
    case TaskState::Waiting:
      task->state_ = next;
      waiting_.push_front(task);
      if (next == TaskState::Paused) {
        emit(Event::Pause, *task);
      }
      break;
    default:
      task->errorCode_ = ErrorCode::Removed;
      task->errorMessage_ = "Removed";
      retire(task, TaskState::Removed);
      break;
    }
    if (shuttingDown_ && active_.empty()) {
      finishShutdown();
    }
    schedule();
  });
}

void Engine::retire(const std::shared_ptr<Task>& task, TaskState state)
{
  task->state_ = state;
  active_.erase(std::remove(active_.begin(), active_.end(), task),
                active_.end());
  stopped_.push_back(task);
  ++stoppedTotal_;
  if (state == TaskState::Removed) {
    task->discardState();
  }
  emit(state == TaskState::Complete ? Event::Complete
       : state == TaskState::Error  ? Event::Error
                                    : Event::Stop,
       *task);
  const auto limit =
      static_cast<size_t>(global_->integer("max-download-result"));
  const bool keepUnfinished = global_->flag("keep-unfinished-download-result");
  for (auto it = stopped_.begin();
       stopped_.size() > limit && it != stopped_.end();) {
    if (keepUnfinished && (*it)->state_ != TaskState::Complete) {
      ++it;
      continue;
    }
    tasks_.erase((*it)->gid());
    it = stopped_.erase(it);
  }
  schedule();
}

void Engine::taskCompleted(Task& task)
{
  auto self = task.shared_from_this();
  task.checkpoint();
  if (follower_) {
    follower_(task);
  }
  // Defer so the task can finish its own callback stack.
  asio::post(io_, [this, self] { retire(self, TaskState::Complete); });
}

void Engine::taskFailed(Task& task)
{
  auto self = task.shared_from_this();
  task.checkpoint();
  log::notice("Download {} failed: {}", gidText(task.gid()),
              task.errorMessage());
  exitStatus_ = static_cast<int>(task.errorCode());
  asio::post(io_, [this, self] { retire(self, TaskState::Error); });
}

void Engine::tick()
{
  log::flush();
  const auto now = Clock::now();
  const auto checkpointEvery =
      std::chrono::seconds(global_->integer("state-save-interval"));
  if (now - lastCheckpoint_ >= checkpointEvery) {
    lastCheckpoint_ = now;
    for (const auto& task : active_) {
      task->checkpoint();
    }
  }
  const auto sessionEvery = global_->integer("save-session-interval");
  if (sessionEvery > 0 &&
      now - lastSessionSave_ >= std::chrono::seconds(sessionEvery)) {
    lastSessionSave_ = now;
    saveSession();
  }
  const auto stopAfter = global_->integer("stop");
  if (stopAfter > 0 && now - started_ >= std::chrono::seconds(stopAfter)) {
    shutdown(false);
  }
  const auto parent = global_->integer("stop-with-process");
  if (parent > 0 && !processAlive(parent)) {
    log::notice("Process {} ended; shutting down", parent);
    shutdown(false);
  }
  ticker_.expires_after(std::chrono::seconds(1));
  ticker_.async_wait([this](const boost::system::error_code& error) {
    if (!error && !io_.stopped()) {
      tick();
    }
  });
}

void Engine::shutdown(bool force)
{
  if (shuttingDown_ && !(force && !forceShutdown_)) {
    return;
  }
  shuttingDown_ = true;
  forceShutdown_ = forceShutdown_ || force;
  log::notice("Shutting down");
  if (active_.empty()) {
    asio::post(io_, [this] { finishShutdown(); });
    return;
  }
  for (const auto& task : std::vector(active_)) {
    if (!task->stopping_ || force) {
      stopTask(task, force, TaskState::Waiting);
    }
  }
}

void Engine::finishShutdown()
{
  if (finished_) {
    return;
  }
  finished_ = true;
  saveSession();
  if (bt_) {
    bt_->close();
  }
  // The ED2K network removes its port mappings before the loop stops.
  if (ed2k_) {
    ed2k_->close([this] { stopLoop(); });
    return;
  }
  stopLoop();
}

void Engine::stopLoop()
{
  if (http_) {
    http_->close();
  }
  ticker_.cancel();
  if (disk_) {
    disk_->drain();
  }
  io_.stop();
}

bool Engine::saveSession()
{
  const auto path = global_->get("save-session");
  if (path.empty()) {
    return true;
  }
  std::vector<std::shared_ptr<Task>> tasks(active_.begin(), active_.end());
  tasks.insert(tasks.end(), waiting_.begin(), waiting_.end());
  tasks.insert(tasks.end(), stopped_.begin(), stopped_.end());
  if (!writeFileAtomically(path, serializeSession(tasks))) {
    log::error("Could not write the session file {}", path);
    return false;
  }
  return true;
}

DiskPool& Engine::disk()
{
  if (!disk_) {
    disk_ = std::make_unique<DiskPool>(io_);
  }
  return *disk_;
}

StateStore& Engine::state()
{
  if (!state_) {
    state_ = std::make_unique<StateStore>(*this);
  }
  return *state_;
}

http::Client& Engine::http()
{
  if (!http_) {
    http_ = std::make_unique<http::Client>(*this);
  }
  return *http_;
}

bt::Session& Engine::bt()
{
  if (!bt_) {
    bt_ = std::make_unique<bt::Session>(*this);
  }
  return *bt_;
}

ed2k::Network& Engine::ed2k()
{
  if (!ed2k_) {
    ed2k_ = std::make_unique<ed2k::Network>(*this);
  }
  return *ed2k_;
}

} // namespace aria2
