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
#ifndef D_RATE_LIMIT_SCHEDULER_H
#define D_RATE_LIMIT_SCHEDULER_H

#include "common.h"

#include <array>
#include <stdint.h>

namespace aria2 {

enum class RateLimitBackend { Curl = 0, Libtorrent = 1, Ed2k = 2 };

enum class RateLimitDirection { Download = 0, Upload = 1 };

class RateLimitScheduler {
public:
  void setGlobalLimit(RateLimitDirection direction, int64_t limit);

  void setBackendCap(RateLimitBackend backend, RateLimitDirection direction,
                     int64_t limit);

  void setActive(RateLimitBackend backend, RateLimitDirection direction,
                 bool active);

  void setObservedSpeed(RateLimitBackend backend, RateLimitDirection direction,
                        int64_t speed);

  void clearObservedSpeeds();

  void recalculate();

  int64_t backendLimit(RateLimitBackend backend,
                       RateLimitDirection direction) const;

private:
  static constexpr size_t BACKEND_COUNT = 3;
  static constexpr size_t DIRECTION_COUNT = 2;

  struct BackendState {
    bool active = false;
    int64_t cap = 0;
    int64_t observedSpeed = 0;
    bool observed = false;
    int64_t assignedLimit = 0;
  };

  static size_t index(RateLimitBackend backend);
  static size_t index(RateLimitDirection direction);

  void recalculateDirection(RateLimitDirection direction);

  std::array<int64_t, DIRECTION_COUNT> globalLimits_{};
  std::array<std::array<BackendState, BACKEND_COUNT>, DIRECTION_COUNT>
      states_{};
};

} // namespace aria2

#endif // D_RATE_LIMIT_SCHEDULER_H
