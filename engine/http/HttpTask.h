/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Task.h"
#include "http/SegmentPlanner.h"

#include <boost/asio/steady_timer.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aria2 {
class Output;
}

namespace aria2::http {

class Worker;

// An HTTP, HTTPS or SFTP download over one or more mirror URIs. See
// engine/DESIGN.md for the scheduling model.
class HttpTask final : public Task {
public:
  HttpTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
           std::vector<std::string> uris);
  ~HttpTask() override;

  void start() override;
  void stop(bool force, std::function<void()> done) override;

  int64_t totalLength() const override;
  int64_t completedLength() const override;
  int64_t downloadSpeed() const override;
  int connections() const override;
  std::vector<TaskFile> files() const override;
  void status(boost::json::object& out, const StatusKeys& keys) const override;
  std::vector<std::string> sessionUris() const override;
  void checkpoint() override;
  void discardState() override;
  void optionChanged(std::string_view name) override;
  bool changeUris(int fileIndex, const std::vector<std::string>& remove,
                  const std::vector<std::string>& add, int position) override;

  const std::string& path() const { return path_; }
  const std::string& contentType() const { return contentType_; }

private:
  friend class Worker;

  struct Source {
    std::string uri;
    std::string redirect; // last validated redirect destination
    std::string host;
    int active = 0;
    int notFound = 0;
    bool dead = false;
    bool used = false;
  };

  // A connection position: workers come and go, a slot keeps its failure
  // count so a persistently failing connection stops retrying.
  struct Slot {
    std::shared_ptr<Worker> worker;
    int failures = 0;
    Clock::time_point readyAt{};
  };

  enum class Phase { Idle, Probing, Opening, Running, Finishing, Stopping, Done };

  void launch();
  void startWorker(int id, size_t slot, size_t source, int64_t begin,
                   int64_t requestEnd);
  std::optional<size_t> pickSource();
  void accept(Worker& worker);
  size_t receive(Worker& worker, const char* data, size_t size);
  void finished(Worker& worker, int curlCode);
  void failedResponse(Worker& worker, int curlCode);
  bool rangesIgnored(Worker& worker);
  void reset();
  void retireWorker(Worker& worker);
  void slotFailed(size_t slot, ErrorCode code, std::string message,
                  Clock::duration wait = {});
  void cancelWorker(int id);
  void flush(Worker& worker);
  void write(int64_t offset, std::string data);
  void written(int64_t offset, size_t size, std::error_code error);
  void openOutput(bool truncate);
  void outputReady(std::error_code error);
  bool decidePath(Worker& worker);
  void changed(const std::string& why);
  void abort(ErrorCode code, std::string message);
  void tryFinish();
  void verifyAndComplete();
  void tick();
  void scheduleTick();
  void quiesce();
  bool parallel() const;
  int connectionTarget() const;

  std::vector<Source> sources_;
  std::vector<Slot> slots_;
  std::map<int, std::shared_ptr<Worker>> workers_;
  int nextWorkerId_ = 0;
  SegmentPlanner planner_;
  RangeSet persisted_;
  std::shared_ptr<Output> output_;
  // Data that arrived before the output was open, keyed by offset.
  std::vector<std::pair<int64_t, std::string>> held_;
  std::string path_;
  std::string etag_;
  std::string lastModified_;
  std::string contentType_;
  int64_t remoteTime_ = -1;
  bool owned_ = false;   // the output file belongs to this task
  bool ranged_ = true;   // the server honours byte ranges
  bool single_ = false;  // one connection only
  int restarts_ = 0;
  int cap_ = 1;          // connections allowed after overload feedback
  Clock::time_point calmSince_ = Clock::now();
  Clock::time_point holdUntil_{}; // Retry-After
  Phase phase_ = Phase::Idle;
  ErrorCode lastError_ = ErrorCode::Network;
  std::string lastMessage_;
  size_t pendingWrites_ = 0;
  int64_t unflushed_ = 0;
  mutable SpeedMeter speed_;
  RateLimit limit_;
  boost::asio::steady_timer ticker_;
  std::function<void()> stopped_;
  bool tickScheduled_ = false;
};

} // namespace aria2::http
