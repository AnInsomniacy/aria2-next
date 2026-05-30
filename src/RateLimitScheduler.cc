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
#include "RateLimitScheduler.h"

#include <algorithm>

namespace aria2 {

namespace {
int64_t sanitizeLimit(int64_t limit) { return std::max<int64_t>(0, limit); }

int64_t floorShare(int64_t globalLimit, int64_t baseShare)
{
  const auto oneTenth = std::max<int64_t>(1, globalLimit / 10);
  const auto floor = std::max<int64_t>(64 * 1024, oneTenth);
  return std::min(baseShare, floor);
}
} // namespace

void RateLimitScheduler::setGlobalLimit(RateLimitDirection direction,
                                        int64_t limit)
{
  globalLimits_[index(direction)] = sanitizeLimit(limit);
}

void RateLimitScheduler::setBackendCap(RateLimitBackend backend,
                                       RateLimitDirection direction,
                                       int64_t limit)
{
  states_[index(direction)][index(backend)].cap = sanitizeLimit(limit);
}

void RateLimitScheduler::setActive(RateLimitBackend backend,
                                   RateLimitDirection direction, bool active)
{
  states_[index(direction)][index(backend)].active = active;
}

void RateLimitScheduler::setObservedSpeed(RateLimitBackend backend,
                                          RateLimitDirection direction,
                                          int64_t speed)
{
  states_[index(direction)][index(backend)].observedSpeed =
      std::max<int64_t>(0, speed);
  states_[index(direction)][index(backend)].observed = true;
}

void RateLimitScheduler::clearObservedSpeeds()
{
  for (auto& direction : states_) {
    for (auto& state : direction) {
      state.observedSpeed = 0;
      state.observed = false;
    }
  }
}

void RateLimitScheduler::recalculate()
{
  recalculateDirection(RateLimitDirection::Download);
  recalculateDirection(RateLimitDirection::Upload);
}

int64_t RateLimitScheduler::backendLimit(RateLimitBackend backend,
                                         RateLimitDirection direction) const
{
  return states_[index(direction)][index(backend)].assignedLimit;
}

size_t RateLimitScheduler::index(RateLimitBackend backend)
{
  return static_cast<size_t>(backend);
}

size_t RateLimitScheduler::index(RateLimitDirection direction)
{
  return static_cast<size_t>(direction);
}

void RateLimitScheduler::recalculateDirection(RateLimitDirection direction)
{
  auto& states = states_[index(direction)];
  const auto globalLimit = globalLimits_[index(direction)];
  size_t activeCount = 0;
  for (const auto& state : states) {
    if (state.active) {
      ++activeCount;
    }
  }
  for (auto& state : states) {
    state.assignedLimit = 0;
  }
  if (activeCount == 0 || globalLimit == 0) {
    return;
  }

  const auto baseShare = std::max<int64_t>(
      1, globalLimit / static_cast<int64_t>(activeCount));
  int64_t used = 0;
  for (auto& state : states) {
    if (!state.active) {
      continue;
    }
    const auto cap = state.cap == 0 ? globalLimit : state.cap;
    auto assigned = std::min(baseShare, cap);
    if (activeCount > 1 && state.observed &&
        state.observedSpeed < baseShare / 2) {
      assigned = std::min<int64_t>(assigned,
                                   floorShare(globalLimit, baseShare));
    }
    state.assignedLimit = assigned;
    used += assigned;
  }

  auto remaining = std::max<int64_t>(0, globalLimit - used);
  size_t hungryCount = 0;
  for (const auto& state : states) {
    if (!state.active) {
      continue;
    }
    const auto cap = state.cap == 0 ? globalLimit : state.cap;
    if (state.assignedLimit < cap &&
        (!state.observed || state.observedSpeed >= state.assignedLimit / 2)) {
      ++hungryCount;
    }
  }
  while (remaining > 0 && hungryCount > 0) {
    bool assignedAny = false;
    const auto share =
        std::max<int64_t>(1, remaining / static_cast<int64_t>(hungryCount));
    for (auto& state : states) {
      if (!state.active ||
          (state.observed && state.observedSpeed < state.assignedLimit / 2)) {
        continue;
      }
      const auto cap = state.cap == 0 ? globalLimit : state.cap;
      if (state.assignedLimit >= cap) {
        continue;
      }
      const auto extra = std::min(share, cap - state.assignedLimit);
      state.assignedLimit += extra;
      remaining -= extra;
      assignedAny = true;
      if (remaining == 0) {
        break;
      }
    }
    if (!assignedAny) {
      break;
    }
  }
}

} // namespace aria2
