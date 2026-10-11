/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Text.h"
#include "ed2k/Compression.h"
#include "ed2k/Hash.h"
#include "ed2k/KadRouting.h"
#include "ed2k/Link.h"
#include "ed2k/Parts.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <random>
#include <set>

namespace ed2k = aria2::ed2k;
using ed2k::KadContact;
using ed2k::KadTraversal;
using ed2k::KadTraversalAction;
using ed2k::KadTraversalActionType;

TEST_CASE("ED2K links keep their sources, hash set and AICH root")
{
  const std::string part1(32, 'a');
  const std::string part2(32, 'b');
  const auto uri = "ed2k://|file|a%20b.bin|9728000|" + std::string(32, 'c') +
                   "|p=" + part1 + ':' + part2 + "|h=" + std::string(32, 'A') +
                   "|/|sources,10.0.0.1:4662," +
                   "10.0.0.2:4663:128:" + std::string(32, 'd') + "|/";
  REQUIRE(ed2k::isLink(uri));
  const auto link = ed2k::parseLink(uri);
  CHECK(link.name == "a b.bin");
  CHECK(link.size == ed2k::PIECE_LENGTH);
  CHECK(link.pieceHashes.size() == 2);
  CHECK(link.aichHash.size() == ed2k::AICH_HASH_LENGTH);
  REQUIRE(link.sources.size() == 2);
  CHECK(link.sources[0].host == "10.0.0.1");
  CHECK(link.sources[0].port == 4662);
  CHECK(link.sources[1].cryptOptions == 128);
  CHECK(aria2::text::toHex(link.sources[1].userHash) == std::string(32, 'd'));
  CHECK(ed2k::parseLink(ed2k::toFileLink(link)).sources[1].userHash ==
        link.sources[1].userHash);
  CHECK_THROWS(ed2k::parseLink("ed2k://|file|x|12|nothex|/"));
}

TEST_CASE("an exact multiple of the part size has one more hash, of nothing")
{
  CHECK(aria2::text::toHex(ed2k::md4Digest("")) ==
        "31d6cfe0d16ae931b73c59d7e0c089c0");
  CHECK(ed2k::hashSetPartCount(ed2k::PIECE_LENGTH - 1) == 0);
  CHECK(ed2k::hashSetPartCount(ed2k::PIECE_LENGTH) == 2);
  CHECK(ed2k::hashSetPartCount(2 * int64_t{ed2k::PIECE_LENGTH} + 1) == 3);
  const std::vector<std::string> one{ed2k::md4Digest("x")};
  CHECK(ed2k::rootHash(one) == one[0]);
  const std::vector<std::string> two{ed2k::md4Digest("x"), ed2k::md4Digest("")};
  CHECK(ed2k::rootHash(two) == ed2k::md4Digest(two[0] + two[1]));
}

TEST_CASE("blocks are deflated only when that saves bytes")
{
  const std::string text(ed2k::BLOCK_LENGTH, 'z');
  std::string packed;
  REQUIRE(ed2k::deflateBlock(packed, text));
  CHECK(packed.size() < text.size() / 10);
  std::string unpacked;
  REQUIRE(ed2k::inflateCompressedPartData(unpacked, packed, text.size()));
  CHECK(unpacked == text);

  std::mt19937 random(7);
  std::string noise(ed2k::BLOCK_LENGTH, '\0');
  for (auto& byte : noise) {
    byte = static_cast<char>(random());
  }
  CHECK_FALSE(ed2k::deflateBlock(packed, noise));
}

TEST_CASE("sources claim disjoint blocks until the endgame")
{
  ed2k::Parts parts(2 * int64_t{ed2k::PIECE_LENGTH} + 1000);
  REQUIRE(parts.count() == 3);
  const int a = 0;
  const int b = 0;
  const auto first = parts.claim(&a, {}, 60, {}, false);
  const auto second = parts.claim(&b, {}, 60, {}, false);
  REQUIRE_FALSE(first.empty());
  REQUIRE_FALSE(second.empty());
  aria2::http::RangeSet claimed;
  int64_t total = 0;
  for (const auto& range : first) {
    CHECK(range.second - range.first <= ed2k::BLOCK_LENGTH);
    claimed.add(range.first, range.second);
    total += range.second - range.first;
  }
  for (const auto& range : second) {
    claimed.add(range.first, range.second);
    total += range.second - range.first;
  }
  CHECK(claimed.total() == total); // no overlap
  CHECK(parts.claim(&b, {}, 60, {}, false)
            .empty()); // the rest is claimed, not stale yet

  // A source holding only the last part is offered that part.
  parts.releaseAll(&a);
  parts.releaseAll(&b);
  const auto last = parts.claim(&a, {false, false, true}, 10, {}, false);
  REQUIRE(last.size() == 1);
  CHECK(last[0] == parts.part(2));

  const auto [begin, end] = parts.part(2);
  CHECK(parts.add(begin, end) == std::optional<size_t>(2));
  CHECK_FALSE(parts.verified(2));
  parts.reject(2);
  CHECK(parts.completed() == 0);
  parts.add(begin, end);
  parts.verify(2);
  CHECK(parts.completed() == end - begin);
  CHECK_FALSE(parts.complete());
}

