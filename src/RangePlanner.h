/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_RANGE_PLANNER_H
#define D_RANGE_PLANNER_H

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

namespace aria2 {

struct RangeLease {
  int64_t begin = 0;
  int64_t end = 0;
  size_t attempts = 0;
  size_t uriIndex = 0;

  bool empty() const { return begin >= end; }
  int64_t length() const { return end - begin; }

  RangeLease remainder(int64_t offset) const
  {
    auto result = *this;
    result.begin = std::clamp(offset, begin, end);
    return result;
  }
};

class RangePlanner {
public:
  using TimePoint = std::chrono::steady_clock::time_point;
  using StoredRange = std::pair<int64_t, int64_t>;

  void clear();
  // Restored ranges belong to a representation of a known total length;
  // completion must not depend on a later response re-declaring it.
  void restore(const std::vector<StoredRange>& ranges, int64_t totalLength);
  void configure(int64_t totalLength, int64_t chunkSize,
                 const std::vector<RangeLease>& active);
  void commit(int64_t begin, int64_t end);

  int64_t totalLength() const { return totalLength_; }
  int64_t completedLength() const;
  int64_t contiguousLength() const;
  int64_t gapEnd(int64_t begin, int64_t proposedEnd) const;
  bool complete() const;
  const std::vector<StoredRange>& completedRanges() const { return completed_; }

  void enqueue(RangeLease lease);
  void defer(RangeLease lease, TimePoint readyAt);
  std::optional<RangeLease> takeReady(TimePoint now);
  std::optional<TimePoint> nextDeadline() const;
  bool hasReady(TimePoint now) const;
  bool hasPending() const { return !ready_.empty() || !deferred_.empty(); }

  size_t refillReady(size_t targetCount, int64_t preferredPieceSize,
                     int64_t minimumPieceSize);

private:
  struct DeferredLease {
    RangeLease lease;
    TimePoint readyAt;
  };

  void normalizeCompleted();
  void enqueueGap(int64_t begin, int64_t end);
  void releaseDeferred(TimePoint now);

  int64_t totalLength_ = 0;
  int64_t chunkSize_ = 0;
  std::vector<StoredRange> completed_;
  std::deque<RangeLease> ready_;
  std::vector<DeferredLease> deferred_;
};

} // namespace aria2

#endif // D_RANGE_PLANNER_H
