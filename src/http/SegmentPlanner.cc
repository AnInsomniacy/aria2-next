/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/SegmentPlanner.h"

#include <algorithm>
#include <map>

namespace aria2::http {
namespace {
// Splits land on page boundaries so writes stay aligned.
constexpr int64_t alignment = 4096;
// A connection is raced only when it would otherwise hold the download back.
constexpr double raceSeconds = 2.0;
// Copies of one range, including the original.
constexpr int maxCopies = 3;
} // namespace

void SegmentPlanner::setLength(int64_t length)
{
  length_ = length;
  for (auto& [id, worker] : workers_) {
    worker.end = std::min(worker.end, length);
    worker.requestEnd = std::min(worker.requestEnd, length);
  }
}

void SegmentPlanner::restore(const std::vector<Range>& ranges)
{
  for (const auto& [begin, end] : ranges) {
    received_.add(begin, end);
  }
}

void SegmentPlanner::reset()
{
  received_.clear();
  workers_.clear();
  length_ = -1;
  peakSpeed_ = 0;
}

void SegmentPlanner::place(int id, int64_t begin, int64_t requestEnd)
{
  const auto end = length_ >= 0 ? std::min(requestEnd, length_) : requestEnd;
  workers_[id] = Worker{begin, end, requestEnd, false, 0};
}

const SegmentPlanner::Worker* SegmentPlanner::worker(int id) const
{
  const auto found = workers_.find(id);
  return found == workers_.end() ? nullptr : &found->second;
}

bool SegmentPlanner::complete() const
{
  return length_ >= 0 && received_.contains(0, length_);
}

double SegmentPlanner::typicalSpeed(int except) const
{
  double sum = 0;
  int count = 0;
  for (const auto& [id, worker] : workers_) {
    if (id != except && worker.receiving && worker.speed > 0) {
      sum += worker.speed;
      ++count;
    }
  }
  return count ? sum / count : 0;
}

std::optional<Range> SegmentPlanner::largestGap() const
{
  if (length_ < 0) {
    return std::nullopt;
  }
  auto owned = received_;
  for (const auto& [id, worker] : workers_) {
    owned.add(worker.position, std::min(worker.end, length_));
  }
  std::optional<Range> best;
  for (auto gap = owned.firstGap(0, length_); gap;
       gap = owned.firstGap(gap->second, length_)) {
    if (!best || gap->second - gap->first > best->second - best->first) {
      best = gap;
    }
  }
  return best;
}

std::optional<Range> SegmentPlanner::assign(int id)
{
  if (length_ < 0) {
    return std::nullopt;
  }
  if (const auto gap = largestGap()) {
    workers_[id] = Worker{gap->first, gap->second, gap->second, false, 0};
    return gap;
  }
  if (auto range = split(id)) {
    return range;
  }
  return race(id);
}

std::optional<Range> SegmentPlanner::split(int id)
{
  const auto typical = typicalSpeed();
  Worker* victim = nullptr;
  double longest = 0;
  for (auto& [other, worker] : workers_) {
    const auto remaining = worker.end - worker.position;
    if (worker.end == unbounded || remaining < 2 * minSplit_) {
      continue;
    }
    const auto speed = worker.speed > 0 ? worker.speed : typical;
    // Unknown speeds compare by size; any measured speed beats none.
    const auto time = speed > 0 ? remaining / speed : remaining * 1e-3;
    if (!victim || time > longest) {
      victim = &worker;
      longest = time;
    }
  }
  if (!victim) {
    return std::nullopt;
  }
  auto& target = *victim;
  const auto remaining = target.end - target.position;
  const auto own = target.speed > 0 ? target.speed : typical;
  const auto fresh = typical > 0 ? typical : own;
  const auto share = own > 0 && fresh > 0 ? own / (own + fresh) : 0.5;
  auto cut = target.position + static_cast<int64_t>(remaining * share);
  cut = std::clamp(cut, target.position + minSplit_, target.end - minSplit_);
  if (cut - cut % alignment > target.position + minSplit_) {
    cut -= cut % alignment;
  }
  const Range range{cut, target.end};
  target.end = cut;
  workers_[id] = Worker{range.first, range.second, range.second, false, 0};
  return range;
}

// The endgame: a range that would hold the download back gets another copy;
// the first copy to finish wins. A copy that turns out slow as well (another
// bad route) may be raced again, up to maxCopies.
std::optional<Range> SegmentPlanner::race(int id)
{
  struct Group {
    int leader = -1; // the copy with the earliest expected finish
    double time = 0;
    int copies = 0;
    bool young = false;
  };
  std::map<int64_t, Group> groups; // by end
  for (const auto& [other, worker] : workers_) {
    const auto remaining = worker.end - worker.position;
    if (remaining <= 0 || worker.end == unbounded) {
      continue;
    }
    auto& group = groups[worker.end];
    ++group.copies;
    // A new connection gets time to connect and show its speed.
    group.young = group.young || worker.age < raceSeconds;
    const auto speed = worker.receiving ? worker.speed : 0;
    const auto time = speed > 0 ? static_cast<double>(remaining) / speed : 1e9;
    if (group.leader < 0 || time < group.time) {
      group.leader = other;
      group.time = time;
    }
  }
  const Worker* slowest = nullptr;
  double longest = raceSeconds;
  for (const auto& [end, group] : groups) {
    if (group.young || group.copies >= maxCopies || group.time <= longest) {
      continue;
    }
    const auto& leader = workers_.at(group.leader);
    const auto speed = leader.receiving ? leader.speed : 0;
    // Raced only when it is markedly slower than its siblings or than other
    // connections of this download managed before.
    const auto reference = std::max(typicalSpeed(group.leader), peakSpeed_);
    if (reference <= 0 || speed < reference / 2) {
      slowest = &leader;
      longest = group.time;
    }
  }
  if (!slowest) {
    return std::nullopt;
  }
  const Range range{slowest->position, slowest->end};
  workers_[id] = Worker{range.first, range.second, range.second, false, 0};
  return range;
}

int64_t SegmentPlanner::receive(int id, int64_t bytes)
{
  auto& worker = workers_.at(id);
  const auto usable =
      std::clamp<int64_t>(worker.end - worker.position, 0, bytes);
  received_.add(worker.position, worker.position + usable);
  worker.position += usable;
  worker.receiving = true;
  return usable;
}

void SegmentPlanner::updateSpeed(int id, double bytesPerSecond, double seconds)
{
  if (const auto found = workers_.find(id); found != workers_.end()) {
    found->second.age += seconds;
    auto& speed = found->second.speed;
    speed = speed > 0 ? speed * 0.6 + bytesPerSecond * 0.4 : bytesPerSecond;
    peakSpeed_ = std::max(peakSpeed_, speed);
  }
}

bool SegmentPlanner::extend(int id, std::optional<int>& cancelled)
{
  auto& worker = workers_.at(id);
  if (worker.requestEnd <= worker.end ||
      (length_ >= 0 && worker.end >= length_)) {
    return false;
  }
  // Take over the next range if its connection has not produced data yet.
  for (auto& [other, next] : workers_) {
    if (other != id && next.position == worker.end && !next.receiving) {
      worker.end = std::min(worker.requestEnd, next.end);
      cancelled = other;
      workers_.erase(other);
      return true;
    }
  }
  // Or continue into unowned bytes directly after it.
  auto owned = received_;
  for (const auto& [other, next] : workers_) {
    if (other != id) {
      owned.add(next.position, next.end);
    }
  }
  const auto limit =
      length_ >= 0 ? std::min(worker.requestEnd, length_) : worker.requestEnd;
  if (const auto gap = owned.firstGap(worker.end, limit);
      gap && gap->first == worker.end) {
    worker.end = gap->second;
    return true;
  }
  return false;
}

std::vector<int> SegmentPlanner::redundant() const
{
  std::vector<int> result;
  for (const auto& [id, worker] : workers_) {
    if (worker.position < worker.end &&
        received_.contains(worker.position, worker.end)) {
      result.push_back(id);
    }
  }
  return result;
}

} // namespace aria2::http
