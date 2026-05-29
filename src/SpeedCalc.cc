/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "SpeedCalc.h"

#include <algorithm>

#include "wallclock.h"

namespace aria2 {

namespace {
constexpr auto SAMPLE_INTERVAL = 500_ms;
} // namespace

SpeedCalc::SpeedCalc()
    : buckets_{},
      bucketIndex_(0),
      start_(global::wallclock()),
      lastTick_(start_),
      accumulatedLength_(0),
      currentBucketBytes_(0),
      publishedWindowBytes_(0),
      publishedSamples_(0),
      publishedSpeed_(0),
      maxSpeed_(0)
{
}

void SpeedCalc::reset()
{
  buckets_.fill(0);
  bucketIndex_ = 0;
  start_ = global::wallclock();
  lastTick_ = start_;
  accumulatedLength_ = 0;
  currentBucketBytes_ = 0;
  publishedWindowBytes_ = 0;
  publishedSamples_ = 0;
  publishedSpeed_ = 0;
  maxSpeed_ = 0;
}

void SpeedCalc::publishElapsedSamples(const Timer& now)
{
  auto elapsedSamples = lastTick_.difference(now) / SAMPLE_INTERVAL;
  while (elapsedSamples > 0) {
    const auto sampleBytes = currentBucketBytes_;
    currentBucketBytes_ = 0;
    if (sampleBytes == 0) {
      buckets_.fill(0);
      publishedWindowBytes_ = 0;
      publishedSamples_ = 0;
      publishedSpeed_ = 0;
      lastTick_.advance(SAMPLE_INTERVAL);
      --elapsedSamples;
      continue;
    }
    bucketIndex_ = (bucketIndex_ + 1) % buckets_.size();
    publishedWindowBytes_ -= buckets_[bucketIndex_];
    buckets_[bucketIndex_] = sampleBytes;
    publishedWindowBytes_ += sampleBytes;
    publishedSamples_ =
        std::min<int>(publishedSamples_ + 1, static_cast<int>(buckets_.size()));
    publishedSpeed_ =
        publishedSamples_ == 0
            ? 0
            : (publishedWindowBytes_ * 2) / publishedSamples_;
    maxSpeed_ = std::max(publishedSpeed_, maxSpeed_);
    lastTick_.advance(SAMPLE_INTERVAL);
    --elapsedSamples;
  }
}

int SpeedCalc::calculateSpeed()
{
  const auto& now = global::wallclock();
  publishElapsedSamples(now);
  return publishedSpeed_;
}

void SpeedCalc::update(size_t bytes)
{
  const auto& now = global::wallclock();
  publishElapsedSamples(now);
  currentBucketBytes_ += bytes;
  accumulatedLength_ += bytes;
}

int SpeedCalc::calculateAvgSpeed() const
{
  auto milliElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                          start_.difference(global::wallclock()))
                          .count();
  // if milliElapsed is too small, the average speed is rubbish, better
  // return 0
  if (milliElapsed > 4) {
    int speed = accumulatedLength_ * 1000 / milliElapsed;
    return speed;
  }
  else {
    return 0;
  }
}

} // namespace aria2
