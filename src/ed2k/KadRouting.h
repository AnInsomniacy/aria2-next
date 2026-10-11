/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ed2k/Hash.h"
#include "ed2k/KadProtocol.h"
#include "ed2k/KadSearch.h"

namespace aria2::ed2k {

struct KadRoutingNode {
  KadContact contact;
  bool confirmed = false;
  bool seed = false;
  uint32_t failCount = 0;
  int64_t firstSeen = 0;
  int64_t lastSeen = 0;
};

struct KadRoutingBucketSnapshot {
  std::vector<KadRoutingNode> live;
  std::vector<KadRoutingNode> replacements;
  int64_t lastActive = 0;
};

struct KadRoutingSnapshot {
  std::string selfId;
  std::vector<KadRoutingBucketSnapshot> buckets;
  std::vector<Endpoint> routerNodes;
  std::vector<KadContact> routerContacts;
  int64_t lastBootstrap = 0;
  int64_t lastRefresh = 0;
  int64_t lastSelfRefresh = 0;
  int64_t lastFirewalledCheck = 0;
  int64_t lastSourcePublish = 0;
  int64_t lastSourceSearch = 0;
  uint32_t sourceSearchCount = 0;
  uint32_t udpVerifyKey = 0;
  std::vector<std::string> observedAddresses;
  bool firewalled = true;
};

enum class KadTransactionPurpose {
  BOOTSTRAP,
  SOURCE_LOOKUP,
  KEYWORD_LOOKUP,
  REFRESH,
  FIREWALLED_CHECK,
};

enum class KadTraversalKind {
  SOURCE_LOOKUP,
  KEYWORD_LOOKUP,
};

enum class KadTraversalActionType {
  FIND_NODE,
  SEARCH,
};

struct KadTraversalAction {
  KadTraversalActionType type = KadTraversalActionType::FIND_NODE;
  KadContact contact;
};

// Contacts by XOR distance from us, at most `bucketSize` per bucket plus as
// many replacements. One bucket per shared prefix length, except that the far
// half of the network is split eight ways, the next quarter four and the
// eighth after it two, as eMule's routing zones split down to level 4: a
// lookup's first hop then starts much closer to any target.
class KadRoutingTable {
public:
  explicit KadRoutingTable(std::string selfId, size_t bucketSize = 10);

  void heardAbout(const KadContact& contact, int64_t now);
  void nodeSeen(const KadContact& contact, int64_t now);
  void nodeFailed(const KadContact& contact);
  std::vector<KadContact> findClosest(const std::string& targetId, size_t limit,
                                      bool includeUnconfirmed) const;
  std::vector<KadContact> findClosestExcluding(const std::string& targetId,
                                               const std::string& excludedId,
                                               size_t limit,
                                               bool includeUnconfirmed) const;
  bool needBootstrap(int64_t now);
  bool needRefresh(std::string& targetId, int64_t now);
  void addRouterNode(const Endpoint& endpoint);
  void addRouterNode(const KadContact& contact);
  std::vector<Endpoint> getRouterNodes() const;
  std::vector<KadContact> getRouterContacts() const;
  bool findByEndpoint(KadContact& contact, const Endpoint& endpoint) const;
  KadRoutingSnapshot snapshot() const;
  void restore(const KadRoutingSnapshot& snapshot);
  size_t liveSize() const;
  size_t replacementSize() const;
  size_t usefulSize() const;

private:
  struct Bucket {
    std::vector<KadRoutingNode> live;
    std::vector<KadRoutingNode> replacements;
    int64_t lastActive = 0;
  };

  std::string selfId_;
  size_t bucketSize_;
  std::vector<Bucket> buckets_;
  std::vector<Endpoint> routerNodes_;
  std::vector<KadContact> routerContacts_;
  int64_t lastBootstrap_ = 0;
  int64_t lastRefresh_ = 0;
  int64_t lastSelfRefresh_ = 0;

