/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Task.h"
#include "media/Control.h"

#include <boost/asio/steady_timer.hpp>

#include <memory>
#include <string>

namespace aria2::media {

class Session;

// One HLS/DASH presentation remuxed into a single output file. The GPAC and
// FFmpeg pipeline runs on a worker thread (MediaJob) with a frozen copy of
// the task options; this task polls its Control for progress and outcome.
// Progress survives restarts in the media state store.
class MediaTask final : public Task {
public:
  MediaTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
            std::string uri);
  ~MediaTask() override;

  // Whether a URI is handled as media under the `media` option.
  static bool handles(const std::string& uri, const Options& options);
  // Whether a response Content-Type identifies an HLS or DASH manifest.
  static bool manifestType(std::string_view contentType);

  void start() override;
  void stop(bool force, std::function<void()> done) override;

  int64_t totalLength() const override;
  int64_t completedLength() const override;
  int64_t downloadSpeed() const override;
  int connections() const override;
  std::vector<TaskFile> files() const override;
  void status(boost::json::object& out, const StatusKeys& keys) const override;
  std::vector<std::string> sessionUris() const override { return {uri_}; }
  void discardState() override;

  // Ends a live recording after its current segment. A paused recording
  // finishes when it is next started. False when the task is not live.
  bool finishRecording();
  bool failedMedia() const { return snapshot_.state == "error"; }

private:
  void restore();
  std::string outputPath() const;
  int64_t downloadLimit() const;
  void schedulePoll();
  void poll();
  void workerDone();

  std::string uri_;
  std::string directory_; // media state directory
  Snapshot snapshot_;
  std::shared_ptr<Control> control_;
  std::unique_ptr<Session> session_;
  boost::asio::steady_timer timer_;
  std::function<void()> stopped_;
  mutable SpeedMeter speed_;
  int64_t accounted_ = 0;
  bool finishRequested_ = false;
};

} // namespace aria2::media
