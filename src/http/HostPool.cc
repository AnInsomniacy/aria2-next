/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/HostPool.h"

#include <algorithm>
#include <chrono>
#include <optional>

namespace aria2::http {

namespace {
using namespace std::chrono_literals;
// A lowered limit gains one connection per calm period without overload.
constexpr auto calmPeriod = 5s;
// Pause after repeated refusals: 1 s, doubling up to this.
constexpr auto maxPause = 8s;
} // namespace

void HostPool::update(Member member, const std::string& host, int wanted,
                      int64_t remaining, int transferring)
{
  auto [entry, added] = members_.try_emplace(member);
  auto& demand = entry->second;
  if (added) {
    demand.order = nextOrder_++;
  }
  stale_ = stale_ || added || demand.host != host || demand.wanted != wanted ||
           demand.remaining != remaining;
  demand.host = host;
  demand.wanted = std::max(1, wanted);
  demand.remaining = remaining;
  demand.transferring = transferring;
  // Connections the server goes on serving are proof that it accepts them:
  // requests in flight when it refused others may still have been admitted.
  if (const auto state = hosts_.find(host);
      state != hosts_.end() && state->second.limit > 0) {
    int total = 0;
    for (const auto& [other, entry] : members_) {
      total += entry.host == host ? entry.transferring : 0;
    }
    if (total > state->second.limit) {
      state->second.limit = total;
      stale_ = true;
    }
  }
}

void HostPool::leave(Member member)
{
  const auto found = members_.find(member);
  if (found == members_.end()) {
    return;
  }
  const auto host = found->second.host;
  members_.erase(found);
  stale_ = true;
  // A server nobody downloads from any more is asked afresh next time.
  const bool used =
      std::any_of(members_.begin(), members_.end(),
                  [&](const auto& entry) { return entry.second.host == host; });
  if (!used) {
    hosts_.erase(host);
  }
}

std::vector<std::pair<HostPool::Member, const HostPool::Demand*>>
HostPool::membersOf(const std::string& host) const
{
  std::vector<std::pair<Member, const Demand*>> list;
  for (const auto& [member, demand] : members_) {
    if (demand.host == host) {
      list.emplace_back(member, &demand);
    }
  }
  std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
    return a.second->order < b.second->order;
  });
  return list;
}

void HostPool::allocate(const std::string& host) const
{
  const auto found = hosts_.find(host);
  const auto list = membersOf(host);
  if (found == hosts_.end() || found->second.limit <= 0) {
    for (const auto& [member, demand] : list) {
      allowances_[member] = demand->wanted;
    }
    return;
  }
  int seats = found->second.limit;
  std::vector<int> given(list.size(), 0);
  // Every download first gets one connection, in the order they came.
  for (size_t i = 0; i < list.size() && seats > 0; ++i, --seats) {
    given[i] = 1;
  }
  // The rest go one at a time to the largest remaining work per connection.
  while (seats > 0) {
    std::optional<size_t> best;
    double bestQuotient = 0;
    for (size_t i = 0; i < list.size(); ++i) {
      if (given[i] == 0 || given[i] >= list[i].second->wanted) {
        continue;
      }
      const auto weight =
          static_cast<double>(std::max<int64_t>(1, list[i].second->remaining));
      const auto quotient = weight / (given[i] + 1);
      if (!best || quotient > bestQuotient) {
        best = i;
        bestQuotient = quotient;
      }
    }
    if (!best) {
      break;
    }
    ++given[*best];
    --seats;
  }
  for (size_t i = 0; i < list.size(); ++i) {
    allowances_[list[i].first] = given[i];
  }
}

int HostPool::allowance(Member member) const
{
  const auto found = members_.find(member);
  if (found == members_.end()) {
    return 0;
  }
  if (stale_) {
    allowances_.clear();
    std::vector<std::string> done;
    for (const auto& [other, demand] : members_) {
      if (std::find(done.begin(), done.end(), demand.host) == done.end()) {
        allocate(demand.host);
        done.push_back(demand.host);
      }
    }
    stale_ = false;
  }
  const auto allowed = allowances_.find(member);
  return allowed == allowances_.end() ? found->second.wanted : allowed->second;
}

void HostPool::overloaded(Member member, Clock::time_point sent,
                          Clock::time_point now, Clock::time_point readyAt)
{
  const auto found = members_.find(member);
  if (found == members_.end()) {
    return;
  }
  const auto& host = found->second.host;
  auto& state = hosts_[host];
  state.readyAt = std::max(state.readyAt, readyAt);
  if (state.limit > 0 && sent < state.dropped) {
    return; // asked before the last drop: already answered by it
  }
  int transferring = 0;
  for (const auto& [other, demand] : members_) {
    transferring += demand.host == host ? demand.transferring : 0;
  }
  state.limit = std::max(1, transferring);
  state.strikes = now - state.calmSince < calmPeriod ? state.strikes + 1 : 1;
  state.calmSince = now;
  state.dropped = now;
  const auto pause = std::min<Clock::duration>(
      maxPause, 1s * (1 << std::min(state.strikes - 1, 5)));
  state.readyAt = std::max(state.readyAt, now + pause);
  stale_ = true;
}

int HostPool::transferring(Member member) const
{
  const auto found = members_.find(member);
  if (found == members_.end()) {
    return 0;
  }
  int total = 0;
  for (const auto& [other, demand] : members_) {
    total += demand.host == found->second.host ? demand.transferring : 0;
  }
  return total;
}

Clock::time_point HostPool::readyAt(Member member) const
{
  const auto found = members_.find(member);
  if (found == members_.end()) {
    return {};
  }
  const auto host = hosts_.find(found->second.host);
  return host == hosts_.end() ? Clock::time_point{} : host->second.readyAt;
}

void HostPool::tick(Clock::time_point now)
{
  for (auto it = hosts_.begin(); it != hosts_.end();) {
    auto& [host, state] = *it;
    int wanted = 0;
    bool used = false;
    for (const auto& [member, demand] : members_) {
      if (demand.host == host) {
        wanted += demand.wanted;
        used = true;
      }
    }
    // A member that moved to another server (a redirect) left this one.
    if (!used) {
      it = hosts_.erase(it);
      continue;
    }
    if (state.limit > 0 && now - state.calmSince >= calmPeriod) {
      state.limit = state.limit + 1 >= wanted ? 0 : state.limit + 1;
      state.strikes = 0;
      state.calmSince = now;
      stale_ = true;
    }
    ++it;
  }
}

int HostPool::limit(const std::string& host) const
{
  const auto found = hosts_.find(host);
  return found == hosts_.end() ? 0 : found->second.limit;
}

} // namespace aria2::http
