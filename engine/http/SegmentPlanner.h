/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "http/Ranges.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace aria2::http {

inline constexpr int64_t unbounded = std::numeric_limits<int64_t>::max();

// Decides which byte range each connection downloads. It knows nothing about
// sockets: the task reports bytes and outcomes, the planner answers with
// assignments, extensions and cancellations.
class SegmentPlanner {
public:
  struct Worker {
    int64_t position = 0;   // next byte this worker will receive
    int64_t end = 0;        // assigned end, exclusive; may shrink
    int64_t requestEnd = 0; // end the request asked for, exclusive
    bool receiving = false; // has received payload bytes
    double speed = 0;       // bytes per second, smoothed
    double age = 0;         // seconds of speed samples since assignment
  };

  // `minSplit`: a range is split only if both halves get at least this much.
  explicit SegmentPlanner(int64_t minSplit) : minSplit_(minSplit) {}

  // Also clamps workers placed before the length was known.
  void setLength(int64_t length);
  int64_t length() const { return length_; }
  const RangeSet& received() const { return received_; }
  void restore(const std::vector<Range>& ranges);
  void reset();

  // Range for a new connection, or nothing when no work is worth starting.
  // The returned range is registered as worker `id`.
  std::optional<Range> assign(int id);
  // Registers a worker for an explicit request (first request, resume).
  void place(int id, int64_t begin, int64_t requestEnd);

  // Records received bytes and returns how many of them belong to the worker's
  // assignment; the rest must be discarded.
  int64_t receive(int id, int64_t bytes);
  // Called periodically with the rate over the last `seconds`.
  void updateSpeed(int id, double bytesPerSecond, double seconds);
  // The average rate of a finished connection, which may have been too short
  // for periodic samples.
  void noteSpeed(double bytesPerSecond)
  {
    peakSpeed_ = std::max(peakSpeed_, bytesPerSecond);
  }

  // After a worker reached its end: if its request continues into a range of a
  // worker that has not received anything, it takes that range over and the
  // returned id must be cancelled. Returns false when the worker is finished.
  bool extend(int id, std::optional<int>& cancelled);
  // Workers whose remaining range is already covered by others (race losers).
  std::vector<int> redundant() const;

  void release(int id) { workers_.erase(id); }
  const Worker* worker(int id) const;
  const std::map<int, Worker>& workers() const { return workers_; }
  bool complete() const;

private:
  std::optional<Range> largestGap() const;
  std::optional<Range> split(int id);
  std::optional<Range> race(int id);
  double typicalSpeed(int except = -1) const;

  int64_t minSplit_;
  int64_t length_ = -1;
  RangeSet received_;
  std::map<int, Worker> workers_;
  double peakSpeed_ = 0; // fastest smoothed connection speed seen
};

} // namespace aria2::http
