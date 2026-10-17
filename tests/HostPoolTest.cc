/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/HostPool.h"

#include <doctest/doctest.h>

#include <chrono>

using namespace aria2;
using namespace std::chrono_literals;

namespace {
const int members[3]{};
const void* const V = &members[0];
const void* const A = &members[1];
const void* const S = &members[2];
} // namespace

TEST_CASE("host pool without overload grants every request")
{
  http::HostPool pool;
  pool.update(A, "https://cdn:443", 16, 48 << 20, 16);
  pool.update(V, "https://cdn:443", 16, 128 << 20, 0);
  CHECK(pool.allowance(A) == 16);
  CHECK(pool.allowance(V) == 16);
  CHECK(pool.limit("https://cdn:443") == 0);
}

TEST_CASE("host pool divides an overloaded server by remaining bytes")
{
  http::HostPool pool;
  const auto now = Clock::now();
  pool.update(A, "https://cdn:443", 16, 48 << 20, 16);
  pool.update(S, "https://cdn:443", 1, 1 << 20, 1);
  pool.update(V, "https://cdn:443", 16, 128 << 20, 3);
  pool.overloaded(V, now, now, now + 1s);
  CHECK(pool.limit("https://cdn:443") == 20);
  CHECK(pool.transferring(V) == 20);
  CHECK(pool.readyAt(A) == now + 1s);
  // One each first; the larger download then takes most of the rest.
  CHECK(pool.allowance(S) == 1);
  CHECK(pool.allowance(A) + pool.allowance(V) + pool.allowance(S) == 20);
  CHECK(pool.allowance(V) > pool.allowance(A));
  CHECK(pool.allowance(V) >= 13);

  // A finished download's connections go to the others at once.
  pool.leave(S);
  pool.leave(A);
  CHECK(pool.allowance(V) == 16);
}

TEST_CASE("host pool recovers one connection per calm period")
{
  http::HostPool pool;
  const auto start = Clock::now();
  pool.update(V, "https://cdn:443", 4, 1 << 30, 2);
  pool.overloaded(V, start, start, start);
  CHECK(pool.allowance(V) == 2);
  pool.tick(start + 1s);
  CHECK(pool.allowance(V) == 2);
  pool.tick(start + 6s);
  CHECK(pool.allowance(V) == 3);
  // Once it covers every request the limit is lifted.
  pool.tick(start + 12s);
  CHECK(pool.limit("https://cdn:443") == 0);
  CHECK(pool.allowance(V) == 4);
}

TEST_CASE("host pool lowers the limit once per window")
{
  http::HostPool pool;
  const auto start = Clock::now();
  pool.update(V, "https://cdn:443", 16, 1 << 30, 10);
  pool.overloaded(V, start, start, start);
  CHECK(pool.limit("https://cdn:443") == 10);
  // Refusals of requests already in flight at the drop change nothing.
  pool.update(V, "https://cdn:443", 16, 1 << 30, 6);
  pool.overloaded(V, start - 1s, start + 500ms, start + 500ms);
  CHECK(pool.limit("https://cdn:443") == 10);
  CHECK(pool.readyAt(V) == start + 1s);
}

TEST_CASE("host pool pauses longer after drops in a row")
{
  http::HostPool pool;
  const auto start = Clock::now();
  pool.update(V, "https://cdn:443", 16, 1 << 30, 10);
  pool.overloaded(V, start, start, start);
  CHECK(pool.readyAt(V) == start + 1s);
  pool.overloaded(V, start + 2s, start + 2s, start + 2s);
  CHECK(pool.readyAt(V) == start + 4s);
  pool.overloaded(V, start + 4s, start + 4s, start + 4s);
  CHECK(pool.readyAt(V) == start + 8s);
  // After a calm period a refusal starts over.
  pool.tick(start + 10s);
  pool.overloaded(V, start + 20s, start + 20s, start + 20s);
  CHECK(pool.readyAt(V) == start + 21s);
}

TEST_CASE("host pool keeps servers apart and forgets idle ones")
{
  http::HostPool pool;
  const auto now = Clock::now();
  pool.update(V, "https://a:443", 8, 1 << 20, 4);
  pool.update(A, "https://b:443", 8, 1 << 20, 8);
  pool.overloaded(V, now, now, now);
  CHECK(pool.allowance(V) == 4);
  CHECK(pool.allowance(A) == 8);
  pool.leave(V);
  CHECK(pool.limit("https://a:443") == 0);
  CHECK(pool.limit("https://b:443") == 0);

  // The next download from a server starts without its old limit.
  pool.update(A, "https://b:443", 8, 1 << 20, 8);
  pool.overloaded(A, now, now, now);
  CHECK(pool.limit("https://b:443") == 8);
  pool.leave(A);
  pool.update(V, "https://b:443", 16, 1 << 20, 0);
  CHECK(pool.limit("https://b:443") == 0);
  CHECK(pool.allowance(V) == 16);
}
