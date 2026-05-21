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
#include "Ed2kKadState.h"

#include <algorithm>
#include <limits>

#include "DlAbortEx.h"
#include "fmt.h"

namespace aria2 {

namespace ed2k {

namespace {

bool sameContact(const KadContact& lhs, const KadContact& rhs)
{
  return (!lhs.id.empty() && lhs.id == rhs.id) ||
         (lhs.host == rhs.host && lhs.udpPort == rhs.udpPort);
}

bool sameEndpoint(const Endpoint& lhs, const Endpoint& rhs)
{
  return lhs.host == rhs.host && lhs.port == rhs.port;
}

bool sameSourceEndpoint(const KadSearchEntry& lhs, const KadSearchEntry& rhs)
{
  Endpoint lhsEndpoint;
  Endpoint rhsEndpoint;
  return extractKadSourceEndpoint(lhsEndpoint, lhs) &&
         extractKadSourceEndpoint(rhsEndpoint, rhs) &&
         sameEndpoint(lhsEndpoint, rhsEndpoint);
}

bool sameEndpoint(const KadContact& lhs, const KadContact& rhs)
{
  return lhs.host == rhs.host && lhs.udpPort == rhs.udpPort;
}

bool validId(const std::string& id) { return id.size() == HASH_LENGTH; }

bool validRoutingContact(const KadContact& contact)
{
  if (!validId(contact.id) || contact.host.empty() ||
      contact.host == "0.0.0.0" || contact.udpPort == 0 ||
      contact.version <= 1) {
    return false;
  }
  return contact.udpPort != 53 || contact.version > 5;
}

void validateId(const std::string& id)
{
  if (!validId(id)) {
    throw DL_ABORT_EX("Bad ED2K Kad id length.");
  }
}

int distanceCompare(const std::string& lhs, const std::string& rhs,
                    const std::string& target)
{
  validateId(lhs);
  validateId(rhs);
  validateId(target);
  for (size_t i = 0; i < HASH_LENGTH; ++i) {
    const auto ld = static_cast<unsigned char>(lhs[i]) ^
                    static_cast<unsigned char>(target[i]);
    const auto rd = static_cast<unsigned char>(rhs[i]) ^
                    static_cast<unsigned char>(target[i]);
    if (ld < rd) {
      return -1;
    }
    if (ld > rd) {
      return 1;
    }
  }
  return 0;
}

std::vector<KadRoutingNode>::iterator findNode(
    std::vector<KadRoutingNode>& nodes, const KadContact& contact)
{
  return std::find_if(nodes.begin(), nodes.end(),
                      [&](const KadRoutingNode& node) {
                        return sameContact(node.contact, contact);
                      });
}

std::vector<KadRoutingNode>::const_iterator findNode(
    const std::vector<KadRoutingNode>& nodes, const KadContact& contact)
{
  return std::find_if(nodes.begin(), nodes.end(),
                      [&](const KadRoutingNode& node) {
                        return sameContact(node.contact, contact);
                      });
}

void updateNode(KadRoutingNode& node, const KadContact& contact, bool confirmed,
                int64_t now)
{
  node.contact = contact;
  node.lastSeen = now;
  if (node.firstSeen == 0) {
    node.firstSeen = now;
  }
  if (confirmed) {
    node.confirmed = true;
    node.failCount = 0;
  }
}

} // namespace

KadRoutingTable::KadRoutingTable(std::string selfId, size_t bucketSize)
    : selfId_(std::move(selfId)),
      bucketSize_(bucketSize == 0 ? 10 : bucketSize),
      buckets_(128)
{
  validateId(selfId_);
}

void KadRoutingTable::heardAbout(const KadContact& contact, int64_t now)
{
  addNode(contact, false, now);
}

void KadRoutingTable::nodeSeen(const KadContact& contact, int64_t now)
{
  addNode(contact, true, now);
}

void KadRoutingTable::addNode(const KadContact& contact, bool confirmed,
                              int64_t now)
{
  if (!validRoutingContact(contact) || contact.id == selfId_) {
    return;
  }
  auto& bucket = buckets_[bucketIndex(contact.id)];
  bucket.lastActive = now;

  auto live = findNode(bucket.live, contact);
  if (live != bucket.live.end()) {
    updateNode(*live, contact, confirmed, now);
    auto node = *live;
    bucket.live.erase(live);
    bucket.live.push_back(node);
    return;
  }

  auto replacement = findNode(bucket.replacements, contact);
  if (replacement != bucket.replacements.end()) {
    updateNode(*replacement, contact, confirmed, now);
    if (confirmed && bucket.live.size() < bucketSize_) {
      auto node = *replacement;
      bucket.replacements.erase(replacement);
      bucket.live.push_back(node);
    }
    return;
  }

  KadRoutingNode node;
  node.contact = contact;
  node.confirmed = confirmed;
  node.firstSeen = now;
  node.lastSeen = now;
  if (bucket.live.size() < bucketSize_) {
    bucket.live.push_back(node);
    return;
  }
  if (bucket.replacements.size() == bucketSize_) {
    bucket.replacements.erase(bucket.replacements.begin());
  }
  bucket.replacements.push_back(node);
}

void KadRoutingTable::nodeFailed(const KadContact& contact)
{
  if (!validId(contact.id)) {
    return;
  }
  auto& bucket = buckets_[bucketIndex(contact.id)];
  auto live = findNode(bucket.live, contact);
  if (live != bucket.live.end()) {
    if (!live->confirmed) {
      bucket.live.erase(live);
      return;
    }
    ++live->failCount;
    if (!bucket.replacements.empty()) {
      *live = bucket.replacements.front();
      bucket.replacements.erase(bucket.replacements.begin());
      return;
    }
    if (live->failCount >= 20) {
      bucket.live.erase(live);
    }
    return;
  }
  auto replacement = findNode(bucket.replacements, contact);
  if (replacement != bucket.replacements.end()) {
    bucket.replacements.erase(replacement);
  }
}

std::vector<KadContact> KadRoutingTable::findClosest(
    const std::string& targetId, size_t limit, bool includeUnconfirmed) const
{
  return findClosestExcluding(targetId, std::string(), limit,
                              includeUnconfirmed);
}

std::vector<KadContact> KadRoutingTable::findClosestExcluding(
    const std::string& targetId, const std::string& excludedId, size_t limit,
    bool includeUnconfirmed) const
{
  validateId(targetId);
  if (!excludedId.empty()) {
    validateId(excludedId);
  }
  std::vector<KadRoutingNode> nodes;
  for (const auto& bucket : buckets_) {
    for (const auto& node : bucket.live) {
      if ((includeUnconfirmed || node.confirmed) &&
          node.contact.id != excludedId) {
        nodes.push_back(node);
      }
    }
  }
  std::sort(nodes.begin(), nodes.end(),
            [&](const KadRoutingNode& lhs, const KadRoutingNode& rhs) {
              return distanceCompare(lhs.contact.id, rhs.contact.id,
                                     targetId) < 0;
            });
  if (limit > 0 && nodes.size() > limit) {
    nodes.resize(limit);
  }
  std::vector<KadContact> contacts;
  contacts.reserve(nodes.size());
  for (const auto& node : nodes) {
    contacts.push_back(node.contact);
  }
  return contacts;
}

bool KadRoutingTable::needBootstrap(int64_t now)
{
  if (liveSize() != 0 || replacementSize() >= bucketSize_) {
    return false;
  }
  if (lastBootstrap_ == 0 || now - lastBootstrap_ >= 30) {
    lastBootstrap_ = now;
    return true;
  }
  return false;
}

bool KadRoutingTable::needRefresh(std::string& targetId, int64_t now)
{
  if (lastSelfRefresh_ == 0 || now - lastSelfRefresh_ >= 900) {
    lastSelfRefresh_ = now;
    targetId = selfId_;
    return true;
  }
  if (lastRefresh_ != 0 && now - lastRefresh_ < 45) {
    return false;
  }
  auto oldest = buckets_.end();
  for (auto i = buckets_.begin(); i != buckets_.end(); ++i) {
    if (i->lastActive == 0 || now - i->lastActive < 900) {
      continue;
    }
    if (oldest == buckets_.end() || i->lastActive < oldest->lastActive) {
      oldest = i;
    }
  }
  if (oldest == buckets_.end()) {
    return false;
  }
  lastRefresh_ = now;
  auto index = static_cast<size_t>(oldest - buckets_.begin());
  targetId = selfId_;
  auto byteIndex = index / 8;
  auto bitIndex = index % 8;
  targetId[byteIndex] =
      static_cast<char>(static_cast<unsigned char>(targetId[byteIndex]) ^
                        (0x80u >> bitIndex));
  return true;
}

void KadRoutingTable::addRouterNode(const Endpoint& endpoint)
{
  if (endpoint.host.empty() || endpoint.port == 0) {
    return;
  }
  if (std::find_if(routerNodes_.begin(), routerNodes_.end(),
                   [&](const Endpoint& item) {
                     return sameEndpoint(item, endpoint);
                   }) == routerNodes_.end()) {
    routerNodes_.push_back(endpoint);
  }
}

std::vector<Endpoint> KadRoutingTable::getRouterNodes() const
{
  return routerNodes_;
}

KadRoutingSnapshot KadRoutingTable::snapshot() const
{
  KadRoutingSnapshot snapshot;
  snapshot.selfId = selfId_;
  snapshot.lastBootstrap = lastBootstrap_;
  snapshot.lastRefresh = lastRefresh_;
  snapshot.lastSelfRefresh = lastSelfRefresh_;
  snapshot.routerNodes = routerNodes_;
  snapshot.buckets.reserve(buckets_.size());
  for (const auto& bucket : buckets_) {
    KadRoutingBucketSnapshot item;
    item.live = bucket.live;
    item.replacements = bucket.replacements;
    item.lastActive = bucket.lastActive;
    snapshot.buckets.push_back(std::move(item));
  }
  return snapshot;
}

void KadRoutingTable::restore(const KadRoutingSnapshot& snapshot)
{
  validateId(snapshot.selfId);
  routerNodes_ = snapshot.routerNodes;
  lastBootstrap_ = snapshot.lastBootstrap;
  lastRefresh_ = snapshot.lastRefresh;
  lastSelfRefresh_ = snapshot.lastSelfRefresh;
  buckets_.assign(128, Bucket());
  const auto count = std::min(buckets_.size(), snapshot.buckets.size());
  for (size_t i = 0; i < count; ++i) {
    buckets_[i].live = snapshot.buckets[i].live;
    buckets_[i].replacements = snapshot.buckets[i].replacements;
    buckets_[i].lastActive = snapshot.buckets[i].lastActive;
  }
}

size_t KadRoutingTable::liveSize() const
{
  size_t size = 0;
  for (const auto& bucket : buckets_) {
    size += bucket.live.size();
  }
  return size;
}

size_t KadRoutingTable::replacementSize() const
{
  size_t size = 0;
  for (const auto& bucket : buckets_) {
    size += bucket.replacements.size();
  }
  return size;
}

size_t KadRoutingTable::usefulSize() const
{
  return liveSize() + replacementSize();
}

size_t KadRoutingTable::bucketIndex(const std::string& id) const
{
  validateId(id);
  for (size_t i = 0; i < HASH_LENGTH; ++i) {
    const auto x = static_cast<unsigned char>(selfId_[i]) ^
                   static_cast<unsigned char>(id[i]);
    if (x == 0) {
      continue;
    }
    for (size_t bit = 0; bit < 8; ++bit) {
      if (x & (0x80u >> bit)) {
        return i * 8 + bit;
      }
    }
  }
  return 127;
}

KadTraversal::KadTraversal() = default;

KadTraversal::KadTraversal(KadTraversalKind kind, std::string targetId,
                           uint64_t size, size_t branchFactor,
                           size_t targetNodes)
    : kind_(kind),
      targetId_(std::move(targetId)),
      size_(size),
      branchFactor_(branchFactor == 0 ? 3 : branchFactor),
      targetNodes_(targetNodes == 0 ? 8 : targetNodes),
      done_(false)
{
  validateId(targetId_);
}

std::vector<KadTraversalAction> KadTraversal::start(
    const std::vector<KadContact>& seeds)
{
  for (const auto& seed : seeds) {
    addContact(seed);
  }
  return nextActions();
}

std::vector<KadTraversalAction> KadTraversal::onResponse(
    const KadContact& contact, const std::vector<KadContact>& closer)
{
  for (auto& observer : observers_) {
    if (sameContact(observer.contact, contact) ||
        sameEndpoint(observer.contact, contact)) {
      if (!observer.alive) {
        observer.alive = true;
      }
      if (inFlight_ > 0) {
        --inFlight_;
      }
      break;
    }
  }
  for (const auto& item : closer) {
    addContact(item);
  }
  return nextActions();
}

std::vector<KadTraversalAction> KadTraversal::onFailure(
    const KadContact& contact)
{
  for (auto& observer : observers_) {
    if (sameContact(observer.contact, contact) ||
        sameEndpoint(observer.contact, contact)) {
      if (!observer.failed) {
        observer.failed = true;
      }
      if (inFlight_ > 0) {
        --inFlight_;
      }
      break;
    }
  }
  return nextActions();
}

void KadTraversal::addContact(const KadContact& contact)
{
  if (!validId(contact.id) || contact.host.empty() || contact.udpPort == 0) {
    return;
  }
  auto i = std::find_if(observers_.begin(), observers_.end(),
                        [&](const Observer& observer) {
                          return sameContact(observer.contact, contact) ||
                                 sameEndpoint(observer.contact, contact);
                        });
  if (i != observers_.end()) {
    if (i->contact.id.empty()) {
      i->contact.id = contact.id;
    }
    if (i->contact.tcpPort == 0) {
      i->contact.tcpPort = contact.tcpPort;
    }
    if (i->contact.version == 0) {
      i->contact.version = contact.version;
    }
    return;
  }

  Observer observer;
  observer.contact = contact;
  auto insertPos = std::lower_bound(
      observers_.begin(), observers_.end(), observer,
      [&](const Observer& lhs, const Observer& rhs) {
        return distanceCompare(lhs.contact.id, rhs.contact.id, targetId_) < 0;
      });
  observers_.insert(insertPos, observer);
  if (observers_.size() > 100) {
    observers_.resize(100);
  }
}

std::vector<KadTraversalAction> KadTraversal::nextActions()
{
  std::vector<KadTraversalAction> actions;
  if (done_) {
    return actions;
  }

  size_t alive = 0;
  for (const auto& observer : observers_) {
    if (observer.alive && !observer.failed) {
      ++alive;
    }
  }

  for (auto& observer : observers_) {
    if (inFlight_ >= branchFactor_ || alive >= targetNodes_) {
      break;
    }
    if (observer.queried || observer.failed) {
      continue;
    }
    observer.queried = true;
    ++inFlight_;
    KadTraversalAction action;
    action.type = KadTraversalActionType::FIND_NODE;
    action.contact = observer.contact;
    actions.push_back(action);
  }

  if (!actions.empty() || inFlight_ != 0) {
    return actions;
  }

  startSearch(actions);
  return actions;
}

void KadTraversal::startSearch(std::vector<KadTraversalAction>& actions)
{
  if (searchStarted_) {
    done_ = true;
    return;
  }
  searchStarted_ = true;
  for (const auto& observer : observers_) {
    if (observer.failed) {
      continue;
    }
    KadTraversalAction action;
    action.type = KadTraversalActionType::SEARCH;
    action.contact = observer.contact;
    actions.push_back(action);
    if (actions.size() >= targetNodes_) {
      break;
    }
  }
  if (actions.empty()) {
    done_ = true;
  }
}

void KadTransactionTable::add(const KadTransaction& transaction)
{
  transactions_.push_back(transaction);
}

bool KadTransactionTable::complete(const Endpoint& endpoint, uint8_t opcode,
                                   KadTransaction& transaction)
{
  auto i = std::find_if(transactions_.begin(), transactions_.end(),
                        [&](const KadTransaction& item) {
                          return item.expectedOpcode == opcode &&
                                 sameEndpoint(item.endpoint, endpoint);
                        });
  if (i == transactions_.end()) {
    return false;
  }
  transaction = *i;
  transactions_.erase(i);
  return true;
}

bool KadTransactionTable::complete(const Endpoint& endpoint, uint8_t opcode,
                                   const std::string& targetId,
                                   KadTransaction& transaction)
{
  auto i = std::find_if(transactions_.begin(), transactions_.end(),
                        [&](const KadTransaction& item) {
                          return item.expectedOpcode == opcode &&
                                 item.targetId == targetId &&
                                 sameEndpoint(item.endpoint, endpoint);
                        });
  if (i == transactions_.end()) {
    return false;
  }
  transaction = *i;
  transactions_.erase(i);
  return true;
}

std::vector<KadTransaction> KadTransactionTable::expire(int64_t now,
                                                        int64_t timeoutSeconds)
{
  std::vector<KadTransaction> expired;
  auto i = transactions_.begin();
  while (i != transactions_.end()) {
    if (now - i->sentTime >= timeoutSeconds) {
      expired.push_back(*i);
      i = transactions_.erase(i);
    }
    else {
      ++i;
    }
  }
  return expired;
}

void KadSourceIndex::store(const std::string& fileId,
                           const KadSearchEntry& source)
{
  validateId(fileId);
  validateId(source.id);
  auto bucket = std::find_if(buckets_.begin(), buckets_.end(),
                             [&](const Bucket& item) {
                               return item.fileId == fileId;
                             });
  if (bucket == buckets_.end()) {
    Bucket item;
    item.fileId = fileId;
    buckets_.push_back(std::move(item));
    bucket = buckets_.end() - 1;
  }
  auto sourcePos = std::find_if(bucket->sources.begin(), bucket->sources.end(),
                                [&](const KadSearchEntry& item) {
                                  return item.id == source.id ||
                                         sameSourceEndpoint(item, source);
                                });
  if (sourcePos == bucket->sources.end()) {
    bucket->sources.push_back(source);
  }
  else {
    *sourcePos = source;
  }
}

std::vector<KadSearchEntry> KadSourceIndex::find(const std::string& fileId,
                                                 size_t startPosition,
                                                 size_t limit) const
{
  validateId(fileId);
  auto bucket = std::find_if(buckets_.begin(), buckets_.end(),
                             [&](const Bucket& item) {
                               return item.fileId == fileId;
                             });
  if (bucket == buckets_.end() || startPosition >= bucket->sources.size()) {
    return std::vector<KadSearchEntry>();
  }
  const auto end = limit == 0
                       ? bucket->sources.size()
                       : std::min(bucket->sources.size(), startPosition + limit);
  return std::vector<KadSearchEntry>(bucket->sources.begin() + startPosition,
                                     bucket->sources.begin() + end);
}

size_t KadSourceIndex::size() const
{
  size_t total = 0;
  for (const auto& bucket : buckets_) {
    total += bucket.sources.size();
  }
  return total;
}

} // namespace ed2k

} // namespace aria2
