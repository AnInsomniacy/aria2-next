/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "http/Ranges.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace aria2::ed2k {

using Range = http::Range;

// Progress of a file split into ED2K parts (9.28 MB, each with an MD4 hash).
// Bytes arrive in blocks of up to 180 KB; a part is shared with other
// clients only after its hash verified.
//
// Sources claim blocks before requesting them, so two sources work on the
// same block only in the endgame, when nothing unclaimed is left.
class Parts {
public:
  using Owner = const void*;

  explicit Parts(int64_t size);

  int64_t size() const { return size_; }
  size_t count() const { return verified_.size(); }
  Range part(size_t index) const;
  size_t partOf(int64_t offset) const;
  bool verified(size_t index) const { return verified_[index]; }
  const std::vector<bool>& verifiedParts() const { return verified_; }
  bool complete() const;
  // Verified bytes plus received bytes of parts not yet verified.
  int64_t completed() const;
  const http::RangeSet& received() const { return received_; }

  // Records received bytes. Returns the part they filled, if any.
  std::optional<size_t> add(int64_t begin, int64_t end);
  void verify(size_t index);
  // A part failed its hash: its bytes are requested again.
  void reject(size_t index);
  bool filled(size_t index) const;

  // Claims up to `count` blocks that `available` parts hold (empty: all) for
  // `owner`. `availability` counts sources per part for rarest-first.
  std::vector<Range> claim(Owner owner, const std::vector<bool>& available,
                           size_t count, const std::vector<int>& availability,
                           bool preview, Clock::time_point now = Clock::now());
  void release(Owner owner, const Range& range);
  void releaseAll(Owner owner);
  // True when [begin, end) lies inside a block `owner` claimed.
  bool claimed(Owner owner, int64_t begin, int64_t end) const;
  // Parts the given source holds that are still missing here.
  bool wants(const std::vector<bool>& available) const;

private:
  struct Claim {
    int64_t end;
    Owner owner;
    Clock::time_point since;
  };

  void claimGaps(Owner owner, size_t index, size_t count, bool endgame,
                 Clock::time_point now, std::vector<Range>& out);

  int64_t size_;
  std::vector<bool> verified_;
  http::RangeSet received_;
  std::multimap<int64_t, Claim> claims_; // by begin
};

} // namespace aria2::ed2k
