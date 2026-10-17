/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aria2::http {

// Connections to one server, shared by every download from it.
//
// A server that limits how many connections a client may hold answers the
// excess with 429 or 503. The connections it answered at that moment are what
// it accepts, and more should more go on working; that number becomes the
// host's limit and is divided among the downloads from the server, so the
// first one to ask does not keep them all. Each download gets one connection,
// the rest go by remaining bytes, as D'Hondt seats are allocated. The limit
// grows by one per calm period (additive increase) and is lifted once it
// covers every request. As in TCP congestion control, the limit drops once
// per window: refusals of requests sent before the last drop answer what was
// already asked and change nothing. Drops in a row double the pause before
// the next new request.
//
// Without overload feedback a host has no limit and every download gets the
// connections it asks for. Runs on the control thread.
class HostPool {
public:
  using Member = const void*;

  // A member's current wish: up to `wanted` connections for `remaining` bytes
  // (-1 when the length is unknown), `transferring` of them delivering now.
  void update(Member member, const std::string& host, int wanted,
              int64_t remaining, int transferring);
  void leave(Member member);

  // Connections the member may hold now.
  int allowance(Member member) const;
  // The member's host answered 429 or 503 to a request sent at `sent`; no
  // request reaches it before `readyAt`.
  void overloaded(Member member, Clock::time_point sent, Clock::time_point now,
                  Clock::time_point readyAt);
  // Connections delivering from the member's host, over all its downloads.
  int transferring(Member member) const;
  // Earliest time a new request may reach the member's host.
  Clock::time_point readyAt(Member member) const;
  // Lets a limit recover after a calm period.
  void tick(Clock::time_point now);

  // 0 while the host has given no overload feedback.
  int limit(const std::string& host) const;

private:
  struct Demand {
    std::string host;
    int wanted = 1;
    int64_t remaining = -1;
    int transferring = 0;
    uint64_t order = 0; // joining order; earlier members keep the first seats
  };
  struct Host {
    int limit = 0;
    int strikes = 0; // drops without a calm period between them
    Clock::time_point calmSince{};
    Clock::time_point dropped{}; // last time the limit was lowered
    Clock::time_point readyAt{};
  };

  std::vector<std::pair<Member, const Demand*>>
  membersOf(const std::string& host) const;
  void allocate(const std::string& host) const;

  std::map<Member, Demand> members_;
  std::map<std::string, Host> hosts_;
  mutable std::map<Member, int> allowances_;
  mutable bool stale_ = true;
  uint64_t nextOrder_ = 0;
};

} // namespace aria2::http
