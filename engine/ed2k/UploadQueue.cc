/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/UploadQueue.h"

#include "ed2k/Client.h"
#include "ed2k/Hash.h"

#include <algorithm>
#include <cmath>

namespace aria2::ed2k {

using namespace std::chrono_literals;

namespace {
constexpr size_t maxWaiting = 5000;
// A waiting client that did not ask again for this long has given up.
constexpr auto purgeAfter = 1h;
// A slot is passed on after about one part or one hour.
constexpr uint64_t sessionBytes = PIECE_LENGTH + (20 << 10);
constexpr auto sessionTime = 1h;
} // namespace

UploadQueue::UploadQueue(Network& network) : network_(network) {}

uint16_t UploadQueue::request(const std::shared_ptr<Client>& client)
{
  const auto now = Clock::now();
  for (const auto& active : active_) {
    if (active.client == client) {
      return 0;
    }
  }
  auto found =
      std::find_if(waiting_.begin(), waiting_.end(),
                   [&client](const Waiting& w) { return w.client == client; });
  if (found == waiting_.end()) {
    if (waiting_.size() >= maxWaiting) {
      return static_cast<uint16_t>(std::min<size_t>(waiting_.size(), 0xffff));
    }
    waiting_.push_back({client, now, now});
  }
  else {
    found->lastAsked = now;
  }
  grant(now);
  const auto ranked = rank(*client);
  return ranked;
}

double UploadQueue::score(const Waiting& waiting, Clock::time_point now) const
{
  const auto seconds =
      std::chrono::duration<double>(now - waiting.since).count();
  return seconds * modifier(waiting.client->userHash());
}

uint16_t UploadQueue::rank(const Client& client) const
{
  const auto now = Clock::now();
  const auto found = std::find_if(
      waiting_.begin(), waiting_.end(),
      [&client](const Waiting& w) { return w.client.get() == &client; });
  if (found == waiting_.end()) {
    return 0;
  }
  const auto mine = score(*found, now);
  size_t ahead = 0;
  for (const auto& other : waiting_) {
    ahead += score(other, now) > mine ? 1 : 0;
  }
  return static_cast<uint16_t>(std::min<size_t>(ahead + 1, 0xffff));
}

void UploadQueue::touched(const Client& client)
{
  for (auto& waiting : waiting_) {
    if (waiting.client.get() == &client) {
      waiting.lastAsked = Clock::now();
    }
  }
}

void UploadQueue::remove(Client& client)
{
  std::erase_if(waiting_, [&client](const Waiting& w) {
    return w.client.get() == &client;
  });
  const auto before = active_.size();
  std::erase_if(active_, [&client](const Active& a) {
    return a.client.get() == &client;
  });
  if (active_.size() != before) {
    client.revokeSlot();
    grant(Clock::now());
  }
}

void UploadQueue::grant(Clock::time_point now)
{
  while (static_cast<int>(active_.size()) < slots_ && !waiting_.empty()) {
    auto best =
        std::max_element(waiting_.begin(), waiting_.end(),
                         [this, now](const Waiting& a, const Waiting& b) {
                           return score(a, now) < score(b, now);
                         });
    auto client = best->client;
    waiting_.erase(best);
    active_.push_back({client, now});
    client->grantSlot();
  }
}

void UploadQueue::tick(Clock::time_point now)
{
  std::erase_if(waiting_, [now](const Waiting& w) {
    return now - w.lastAsked > purgeAfter && !w.client->connected();
  });
  // Rotate long sessions while others wait.
  if (!waiting_.empty()) {
    for (auto it = active_.begin(); it != active_.end();) {
      auto& client = *it->client;
      if (client.sessionUploaded() >= sessionBytes ||
          now - it->since > sessionTime) {
        auto ended = it->client;
        it = active_.erase(it);
        ended->revokeSlot();
        // eMule puts it back at the end of the queue.
        waiting_.push_back({ended, now, now});
        continue;
      }
      ++it;
    }
  }
  // Slots of clients that lost their connection free up.
  for (auto it = active_.begin(); it != active_.end();) {
    if (it->client->upload() != Client::Upload::Uploading) {
      it = active_.erase(it);
      continue;
    }
    ++it;
  }
  grant(now);
}

void UploadQueue::uploaded(const std::string& userHash, uint64_t bytes)
{
  if (userHash.size() == HASH_LENGTH) {
    auto& credit = credits_[userHash];
    credit.userHash = userHash;
    credit.uploaded += bytes;
  }
}

void UploadQueue::downloaded(const std::string& userHash, uint64_t bytes)
{
  if (userHash.size() == HASH_LENGTH) {
    auto& credit = credits_[userHash];
    credit.userHash = userHash;
    credit.downloaded += bytes;
  }
}

double UploadQueue::modifier(const std::string& userHash) const
{
  const auto found = credits_.find(userHash);
  if (found == credits_.end() || found->second.downloaded < (1u << 20)) {
    return 1;
  }
  const auto& credit = found->second;
  const double down = static_cast<double>(credit.downloaded);
  const double first = credit.uploaded == 0
                           ? 10
                           : down * 2 / static_cast<double>(credit.uploaded);
  const double second = std::sqrt(down / (1 << 20) + 2);
  return std::clamp(std::min(first, second), 1.0, 10.0);
}

std::vector<Credit> UploadQueue::credits() const
{
  std::vector<Credit> result;
  result.reserve(credits_.size());
  for (const auto& [hash, credit] : credits_) {
    result.push_back(credit);
  }
  return result;
}

void UploadQueue::restore(const std::vector<Credit>& credits)
{
  for (const auto& credit : credits) {
    credits_[credit.userHash] = credit;
  }
}

} // namespace aria2::ed2k
