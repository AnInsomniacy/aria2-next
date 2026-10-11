/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/KadRouting.h"
#include "ed2k/Hash.h"
#include "ed2k/KadProtocol.h"
#include "ed2k/KadSearch.h"
#include "ed2k/Link.h"
#include "ed2k/Packet.h"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <algorithm>
#include <random>

namespace aria2::ed2k {

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
    throw ProtocolError("Bad ED2K Kad id length.");
  }
}

int distanceCompare(const std::string& lhs, const std::string& rhs,
                    const std::string& target)
{
  validateId(lhs);
  validateId(rhs);
  validateId(target);
  for (size_t i = 0; i < HASH_LENGTH; i += 4) {
    const auto ld = readUInt32(lhs.data() + i) ^ readUInt32(target.data() + i);
    const auto rd = readUInt32(rhs.data() + i) ^ readUInt32(target.data() + i);
    if (ld < rd) {
      return -1;
    }
    if (ld > rd) {
      return 1;
    }
  }
  return 0;
}

std::vector<KadRoutingNode>::iterator
findNode(std::vector<KadRoutingNode>& nodes, const KadContact& contact)
{
  return std::find_if(nodes.begin(), nodes.end(),
                      [&](const KadRoutingNode& node) {
                        return sameContact(node.contact, contact);
                      });
}

std::vector<KadRoutingNode>::const_iterator
findNode(const std::vector<KadRoutingNode>& nodes, const KadContact& contact)
{
  return std::find_if(nodes.begin(), nodes.end(),
                      [&](const KadRoutingNode& node) {
                        return sameContact(node.contact, contact);
                      });
}

