/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>

namespace aria2 {

using Clock = std::chrono::steady_clock;

// Transfer rate over the last few seconds, in bytes per second.
class SpeedMeter {
public:
  void add(int64_t bytes, Clock::time_point now = Clock::now())
  {
    advance(now);
    slots_[cursor_] += bytes;
    total_ += bytes;
  }

  int64_t rate(Clock::time_point now = Clock::now())
  {
    advance(now);
    int64_t sum = 0;
    for (const auto value : slots_) {
      sum += value;
    }
    const auto window = std::chrono::duration<double>(
        std::min<Clock::duration>(slot * (slots_.size() - 1) +
                                      (now - slotStart_),
                                  now - begin_));
    return window.count() > 0 ? static_cast<int64_t>(sum / window.count()) : 0;
  }

  int64_t total() const { return total_; }

private:
  static constexpr auto slot = std::chrono::milliseconds(500);

  void advance(Clock::time_point now)
  {
    if (slotStart_ == Clock::time_point{}) {
      slotStart_ = begin_ = now;
    }
    while (now - slotStart_ >= slot) {
      cursor_ = (cursor_ + 1) % slots_.size();
      slots_[cursor_] = 0;
      slotStart_ += slot;
      if (now - slotStart_ > slot * slots_.size()) {
        slots_.fill(0);
        slotStart_ = now;
      }
    }
  }

  std::array<int64_t, 8> slots_{};
  size_t cursor_ = 0;
  Clock::time_point slotStart_{};
  Clock::time_point begin_{};
  int64_t total_ = 0;
};

// Token bucket shared by transfers that obey one speed limit.
class RateLimit {
public:
  void setRate(int64_t bytesPerSecond)
  {
    rate_ = bytesPerSecond;
    tokens_ = std::min(tokens_, burst());
  }
  int64_t rate() const { return rate_; }
  bool unlimited() const { return rate_ <= 0; }

  // Takes up to `wanted` bytes; returns how many may be consumed now.
  int64_t take(int64_t wanted, Clock::time_point now = Clock::now())
  {
    if (unlimited()) {
      return wanted;
    }
    refill(now);
    const auto granted = std::min(wanted, std::max<int64_t>(tokens_, 0));
    tokens_ -= granted;
    return granted;
  }
  // Consumes bytes that were already received.
  void charge(int64_t bytes, Clock::time_point now = Clock::now())
  {
    if (!unlimited()) {
      refill(now);
      tokens_ -= bytes;
    }
  }
  bool available(Clock::time_point now = Clock::now())
  {
    if (unlimited()) {
      return true;
    }
    refill(now);
    return tokens_ > 0;
  }

private:
  int64_t burst() const { return std::max<int64_t>(rate_ / 4, 16 << 10); }
  void refill(Clock::time_point now)
  {
    const auto elapsed = std::chrono::duration<double>(now - stamp_).count();
    stamp_ = now;
    tokens_ = std::min(burst(),
                       tokens_ + static_cast<int64_t>(elapsed * rate_));
  }

  int64_t rate_ = 0;
  int64_t tokens_ = 0;
  Clock::time_point stamp_ = Clock::now();
};

} // namespace aria2