  void addNode(const KadContact& contact, bool confirmed, int64_t now);
  size_t bucketIndex(const std::string& id) const;
  // A random ID that falls into the bucket, to refresh it.
  std::string randomIdIn(size_t index) const;
};

// An iterative Kademlia lookup. FIND_NODE requests walk toward the target
// until the closest `targetNodes` contacts that did not fail have answered.
// Contacts the caller accepts as close enough are searched as soon as they
// answer; once the walk converged, the closest that answered are tried too.
class KadTraversal {
public:
  using Searchable = std::function<bool(const KadContact&)>;

  KadTraversal();
  KadTraversal(KadTraversalKind kind, std::string targetId, uint64_t size,
               Searchable searchable, size_t maxSearches,
               size_t branchFactor = 3, size_t targetNodes = 8);

  std::vector<KadTraversalAction> start(const std::vector<KadContact>& seeds);
  std::vector<KadTraversalAction>
  onResponse(const KadContact& contact, const std::vector<KadContact>& closer);
  std::vector<KadTraversalAction> onFailure(const KadContact& contact);
  // No answer for a while: one more request may be in flight.
  std::vector<KadTraversalAction> stalled();
  void onSearchResponse(const KadContact& contact);
  void onSearchFailure(const KadContact& contact);
  bool done() const { return done_; }
  KadTraversalKind kind() const { return kind_; }
  const std::string& targetId() const { return targetId_; }
  uint64_t size() const { return size_; }
  size_t queried() const { return queried_; }
  size_t answered() const { return answered_; }
  size_t searched() const { return searches_; }

private:
  struct Observer {
    KadContact contact;
    bool queried = false;
    bool alive = false;
    bool failed = false;
    bool searched = false;
  };

  KadTraversalKind kind_ = KadTraversalKind::SOURCE_LOOKUP;
  std::string targetId_;
  uint64_t size_ = 0;
  Searchable searchable_;
  size_t maxSearches_ = 0;
  size_t branchFactor_ = 3;
  size_t targetNodes_ = 8;
  size_t inFlight_ = 0;
  size_t extra_ = 0;
  size_t queried_ = 0;
  size_t answered_ = 0;
  size_t searches_ = 0;
  size_t searchInFlight_ = 0;
  bool converged_ = false;
  bool done_ = true;
  std::vector<Observer> observers_;

  Observer* find(const KadContact& contact);
  void addContact(const KadContact& contact);
  void finishSearch(const KadContact& contact, bool failed);
  void search(Observer& observer, bool anyAnswered,
              std::vector<KadTraversalAction>& actions);
  std::vector<KadTraversalAction> nextActions();
};

struct KadTransaction {
  Endpoint endpoint;
  KadContact contact;
  KadTransactionPurpose purpose = KadTransactionPurpose::BOOTSTRAP;
  uint8_t expectedOpcode = 0;
  std::string targetId;
  int64_t sentTime = 0;
};

class KadTransactionTable {
public:
  void add(const KadTransaction& transaction);
  bool complete(const Endpoint& endpoint, uint8_t opcode,
                KadTransaction& transaction);
  bool complete(const Endpoint& endpoint, uint8_t opcode,
                const std::string& targetId, KadTransaction& transaction);
  std::vector<KadTransaction> expire(int64_t now, int64_t timeoutSeconds);
  size_t size() const { return transactions_.size(); }

private:
  std::vector<KadTransaction> transactions_;
};

class KadSourceIndex {
public:
  void store(const std::string& fileId, const KadSearchEntry& source);
  std::vector<KadSearchEntry> find(const std::string& fileId,
                                   size_t startPosition, size_t limit) const;
  size_t size() const;

private:
  struct Bucket {
    std::string fileId;
    std::vector<KadSearchEntry> sources;
  };

  std::vector<Bucket> buckets_;
};

} // namespace aria2::ed2k