void updateNode(KadRoutingNode& node, const KadContact& contact, bool confirmed,
                int64_t now)
{
  const auto oldUdpKey = node.contact.udpKey;
  node.contact = contact;
  if (node.contact.udpKey == 0) {
    node.contact.udpKey = oldUdpKey;
  }
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

namespace {

constexpr size_t splitLevels = 4;
constexpr size_t farBuckets =
    (size_t{1} << splitLevels) - 1; // by the top 4 distance bits
constexpr size_t bucketCount = farBuckets + 128 - splitLevels;

} // namespace

KadRoutingTable::KadRoutingTable(std::string selfId, size_t bucketSize)
    : selfId_(std::move(selfId)),
      bucketSize_(bucketSize == 0 ? 10 : bucketSize),
      buckets_(bucketCount)
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

std::vector<KadContact>
KadRoutingTable::findClosest(const std::string& targetId, size_t limit,
                             bool includeUnconfirmed) const
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
              return distanceCompare(lhs.contact.id, rhs.contact.id, targetId) <
                     0;
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
  if (lastBootstrap_ == 0 || now - lastBootstrap_ >= 2) {
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
  targetId = randomIdIn(static_cast<size_t>(oldest - buckets_.begin()));
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

void KadRoutingTable::addRouterNode(const KadContact& contact)
{
  if (!validRoutingContact(contact)) {
    return;
  }
  Endpoint endpoint;
  endpoint.host = contact.host;
  endpoint.port = contact.udpPort;
  addRouterNode(endpoint);
  auto existing = std::find_if(routerContacts_.begin(), routerContacts_.end(),
                               [&](const KadContact& item) {
                                 return sameContact(item, contact) ||
                                        sameEndpoint(item, contact);
                               });
  if (existing == routerContacts_.end()) {
    routerContacts_.push_back(contact);
    return;
  }
  *existing = contact;
}

std::vector<Endpoint> KadRoutingTable::getRouterNodes() const
{
  return routerNodes_;
}

std::vector<KadContact> KadRoutingTable::getRouterContacts() const
{
  return routerContacts_;
}

bool KadRoutingTable::findByEndpoint(KadContact& contact,
                                     const Endpoint& endpoint) const
{
  for (const auto& bucket : buckets_) {
    for (const auto& node : bucket.live) {
      if (node.contact.host == endpoint.host &&
          node.contact.udpPort == endpoint.port) {
        contact = node.contact;
        return true;
      }
    }
    for (const auto& node : bucket.replacements) {
      if (node.contact.host == endpoint.host &&
          node.contact.udpPort == endpoint.port) {
        contact = node.contact;
        return true;
      }
    }
  }
  auto router = std::find_if(routerContacts_.begin(), routerContacts_.end(),
                             [&](const KadContact& item) {
                               return item.host == endpoint.host &&
                                      item.udpPort == endpoint.port;
                             });
  if (router == routerContacts_.end()) {
    return false;
  }
  contact = *router;
  return true;
}

KadRoutingSnapshot KadRoutingTable::snapshot() const
{
  KadRoutingSnapshot snapshot;
  snapshot.selfId = selfId_;
  snapshot.lastBootstrap = lastBootstrap_;
  snapshot.lastRefresh = lastRefresh_;
  snapshot.lastSelfRefresh = lastSelfRefresh_;
  snapshot.routerNodes = routerNodes_;
  snapshot.routerContacts = routerContacts_;
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
  routerContacts_ = snapshot.routerContacts;
  lastBootstrap_ = snapshot.lastBootstrap;
  lastRefresh_ = snapshot.lastRefresh;
  lastSelfRefresh_ = snapshot.lastSelfRefresh;
  // Placed again, as a snapshot may come from another bucket layout.
  buckets_.assign(bucketCount, Bucket());
  for (const auto& saved : snapshot.buckets) {
    for (const auto* nodes : {&saved.live, &saved.replacements}) {
      for (const auto& node : *nodes) {
        if (!validId(node.contact.id)) {
          continue;
        }
        auto& bucket = buckets_[bucketIndex(node.contact.id)];
        bucket.lastActive = std::max(bucket.lastActive, saved.lastActive);
        if (nodes == &saved.live && bucket.live.size() < bucketSize_) {
          bucket.live.push_back(node);
        }
        else if (bucket.replacements.size() < bucketSize_) {
          bucket.replacements.push_back(node);
        }
      }
    }
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
  for (size_t chunk = 0; chunk < HASH_LENGTH / 4; ++chunk) {
    const auto offset = chunk * 4;
    const auto x =
        readUInt32(selfId_.data() + offset) ^ readUInt32(id.data() + offset);
    if (x == 0) {
      continue;
    }
    if (chunk == 0 && (x >> (32 - splitLevels)) != 0) {
      return (x >> (32 - splitLevels)) - 1;
    }
    for (size_t bit = 0; bit < 32; ++bit) {
      if (x & (0x80000000u >> bit)) {
        return farBuckets + chunk * 32 + bit - splitLevels;
      }
    }
  }
  return bucketCount - 1;
}

std::string KadRoutingTable::randomIdIn(size_t index) const
{
  static thread_local std::minstd_rand random{std::random_device{}()};
  // The distance: its leading bits pick the bucket, the rest are random.
  uint32_t words[HASH_LENGTH / 4];
  for (auto& word : words) {
    word = static_cast<uint32_t>(random()) ^
           (static_cast<uint32_t>(random()) << 16);
  }
  if (index < farBuckets) {
    words[0] = (static_cast<uint32_t>(index + 1) << (32 - splitLevels)) |
               (words[0] >> splitLevels);
  }
  else {
    const auto prefix = std::min<size_t>(index - farBuckets + splitLevels, 127);
    for (size_t chunk = 0; chunk < prefix / 32; ++chunk) {
      words[chunk] = 0;
    }
    const auto bit = 0x80000000u >> (prefix % 32);
    words[prefix / 32] = bit | (words[prefix / 32] & (bit - 1));
  }
  std::string id = selfId_;
  for (size_t chunk = 0; chunk < HASH_LENGTH / 4; ++chunk) {
    const auto offset = chunk * 4;
    id.replace(offset, 4,
               packUInt32(readUInt32(id.data() + offset) ^ words[chunk]));
  }
  return id;
}

KadTraversal::KadTraversal() = default;

KadTraversal::KadTraversal(KadTraversalKind kind, std::string targetId,
                           uint64_t size, Searchable searchable,
                           size_t maxSearches, size_t branchFactor,
                           size_t targetNodes)
    : kind_(kind),
      targetId_(std::move(targetId)),
      size_(size),
      searchable_(std::move(searchable)),
      maxSearches_(maxSearches),
      branchFactor_(branchFactor == 0 ? 3 : branchFactor),
      targetNodes_(targetNodes == 0 ? 8 : targetNodes),
      done_(false)
{
  validateId(targetId_);
}

std::vector<KadTraversalAction>
KadTraversal::start(const std::vector<KadContact>& seeds)
{
  for (const auto& seed : seeds) {
    addContact(seed);
  }
  return nextActions();
}

KadTraversal::Observer* KadTraversal::find(const KadContact& contact)
{
  for (auto& observer : observers_) {
    if (sameContact(observer.contact, contact) ||
        sameEndpoint(observer.contact, contact)) {
      return &observer;
    }
  }
  return nullptr;
}

std::vector<KadTraversalAction>
KadTraversal::onResponse(const KadContact& contact,
                         const std::vector<KadContact>& closer)
{
  std::vector<KadTraversalAction> actions;
  if (done_) {
    return actions;
  }
  if (auto* observer = find(contact);
      observer && observer->queried && !observer->alive) {
    observer->alive = true;
    observer->failed = false;
    ++answered_;
    if (inFlight_ > 0) {
      --inFlight_;
    }
    search(*observer, false, actions);
  }
  for (const auto& item : closer) {
    addContact(item);
  }
  auto next = nextActions();
  actions.insert(actions.end(), next.begin(), next.end());
  return actions;
}

std::vector<KadTraversalAction>
KadTraversal::onFailure(const KadContact& contact)
{
  if (auto* observer = find(contact);
      observer && !observer->failed && !observer->alive) {
    observer->failed = true;
    if (inFlight_ > 0) {
      --inFlight_;
    }
  }
  return nextActions();
}

std::vector<KadTraversalAction> KadTraversal::stalled()
{
  if (extra_ < 2 * branchFactor_) {
    ++extra_;
  }
  return nextActions();
}

void KadTraversal::onSearchResponse(const KadContact& contact)
{
  finishSearch(contact, false);
}

void KadTraversal::onSearchFailure(const KadContact& contact)
{
  finishSearch(contact, true);
}

void KadTraversal::finishSearch(const KadContact& contact, bool failed)
{
  if (auto* observer = find(contact); observer && failed) {
    observer->failed = true;
  }
  if (searchInFlight_ > 0) {
    --searchInFlight_;
  }
  if (converged_ && searchInFlight_ == 0) {
    done_ = true;
  }
}

// `anyAnswered` lifts the closeness test, for the closest contacts once the
// walk converged.
void KadTraversal::search(Observer& observer, bool anyAnswered,
                          std::vector<KadTraversalAction>& actions)
{
  if (observer.searched || searches_ >= maxSearches_ || !searchable_ ||
      (!anyAnswered && !searchable_(observer.contact))) {
    return;
  }
  observer.searched = true;
  ++searches_;
  ++searchInFlight_;
  actions.push_back({KadTraversalActionType::SEARCH, observer.contact});
}

void KadTraversal::addContact(const KadContact& contact)
{
  if (!validId(contact.id) || contact.host.empty() || contact.udpPort == 0) {
    return;
  }
  if (auto* known = find(contact)) {
    if (known->contact.tcpPort == 0) {
      known->contact.tcpPort = contact.tcpPort;
    }
    if (known->contact.version == 0) {
      known->contact.version = contact.version;
    }
    if (known->contact.udpKey == 0) {
      known->contact.udpKey = contact.udpKey;
    }
    return;
  }
  Observer observer;
  observer.contact = contact;
  auto at = std::lower_bound(
      observers_.begin(), observers_.end(), observer,
      [&](const Observer& lhs, const Observer& rhs) {
        return distanceCompare(lhs.contact.id, rhs.contact.id, targetId_) < 0;
      });
  observers_.insert(at, observer);
  // Far contacts that were never asked are of no further use.
  while (observers_.size() > 128 && !observers_.back().queried) {
    observers_.pop_back();
  }
}

std::vector<KadTraversalAction> KadTraversal::nextActions()
{
  std::vector<KadTraversalAction> actions;
  if (done_ || converged_) {
    return actions;
  }
  // The walk converged when the closest contacts that did not fail all
  // answered.
  size_t considered = 0;
  bool converged = true;
  for (auto& observer : observers_) {
    if (observer.failed) {
      continue;
    }
    if (++considered > targetNodes_) {
      break;
    }
    if (observer.alive) {
      continue;
    }
    converged = false;
    if (!observer.queried && inFlight_ < branchFactor_ + extra_) {
      observer.queried = true;
      ++queried_;
      ++inFlight_;
      actions.push_back({KadTraversalActionType::FIND_NODE, observer.contact});
    }
  }
  if (!converged) {
    return actions;
  }
  converged_ = true;
  considered = 0;
  for (auto& observer : observers_) {
    if (observer.alive && ++considered <= targetNodes_) {
      search(observer, true, actions);
    }
  }
  if (searchInFlight_ == 0) {
    done_ = true;
  }
  return actions;
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
  auto bucket =
      std::find_if(buckets_.begin(), buckets_.end(),
                   [&](const Bucket& item) { return item.fileId == fileId; });
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
  auto bucket =
      std::find_if(buckets_.begin(), buckets_.end(),
                   [&](const Bucket& item) { return item.fileId == fileId; });
  if (bucket == buckets_.end() || startPosition >= bucket->sources.size()) {
    return std::vector<KadSearchEntry>();
  }
  const auto end =
      limit == 0 ? bucket->sources.size()
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

} // namespace aria2::ed2k
