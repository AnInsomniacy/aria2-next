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
#include "HttpAdaptiveWindow.h"

#include <algorithm>

namespace aria2 {

int HttpAdaptiveWindow::limit(int maxLimit) const
{
  if (maxLimit <= 1 || rangeUnsupported_) {
    return 1;
  }
  return std::max(1, std::min(limit_, maxLimit));
}

void HttpAdaptiveWindow::reset(int maxLimit)
{
  rangeUnsupported_ = false;
  cooldownSuccessesRemaining_ = 0;
  slowStartThreshold_ = kSlowStartThreshold;
  limit_ = std::max(1, std::min(kInitialLimit, maxLimit));
}

void HttpAdaptiveWindow::onSuccess(int maxLimit)
{
  if (rangeUnsupported_) {
    return;
  }
  if (cooldownSuccessesRemaining_ > 0) {
    --cooldownSuccessesRemaining_;
    return;
  }
  if (limit_ < slowStartThreshold_) {
    limit_ = std::min(slowStartThreshold_, limit_ * 2);
  }
  else {
    ++limit_;
  }
  limit_ = std::max(1, std::min(limit_, maxLimit));
}

void HttpAdaptiveWindow::onTransientFailure()
{
  if (rangeUnsupported_) {
    return;
  }
  limit_ = std::max(1, limit_ / 2);
  slowStartThreshold_ = limit_;
  cooldownSuccessesRemaining_ = kCooldownSuccesses;
}

void HttpAdaptiveWindow::onRangeUnsupported()
{
  rangeUnsupported_ = true;
  cooldownSuccessesRemaining_ = 0;
  slowStartThreshold_ = 1;
  limit_ = 1;
}

} // namespace aria2
