/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/Ranges.h"

#include <algorithm>

namespace aria2::http {

void RangeSet::add(int64_t begin, int64_t end)
{
  if (begin >= end) {
    return;
  }
  auto it = ranges_.upper_bound(begin);
  if (it != ranges_.begin() && std::prev(it)->second >= begin) {
    --it;
    begin = it->first;
  }
  while (it != ranges_.end() && it->first <= end) {
    end = std::max(end, it->second);
    it = ranges_.erase(it);
  }
  ranges_.emplace(begin, end);
}

bool RangeSet::contains(int64_t begin, int64_t end) const
{
  if (begin >= end) {
    return true;
  }
  auto it = ranges_.upper_bound(begin);
  return it != ranges_.begin() && std::prev(it)->second >= end;
}

int64_t RangeSet::total() const
{
  int64_t sum = 0;
  for (const auto& [begin, end] : ranges_) {
    sum += end - begin;
  }
  return sum;
}

int64_t RangeSet::covered(int64_t begin, int64_t end) const
{
  int64_t sum = 0;
  auto it = ranges_.upper_bound(begin);
  if (it != ranges_.begin()) {
    --it;
  }
  for (; it != ranges_.end() && it->first < end; ++it) {
    sum += std::max<int64_t>(
        0, std::min(end, it->second) - std::max(begin, it->first));
  }
  return sum;
}

std::optional<Range> RangeSet::firstGap(int64_t from, int64_t limit) const
{
  auto cursor = from;
  auto it = ranges_.upper_bound(from);
  if (it != ranges_.begin() && std::prev(it)->second > from) {
    cursor = std::prev(it)->second;
  }
  if (cursor >= limit) {
    return std::nullopt;
  }
  const auto next = it == ranges_.end() ? limit : std::min(limit, it->first);
  return Range{cursor, next};
}

} // namespace aria2::http
