/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_ED2K_KAD_STATE_H
#define D_ED2K_KAD_STATE_H

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_kad_search.h"

namespace aria2 {

namespace ed2k {

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
  int64_t lastBootstrap = 0;
  int64_t lastRefresh = 0;
  int64_t lastSelfRefresh = 0;
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

class KadRoutingTable {
public:
  explicit KadRoutingTable(std::string selfId, size_t bucketSize = 10);

  void heardAbout(const KadContact& contact, int64_t now);
  void nodeSeen(const KadContact& contact, int64_t now);
  void nodeFailed(const KadContact& contact);
  std::vector<KadContact> findClosest(const std::string& targetId,
                                      size_t limit,
                                      bool includeUnconfirmed) const;
  bool needBootstrap(int64_t now);
  bool needRefresh(std::string& targetId, int64_t now);
  void addRouterNode(const Endpoint& endpoint);
  std::vector<Endpoint> getRouterNodes() const;
  KadRoutingSnapshot snapshot() const;
  void restore(const KadRoutingSnapshot& snapshot);
  size_t liveSize() const;
  size_t replacementSize() const;

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
  int64_t lastBootstrap_ = 0;
  int64_t lastRefresh_ = 0;
  int64_t lastSelfRefresh_ = 0;

  void addNode(const KadContact& contact, bool confirmed, int64_t now);
  size_t bucketIndex(const std::string& id) const;
};

class KadTraversal {
public:
  KadTraversal();
  KadTraversal(KadTraversalKind kind, std::string targetId, uint64_t size,
               size_t branchFactor = 3, size_t targetNodes = 8);

  std::vector<KadTraversalAction> start(const std::vector<KadContact>& seeds);
  std::vector<KadTraversalAction> onResponse(
      const KadContact& contact, const std::vector<KadContact>& closer);
  std::vector<KadTraversalAction> onFailure(const KadContact& contact);
  bool done() const { return done_; }
  KadTraversalKind kind() const { return kind_; }
  const std::string& targetId() const { return targetId_; }
  uint64_t size() const { return size_; }

private:
  struct Observer {
    KadContact contact;
    bool queried = false;
    bool alive = false;
    bool failed = false;
  };

  KadTraversalKind kind_ = KadTraversalKind::SOURCE_LOOKUP;
  std::string targetId_;
  uint64_t size_ = 0;
  size_t branchFactor_ = 3;
  size_t targetNodes_ = 8;
  size_t inFlight_ = 0;
  bool searchStarted_ = false;
  bool done_ = true;
  std::vector<Observer> observers_;

  void addContact(const KadContact& contact);
  std::vector<KadTraversalAction> nextActions();
  void startSearch(std::vector<KadTraversalAction>& actions);
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
                                   size_t startPosition,
                                   size_t limit) const;
  size_t size() const;

private:
  struct Bucket {
    std::string fileId;
    std::vector<KadSearchEntry> sources;
  };

  std::vector<Bucket> buckets_;
};

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_KAD_STATE_H
