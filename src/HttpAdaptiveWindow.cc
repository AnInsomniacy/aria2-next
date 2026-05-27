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
  rateLimitStrikes_ = 0;
  rateLimitSuccesses_ = 0;
  successesAtCeiling_ = 0;
  slowStartThreshold_ = kSlowStartThreshold;
  learnedCeiling_ = std::max(1, maxLimit);
  limit_ = std::max(1, std::min(kInitialLimit, maxLimit));
}

void HttpAdaptiveWindow::onSuccess(int maxLimit)
{
  if (rangeUnsupported_) {
    return;
  }
  if (rateLimitStrikes_ > 0) {
    ++rateLimitSuccesses_;
    if (rateLimitSuccesses_ <= rateLimitStrikes_) {
      return;
    }
    rateLimitStrikes_ = 0;
    rateLimitSuccesses_ = 0;
    slowStartThreshold_ = 2;
    successesAtCeiling_ = 0;
    limit_ = std::min(2, std::min(learnedCeiling_, maxLimit));
    return;
  }
  if (cooldownSuccessesRemaining_ > 0) {
    --cooldownSuccessesRemaining_;
    return;
  }
  const auto effectiveCeiling = std::max(1, std::min(learnedCeiling_, maxLimit));
  if (limit_ < std::min(slowStartThreshold_, effectiveCeiling)) {
    limit_ = std::min(std::min(slowStartThreshold_, effectiveCeiling),
                      limit_ * 2);
    successesAtCeiling_ = 0;
  }
  else if (limit_ < effectiveCeiling) {
    ++limit_;
    successesAtCeiling_ = 0;
  }
  else {
    ++successesAtCeiling_;
    if (successesAtCeiling_ >= kProbeSuccesses && learnedCeiling_ < maxLimit) {
      ++learnedCeiling_;
      limit_ = std::min(learnedCeiling_, maxLimit);
      successesAtCeiling_ = 0;
    }
  }
  limit_ = std::max(1, std::min(limit_, maxLimit));
}

void HttpAdaptiveWindow::onTransientFailure()
{
  if (rangeUnsupported_) {
    return;
  }
  rateLimitStrikes_ = 0;
  rateLimitSuccesses_ = 0;
  successesAtCeiling_ = 0;
  limit_ = std::max(1, limit_ / 2);
  slowStartThreshold_ = limit_;
  cooldownSuccessesRemaining_ = kCooldownSuccesses;
}

void HttpAdaptiveWindow::onRateLimited()
{
  if (rangeUnsupported_) {
    return;
  }
  learnedCeiling_ = std::max(1, std::min(learnedCeiling_,
                                         std::max(1, limit_ / 2)));
  rateLimitStrikes_ = std::min(rateLimitStrikes_ + 1, 4);
  rateLimitSuccesses_ = 0;
  successesAtCeiling_ = 0;
  cooldownSuccessesRemaining_ = 0;
  slowStartThreshold_ = 1;
  limit_ = 1;
}

void HttpAdaptiveWindow::onRangeUnsupported()
{
  rangeUnsupported_ = true;
  cooldownSuccessesRemaining_ = 0;
  rateLimitStrikes_ = 0;
  rateLimitSuccesses_ = 0;
  successesAtCeiling_ = 0;
  learnedCeiling_ = 1;
  slowStartThreshold_ = 1;
  limit_ = 1;
}

} // namespace aria2