namespace {

// Contacts ranked by distance to the zero target: rank 0 is the closest.
KadContact rankedContact(int rank)
{
  std::string id(ed2k::HASH_LENGTH, '\0');
  const uint32_t distance = (rank + 1) * 1000;
  for (int i = 0; i < 4; ++i) {
    id[i] = static_cast<char>(distance >> (8 * i));
  }
  return {id,
          "10.0." + std::to_string(rank / 250) + '.' +
              std::to_string(rank % 250 + 1),
          4672,
          4662,
          8,
          0};
}

int rankOf(const KadContact& contact)
{
  return std::stoi(contact.host.substr(contact.host.rfind('.') + 1)) - 1 +
         250 * std::stoi(contact.host.substr(5, contact.host.rfind('.') - 5));
}

struct Walk {
  std::set<int> asked;
  std::set<int> searched;
};

// Runs a traversal where each node knows a few nodes about twice as close as
// itself, and nodes in `dead` never answer.
Walk walk(KadTraversal& traversal, const std::vector<int>& seeds,
          const std::set<int>& dead)
{
  Walk result;
  std::vector<KadContact> seedContacts;
  for (const int rank : seeds) {
    seedContacts.push_back(rankedContact(rank));
  }
  auto pending = traversal.start(seedContacts);
  for (int round = 0; !pending.empty() && round < 1000; ++round) {
    const auto action = pending.front();
    pending.erase(pending.begin());
    const int rank = rankOf(action.contact);
    std::vector<KadTraversalAction> next;
    if (action.type == KadTraversalActionType::SEARCH) {
      result.searched.insert(rank);
      traversal.onSearchResponse(action.contact);
      continue;
    }
    result.asked.insert(rank);
    if (dead.contains(rank)) {
      next = traversal.onFailure(action.contact);
    }
    else {
      std::vector<KadContact> closer;
      for (int known = std::max(0, rank / 2 - 1); known < rank / 2 + 4;
           ++known) {
        closer.push_back(rankedContact(known));
      }
      next = traversal.onResponse(action.contact, closer);
    }
    pending.insert(pending.end(), next.begin(), next.end());
  }
  return result;
}

} // namespace

TEST_CASE("a Kad lookup walks to the closest nodes and searches them")
{
  const std::string target(ed2k::HASH_LENGTH, '\0');
  KadTraversal traversal(
      ed2k::KadTraversalKind::KEYWORD_LOOKUP, target, 0,
      [](const KadContact& contact) { return rankOf(contact) < 5; }, 20);
  const auto result = walk(traversal, {150, 160, 170}, {});
  CHECK(traversal.done());
  for (int rank = 0; rank < 8; ++rank) {
    CHECK(result.asked.contains(rank));
    CHECK(result.searched.contains(
        rank)); // the closest that answered, after convergence
  }
  CHECK(result.searched.size() <= 20);
  CHECK(result.asked.size() < 60); // a walk, not a flood
}

TEST_CASE("a Kad lookup converges around nodes that never answer")
{
  const std::string target(ed2k::HASH_LENGTH, '\0');
  KadTraversal traversal(
      ed2k::KadTraversalKind::SOURCE_LOOKUP, target, 0,
      [](const KadContact& contact) { return rankOf(contact) < 3; }, 20);
  const auto result = walk(traversal, {200, 210, 220}, {1, 2});
  CHECK(traversal.done());
  CHECK(result.asked.contains(1));
  CHECK_FALSE(result.searched.contains(1));
  CHECK_FALSE(result.searched.contains(2));
  for (int rank : {0, 3, 4, 5, 6, 7, 8, 9}) {
    CAPTURE(rank);
    CHECK(result.searched.contains(rank));
  }
}

TEST_CASE("a Kad lookup without a search predicate only walks")
{
  const std::string target(ed2k::HASH_LENGTH, '\0');
  KadTraversal traversal(ed2k::KadTraversalKind::SOURCE_LOOKUP, target, 0, {},
                         20);
  const auto result = walk(traversal, {100}, {});
  CHECK(traversal.done());
  CHECK(result.searched.empty());
  CHECK(result.asked.contains(0));
}

TEST_CASE("the Kad routing table keeps many contacts in the far half")
{
  std::mt19937 random(11);
  const auto randomId = [&random] {
    std::string id(ed2k::HASH_LENGTH, '\0');
    for (auto& byte : id) {
      byte = static_cast<char>(random());
    }
    return id;
  };
  ed2k::KadRoutingTable table(randomId());
  for (int i = 0; i < 2000; ++i) {
    table.nodeSeen(
        {randomId(),
         "10.1." + std::to_string(i / 250) + '.' + std::to_string(i % 250 + 1),
         4672, 4662, 8, 0},
        1000);
  }
  // 15 far buckets of 10, then one per shared prefix length.
  CHECK(table.liveSize() > 150);
  CHECK(table.liveSize() < 250);

  ed2k::KadRoutingTable restored(table.snapshot().selfId);
  restored.restore(table.snapshot());
  CHECK(restored.liveSize() == table.liveSize());
  CHECK(restored.replacementSize() == table.replacementSize());

  // Refreshing a stale bucket looks up an ID inside it.
  std::string target;
  CHECK(restored.needRefresh(target, 1000)); // our own ID first
  CHECK(target == table.snapshot().selfId);
  CHECK(restored.needRefresh(target, 1000 + 901)); // our own again
  CHECK(restored.needRefresh(target, 1000 + 901)); // then the stalest bucket
  CHECK(target != table.snapshot().selfId);
}
