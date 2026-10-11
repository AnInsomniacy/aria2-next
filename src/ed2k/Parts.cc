/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Parts.h"

#include "ed2k/Hash.h"

#include <algorithm>
#include <random>
#include <tuple>

namespace aria2::ed2k {

namespace {
// In the endgame a block is duplicated only when its claim is this old.
constexpr auto duplicateAfter = std::chrono::seconds(5);
} // namespace

Parts::Parts(int64_t size)
    : size_(size),
      verified_(static_cast<size_t>((size + PIECE_LENGTH - 1) / PIECE_LENGTH))
{
}

Range Parts::part(size_t index) const
{
  const auto begin = static_cast<int64_t>(index) * PIECE_LENGTH;
  return {begin, std::min<int64_t>(begin + PIECE_LENGTH, size_)};
}

size_t Parts::partOf(int64_t offset) const
{
  return static_cast<size_t>(offset / PIECE_LENGTH);
}

bool Parts::complete() const
{
  return std::all_of(verified_.begin(), verified_.end(),
                     [](bool done) { return done; });
}

int64_t Parts::completed() const { return received_.total(); }

bool Parts::filled(size_t index) const
{
  const auto [begin, end] = part(index);
  return received_.contains(begin, end);
}

std::optional<size_t> Parts::add(int64_t begin, int64_t end)
{
  begin = std::max<int64_t>(begin, 0);
  end = std::min(end, size_);
  if (begin >= end) {
    return std::nullopt;
  }
  received_.add(begin, end);
  for (auto index = partOf(begin); index <= partOf(end - 1); ++index) {
    if (!verified_[index] && filled(index)) {
      return index;
    }
  }
  return std::nullopt;
}

void Parts::verify(size_t index)
{
  verified_[index] = true;
  const auto [begin, end] = part(index);
  received_.add(begin, end);
}

void Parts::reject(size_t index)
{
  verified_[index] = false;
  const auto [begin, end] = part(index);
  received_.remove(begin, end);
}

bool Parts::wants(const std::vector<bool>& available) const
{
  for (size_t index = 0; index < count(); ++index) {
    if (!verified_[index] && !filled(index) &&
        (available.empty() || (index < available.size() && available[index]))) {
      return true;
    }
  }
  return false;
}

std::vector<Range> Parts::claim(Owner owner, const std::vector<bool>& available,
                                size_t count,
                                const std::vector<int>& availability,
                                bool preview, Clock::time_point now)
{
  struct Candidate {
    size_t index;
    std::tuple<int, int, int, int, unsigned> rank; // lower is better
  };
  static thread_local std::minstd_rand random{std::random_device{}()};
  std::vector<Candidate> candidates;
  const auto last = this->count() - 1;
  for (size_t index = 0; index < this->count(); ++index) {
    if (verified_[index] || filled(index) ||
        (!available.empty() &&
         (index >= available.size() || !available[index]))) {
      continue;
    }
    const auto [begin, end] = part(index);
    bool mine = false;
    bool claimedHere = false;
    for (const auto& [first, claim] : claims_) {
      if (first < end && claim.end > begin) {
        claimedHere = true;
        mine = mine || claim.owner == owner;
      }
    }
    const bool started = claimedHere || received_.covered(begin, end) > 0;
    // Previews want the first part and the last (or two, if it is short).
    const bool edge =
        index == 0 || index == last ||
        (index + 1 == last && size_ - part(last).first < PIECE_LENGTH / 3);
    const int rarity = index < availability.size() ? availability[index] : 0;
    candidates.push_back(
        {index,
         {mine ? 0 : 1, preview && edge ? 0 : 1, started ? 0 : 1, rarity,
          static_cast<unsigned>(random())}});
  }
  std::sort(
      candidates.begin(), candidates.end(),
      [](const Candidate& a, const Candidate& b) { return a.rank < b.rank; });
  std::vector<Range> out;
  for (const bool endgame : {false, true}) {
    for (const auto& candidate : candidates) {
      if (out.size() >= count) {
        return out;
      }
      claimGaps(owner, candidate.index, count, endgame, now, out);
    }
    if (!out.empty()) {
      break;
    }
  }
  return out;
}

void Parts::claimGaps(Owner owner, size_t index, size_t count, bool endgame,
                      Clock::time_point now, std::vector<Range>& out)
{
  const auto [partBegin, partEnd] = part(index);
  auto from = partBegin;
  while (out.size() < count) {
    const auto gap = received_.firstGap(from, partEnd);
    if (!gap) {
      return;
    }
    // The first stretch of the gap that no blocking claim covers.
    auto begin = gap->first;
    auto end = gap->second;
    for (bool moved = true; moved && begin < end;) {
      moved = false;
      for (const auto& [first, claim] : claims_) {
        if (claim.end <= begin || first >= end ||
            !(claim.owner == owner || !endgame ||
              now - claim.since < duplicateAfter)) {
          continue;
        }
        if (first <= begin) {
          begin = claim.end;
          moved = true;
          break;
        }
        end = first;
      }
    }
    if (begin >= end) {
      from = gap->second;
      continue;
    }
    // Blocks follow the 180 KB grid of the part, as eMule requests them.
    const auto grid =
        partBegin + ((begin - partBegin) / BLOCK_LENGTH + 1) * BLOCK_LENGTH;
    const Range block{begin, std::min(end, grid)};
    claims_.emplace(block.first, Claim{block.second, owner, now});
    out.push_back(block);
    from = block.second;
  }
}

void Parts::release(Owner owner, const Range& range)
{
  auto [first, last] = claims_.equal_range(range.first);
  for (auto it = first; it != last; ++it) {
    if (it->second.owner == owner) {
      claims_.erase(it);
      return;
    }
  }
}

void Parts::releaseAll(Owner owner)
{
  std::erase_if(claims_, [owner](const auto& entry) {
    return entry.second.owner == owner;
  });
}

bool Parts::claimed(Owner owner, int64_t begin, int64_t end) const
{
  for (const auto& [first, claim] : claims_) {
    if (claim.owner == owner && first <= begin && end <= claim.end) {
      return true;
    }
  }
  return false;
}

} // namespace aria2::ed2k
