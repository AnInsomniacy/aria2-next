/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 AnInsomniacy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_HTTP_ADAPTIVE_WINDOW_H
#define D_HTTP_ADAPTIVE_WINDOW_H

namespace aria2 {

class HttpAdaptiveWindow {
public:
  int limit(int maxLimit) const;

  void reset(int maxLimit);

  void onSuccess(int maxLimit);

  void onTransientFailure();

  void onRangeUnsupported();

private:
  static constexpr int kInitialLimit = 4;
  static constexpr int kSlowStartThreshold = 16;
  static constexpr int kCooldownSuccesses = 2;

  int limit_ = kInitialLimit;
  int slowStartThreshold_ = kSlowStartThreshold;
  int cooldownSuccessesRemaining_ = 0;
  bool rangeUnsupported_ = false;
};

} // namespace aria2

#endif // D_HTTP_ADAPTIVE_WINDOW_H
