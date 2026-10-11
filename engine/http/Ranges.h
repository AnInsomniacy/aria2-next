/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace aria2::http {

using Range = std::pair<int64_t, int64_t>; // [begin, end)

// Disjoint, coalesced byte ranges.
class RangeSet {
public:
  void add(int64_t begin, int64_t end);
  void remove(int64_t begin, int64_t end);
  void clear() { ranges_.clear(); }
  bool contains(int64_t begin, int64_t end) const;
  int64_t total() const;
  // Bytes of [begin, end) that are present.
  int64_t covered(int64_t begin, int64_t end) const;
  // First missing range inside [from, limit), if any.
  std::optional<Range> firstGap(int64_t from, int64_t limit) const;
  std::vector<Range> ranges() const { return {ranges_.begin(), ranges_.end()}; }
  bool empty() const { return ranges_.empty(); }

private:
  std::map<int64_t, int64_t> ranges_; // begin -> end
};

} // namespace aria2::http
