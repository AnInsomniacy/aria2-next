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
#include "Ed2kAttribute.h"

#include <algorithm>
#include <chrono>

#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "DlAbortEx.h"
#include "Ed2kCommand.h"
#include "Option.h"
#include "RequestGroup.h"
#include "SimpleRandomizer.h"
#include "a2functional.h"
#include "ed2k_hash.h"
#include "ed2k_policy.h"
#include "prefs.h"
#include "util.h"
#include "wallclock.h"

namespace aria2 {

Ed2kAttribute* getEd2kAttrs(const std::shared_ptr<DownloadContext>& dctx)
{
  return getEd2kAttrs(dctx.get());
}

Ed2kAttribute* getEd2kAttrs(DownloadContext* dctx)
{
  return static_cast<Ed2kAttribute*>(dctx->getAttribute(CTX_ATTR_ED2K).get());
}

std::string normalizeEd2kClientHash(std::string clientHash)
{
  if (clientHash.size() >= ed2k::HASH_LENGTH) {
    clientHash = clientHash.substr(0, ed2k::HASH_LENGTH);
  }
  else {
    clientHash.append(ed2k::HASH_LENGTH - clientHash.size(), '\0');
  }
  clientHash[5] = 14;
  clientHash[14] = 111;
  return clientHash;
}

std::string createEd2kClientHash()
{
  std::string clientHash(ed2k::HASH_LENGTH, '\0');
  SimpleRandomizer::getInstance()->getRandomBytes(
      reinterpret_cast<unsigned char*>(&clientHash[0]), clientHash.size());
  return normalizeEd2kClientHash(std::move(clientHash));
}

std::string getOrCreateEd2kClientHash(Option* option)
{
  if (!option->blank(PREF_ED2K_CLIENT_HASH)) {
    const auto value = option->get(PREF_ED2K_CLIENT_HASH);
    if (value.size() != ed2k::HASH_LENGTH * 2 || !util::isHexDigit(value)) {
      throw DL_ABORT_EX("Cannot parse ED2K client hash.");
    }
    const auto clientHash =
        normalizeEd2kClientHash(util::fromHex(value.begin(), value.end()));
    option->put(PREF_ED2K_CLIENT_HASH, util::toHex(clientHash));
    return clientHash;
  }
  const auto clientHash = createEd2kClientHash();
  option->put(PREF_ED2K_CLIENT_HASH, util::toHex(clientHash));
  return clientHash;
}

bool addUniqueEndpoint(std::vector<ed2k::Endpoint>& endpoints,
                       const ed2k::Endpoint& endpoint)
{
  if (endpoint.host.empty() || endpoint.port == 0) {
    return false;
  }
  auto i = std::find_if(endpoints.begin(), endpoints.end(),
                        [&](const ed2k::Endpoint& item) {
                          return item.host == endpoint.host &&
                                 item.port == endpoint.port;
                        });
  if (i != endpoints.end()) {
    return false;
  }
  endpoints.push_back(endpoint);
  return true;
}

bool sameEndpoint(const ed2k::Endpoint& lhs, const ed2k::Endpoint& rhs)
{
  return lhs.host == rhs.host && lhs.port == rhs.port;
}

bool isFilteredSourceExchangePeer(const ed2k::Endpoint& peer,
                                  const ed2k::Endpoint& remotePeer)
{
  if (peer.host.empty() || peer.port == 0) {
    return true;
  }
  if (sameEndpoint(peer, remotePeer)) {
    return true;
  }
  if (util::isNumericHost(peer.host) &&
      (peer.host == "0.0.0.0" || peer.host == "127.0.0.1" ||
       peer.host == "::" || peer.host == "::1")) {
    return true;
  }
  return false;
}

bool matchesSearchQuery(const ed2k::SearchResultEntry& entry,
                        const ed2k::SearchQuery& query)
{
  if (query.minSize > 0 && entry.size > 0 && entry.size < query.minSize) {
    return false;
  }
  if (query.maxSize > 0 && entry.size > query.maxSize) {
    return false;
  }
  if (query.minSourceCount > 0 && entry.sourceCount < query.minSourceCount) {
    return false;
  }
  if (query.minCompleteSourceCount > 0 &&
      entry.completeSourceCount < query.minCompleteSourceCount) {
    return false;
  }
  return true;
}

bool addEd2kPeer(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  return addEd2kPeer(attrs, peer, 0);
}

bool addEd2kPeer(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                 uint32_t sourceFlag)
{
  if (!attrs || !addUniqueEndpoint(attrs->peers, peer)) {
    auto state = getEd2kPeerState(attrs, peer);
    if (state) {
      if (!peer.userHash.empty() && state->endpoint.userHash.empty()) {
        state->endpoint.userHash = peer.userHash;
      }
      if (peer.cryptOptions != 0 && state->endpoint.cryptOptions == 0) {
        state->endpoint.cryptOptions = peer.cryptOptions;
      }
      state->sourceFlags |= sourceFlag;
    }
    return false;
  }
  auto state = getEd2kPeerState(attrs, peer);
  if (state) {
    state->sourceFlags |= sourceFlag;
  }
  return true;
}

bool addEd2kFoundSource(Ed2kAttribute* attrs, const ed2k::FoundSource& source,
                        uint32_t sourceFlag, bool callbackRequested)
{
  if (!attrs || source.endpoint.host.empty() || source.endpoint.port == 0) {
    return false;
  }
  if (!source.lowId) {
    return addEd2kPeer(attrs, source.endpoint, sourceFlag);
  }
  auto state = getEd2kPeerState(attrs, source.endpoint);
  if (!state) {
    return false;
  }
  if (!source.endpoint.userHash.empty() && state->endpoint.userHash.empty()) {
    state->endpoint.userHash = source.endpoint.userHash;
  }
  if (source.endpoint.cryptOptions != 0 &&
      state->endpoint.cryptOptions == 0) {
    state->endpoint.cryptOptions = source.endpoint.cryptOptions;
  }
  state->sourceFlags |= sourceFlag;
  state->clientId = source.clientId;
  state->lowId = true;
  if (callbackRequested) {
    state->callbackRequested = true;
    state->callbackImpossible = false;
  }
  else if (!state->callbackRequested) {
    state->callbackImpossible = true;
  }
  state->connecting = false;
  state->accepted = false;
  state->queued = false;
  state->requestedParts.clear();
  return false;
}

size_t mergeEd2kServerSources(Ed2kAttribute* attrs,
                              const std::vector<ed2k::FoundSource>& sources,
                              uint32_t sourceFlag)
{
  if (!attrs) {
    return 0;
  }
  size_t added = 0;
  for (const auto& source : sources) {
    if (source.lowId) {
      addEd2kFoundSource(attrs, source, sourceFlag, false);
      continue;
    }
    if ((source.endpoint.cryptOptions & ed2k::SOURCE_CRYPT_REQUIRE) != 0) {
      continue;
    }
    if (addEd2kFoundSource(attrs, source, sourceFlag, false)) {
      ++added;
    }
  }
  return added;
}

size_t mergeEd2kSourceExchangePeers(
    Ed2kAttribute* attrs, const std::vector<ed2k::SourceExchangeEntry>& entries,
    const ed2k::Endpoint& remotePeer)
{
  if (!attrs) {
    return 0;
  }
  size_t added = 0;
  for (const auto& entry : entries) {
    auto peer = entry.endpoint;
    if (!entry.userHash.empty()) {
      peer.userHash = entry.userHash;
    }
    if (entry.cryptOptions != 0) {
      peer.cryptOptions = entry.cryptOptions;
    }
    if (isFilteredSourceExchangePeer(peer, remotePeer)) {
      continue;
    }
    if (addEd2kPeer(attrs, peer, ed2k::PEER_SOURCE_EXCHANGE)) {
      ++added;
    }
  }
  return added;
}

ed2k::PeerState* getEd2kPeerState(Ed2kAttribute* attrs,
                                  const ed2k::Endpoint& peer)
{
  if (!attrs || peer.host.empty() || peer.port == 0) {
    return nullptr;
  }
  auto i = std::find_if(attrs->peerStates.begin(), attrs->peerStates.end(),
                        [&](const ed2k::PeerState& state) {
                          return state.endpoint.host == peer.host &&
                                 state.endpoint.port == peer.port;
                        });
  if (i != attrs->peerStates.end()) {
    return &*i;
  }
  ed2k::PeerState state;
  state.endpoint = peer;
  attrs->peerStates.push_back(state);
  return &attrs->peerStates.back();
}

bool markEd2kPeerQueued(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                        uint16_t rank,
                        const std::vector<bool>& partStatus)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->queued = true;
  state->dead = false;
  state->cancelled = false;
  state->noFile = false;
  state->outOfParts = false;
  state->queueRank = rank;
  state->partStatus = partStatus;
  return true;
}

bool markEd2kPeerConnecting(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->connecting = true;
  return true;
}

bool markEd2kPeerDisconnected(Ed2kAttribute* attrs,
                              const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->connecting = false;
  state->accepted = false;
  state->requestedParts.clear();
  return true;
}

bool markEd2kCallbackFailed(Ed2kAttribute* attrs, uint32_t clientId)
{
  if (!attrs || clientId == 0) {
    return false;
  }
  auto i = std::find_if(attrs->peerStates.begin(), attrs->peerStates.end(),
                        [&](const ed2k::PeerState& state) {
                          return state.lowId && state.clientId == clientId;
                        });
  if (i == attrs->peerStates.end()) {
    return false;
  }
  i->callbackRequested = false;
  i->callbackImpossible = true;
  i->connecting = false;
  i->accepted = false;
  i->queued = false;
  i->requestedParts.clear();
  return true;
}

bool updateEd2kPeerPartStatus(Ed2kAttribute* attrs,
                              const ed2k::Endpoint& peer,
                              const std::vector<bool>& partStatus)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->partStatus = partStatus;
  return true;
}

bool updateEd2kPeerRequestedParts(
    Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
    const std::vector<ed2k::PartRange>& ranges)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->requestedParts = ranges;
  return true;
}

bool clearEd2kPeerRequestedParts(Ed2kAttribute* attrs,
                                 const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->requestedParts.clear();
  return true;
}

bool markEd2kPeerAccepted(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->connecting = false;
  state->accepted = true;
  state->queued = false;
  state->dead = false;
  state->outOfParts = false;
  return true;
}

bool markEd2kPeerOutOfParts(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->outOfParts = true;
  state->connecting = false;
  state->accepted = false;
  state->queued = true;
  state->requestedParts.clear();
  return true;
}

bool markEd2kPeerCancelled(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->cancelled = true;
  state->connecting = false;
  state->accepted = false;
  state->queued = false;
  state->requestedParts.clear();
  return true;
}

bool markEd2kPeerFailure(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                         int64_t now, int64_t baseRetrySeconds)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->queued = false;
  state->connecting = false;
  state->dead = true;
  state->accepted = false;
  state->requestedParts.clear();
  ++state->failCount;
  state->lastFailureTime = now;
  const auto multiplier = std::min<uint32_t>(state->failCount, 6);
  state->nextRetryTime = now + baseRetrySeconds * multiplier;
  return true;
}

bool markEd2kPeerDead(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                      int64_t now, int64_t baseRetrySeconds)
{
  if (!markEd2kPeerFailure(attrs, peer, now, baseRetrySeconds)) {
    return false;
  }
  auto state = getEd2kPeerState(attrs, peer);
  state->noFile = true;
  return true;
}

size_t expireEd2kDeadSources(Ed2kAttribute* attrs, int64_t now)
{
  if (!attrs) {
    return 0;
  }
  size_t expired = 0;
  for (auto& state : attrs->peerStates) {
    if (!state.dead || state.nextRetryTime == 0 ||
        state.nextRetryTime > now) {
      continue;
    }
    state.dead = false;
    state.noFile = false;
    state.cancelled = false;
    state.nextRetryTime = 0;
    state.requestedParts.clear();
    ++expired;
  }
  return expired;
}

ed2k::ServerState* getEd2kServerState(Ed2kAttribute* attrs,
                                      const ed2k::Endpoint& server)
{
  if (!attrs || server.host.empty() || server.port == 0) {
    return nullptr;
  }
  auto i = std::find_if(attrs->serverStates.begin(), attrs->serverStates.end(),
                        [&](const ed2k::ServerState& item) {
                          return item.endpoint.host == server.host &&
                                 item.endpoint.port == server.port;
                        });
  if (i != attrs->serverStates.end()) {
    return &*i;
  }
  ed2k::ServerState state;
  state.endpoint = server;
  attrs->serverStates.push_back(state);
  return &attrs->serverStates.back();
}

ed2k::ServerState* updateEd2kServerConnecting(Ed2kAttribute* attrs,
                                              const ed2k::Endpoint& server)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return nullptr;
  }
  state->connecting = true;
  return state;
}

ed2k::ServerState* updateEd2kServerConnected(Ed2kAttribute* attrs,
                                             const ed2k::Endpoint& server)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return nullptr;
  }
  state->connecting = false;
  state->connected = true;
  return state;
}

void updateEd2kServerIdChange(Ed2kAttribute* attrs,
                              const ed2k::Endpoint& server,
                              const ed2k::ServerIdChange& idChange)
{
  auto state = updateEd2kServerConnected(attrs, server);
  if (!state) {
    return;
  }
  state->handshakeCompleted = true;
  state->clientId = idChange.clientId;
  state->highId = idChange.highId;
  state->ipAddress = idChange.ipAddress;
  state->tcpFlags = idChange.tcpFlags;
  if (idChange.tcpObfuscationPort != 0) {
    state->tcpObfuscationPort = idChange.tcpObfuscationPort;
  }
  state->failCount = 0;
  state->lastFailureTime = 0;
  state->nextRetryTime = 0;
}

void updateEd2kServerStatus(Ed2kAttribute* attrs,
                            const ed2k::Endpoint& server,
                            const ed2k::ServerStatus& status)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->users = status.users;
  state->files = status.files;
}

void updateEd2kServerUdpStatus(Ed2kAttribute* attrs,
                               const ed2k::Endpoint& server,
                               const ed2k::ServerStatus& status, int64_t now)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->users = status.users;
  state->files = status.files;
  state->maxUsers = status.maxUsers;
  state->softFiles = status.softFiles;
  state->hardFiles = status.hardFiles;
  state->udpFlags = status.udpFlags;
  state->lowIdUsers = status.lowIdUsers;
  state->udpObfuscationPort = status.udpObfuscationPort;
  state->tcpObfuscationPort = status.tcpObfuscationPort;
  state->udpKey = status.udpKey;
  state->udpStatusChallenge = 0;
  state->lastUdpStatusTime = now;
}

void updateEd2kServerMessage(Ed2kAttribute* attrs,
                             const ed2k::Endpoint& server,
                             const std::string& message)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->lastMessage = message;
}

void updateEd2kServerIdent(Ed2kAttribute* attrs,
                           const ed2k::Endpoint& server,
                           const ed2k::ServerIdent& ident)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  if (!ident.name.empty()) {
    state->name = ident.name;
  }
  if (!ident.description.empty()) {
    state->description = ident.description;
  }
}

void updateEd2kServerSourceRequestTime(Ed2kAttribute* attrs,
                                       const ed2k::Endpoint& server,
                                       int64_t nextTime)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->nextSourceRequestTime = nextTime;
}

void markEd2kServerSourceRequestFinished(Ed2kAttribute* attrs,
                                         const ed2k::Endpoint& server)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->connected = false;
  state->connecting = false;
}

void updateEd2kServerFailure(Ed2kAttribute* attrs,
                             const ed2k::Endpoint& server, int64_t now,
                             int64_t baseRetrySeconds)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->connecting = false;
  state->connected = false;
  state->handshakeCompleted = false;
  ++state->failCount;
  state->lastFailureTime = now;
  const auto multiplier = std::min<uint32_t>(state->failCount, 6);
  state->nextRetryTime = now + baseRetrySeconds * multiplier;
}

size_t addEd2kSearchResults(Ed2kAttribute* attrs,
                            const std::vector<ed2k::SearchResultEntry>& entries,
                            bool moreResults)
{
  if (!attrs) {
    return 0;
  }
  size_t added = 0;
  for (const auto& entry : entries) {
    if (!matchesSearchQuery(entry, attrs->searchQuery)) {
      continue;
    }
    auto i = std::find_if(attrs->searchResults.begin(),
                          attrs->searchResults.end(),
                          [&](const ed2k::SearchResultEntry& item) {
                            return item.hash == entry.hash &&
                                   item.size == entry.size;
                          });
    if (i == attrs->searchResults.end()) {
      attrs->searchResults.push_back(entry);
      ++added;
    }
    else {
      if (i->name.empty()) {
        i->name = entry.name;
      }
      i->sourceCount = std::max(i->sourceCount, entry.sourceCount);
      i->completeSourceCount =
          std::max(i->completeSourceCount, entry.completeSourceCount);
      if (i->fileType.empty()) {
        i->fileType = entry.fileType;
      }
      if (i->extension.empty()) {
        i->extension = entry.extension;
      }
      if (i->mediaArtist.empty()) {
        i->mediaArtist = entry.mediaArtist;
      }
      if (i->mediaAlbum.empty()) {
        i->mediaAlbum = entry.mediaAlbum;
      }
      if (i->mediaTitle.empty()) {
        i->mediaTitle = entry.mediaTitle;
      }
      if (i->mediaLength.empty()) {
        i->mediaLength = entry.mediaLength;
      }
      if (i->mediaBitrate == 0) {
        i->mediaBitrate = entry.mediaBitrate;
      }
      if (i->mediaCodec.empty()) {
        i->mediaCodec = entry.mediaCodec;
      }
      if (i->ed2kLink.empty()) {
        i->ed2kLink = entry.ed2kLink;
      }
      if (i->sourceNetwork.empty()) {
        i->sourceNetwork = entry.sourceNetwork;
      }
      else if (!entry.sourceNetwork.empty() &&
               i->sourceNetwork.find(entry.sourceNetwork) ==
                   std::string::npos) {
        i->sourceNetwork += "|" + entry.sourceNetwork;
      }
    }
  }
  attrs->searchMoreResults = moreResults;
  return added;
}

void schedulePendingEd2kServers(RequestGroup* requestGroup, DownloadEngine* e)
{
  std::vector<std::unique_ptr<Command>> commands;
  schedulePendingEd2kServers(commands, requestGroup, e);
  e->addCommand(std::move(commands));
}

void schedulePendingEd2kServers(std::vector<std::unique_ptr<Command>>& commands,
                                RequestGroup* requestGroup,
                                DownloadEngine* e)
{
  auto attrs = getEd2kAttrs(requestGroup->getDownloadContext());
  if (attrs->servers.empty()) {
    return;
  }
  if (attrs->nextServerIndex >= attrs->servers.size()) {
    attrs->nextServerIndex = 0;
  }
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       global::wallclock().getTime().time_since_epoch())
                       .count();
  const auto limit = attrs->servers.size();
  size_t scanned = 0;
  while (scanned < limit) {
    auto server = attrs->servers[attrs->nextServerIndex];
    attrs->nextServerIndex = (attrs->nextServerIndex + 1) %
                             attrs->servers.size();
    ++scanned;
    auto state = getEd2kServerState(attrs, server);
    if (!state) {
      continue;
    }
    if (state->connecting) {
      continue;
    }
    if (state->connected) {
      continue;
    }
    if (state->handshakeCompleted &&
        (state->nextSourceRequestTime == 0 ||
         state->nextSourceRequestTime > now)) {
      continue;
    }
    if (state->nextRetryTime > 0 && state->nextRetryTime > now) {
      continue;
    }
    updateEd2kServerConnecting(attrs, server);
    commands.push_back(make_unique<Ed2kCommand>(e->newCUID(), requestGroup, e,
                                                server, true, false));
  }
}

void schedulePendingEd2kPeers(RequestGroup* requestGroup, DownloadEngine* e)
{
  auto attrs = getEd2kAttrs(requestGroup->getDownloadContext());
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       global::wallclock().getTime().time_since_epoch())
                       .count();
  expireEd2kDeadSources(attrs, now);
  while (requestGroup->getNumStreamCommand() <
         requestGroup->getNumConcurrentCommand()) {
    auto state = ed2k::selectConnectPeer(attrs->peerStates, now);
    if (!state) {
      return;
    }
    auto peer = state->endpoint;
    state->dead = false;
    state->connecting = true;
    e->addCommand(make_unique<Ed2kCommand>(e->newCUID(), requestGroup, e,
                                           peer, false));
  }
}

ed2k::KadRoutingSnapshot createEd2kKadSnapshot(const Ed2kAttribute* attrs)
{
  ed2k::KadRoutingSnapshot snapshot = attrs->kadRoutingTable->snapshot();
  snapshot.lastFirewalledCheck = attrs->lastKadFirewalledCheck;
  snapshot.lastSourcePublish = attrs->lastKadSourcePublish;
  snapshot.observedAddresses = attrs->kadObservedAddresses;
  snapshot.firewalled = attrs->kadFirewalled;
  return snapshot;
}

void restoreEd2kKadOperationalState(Ed2kAttribute* attrs,
                                    const ed2k::KadRoutingSnapshot& snapshot)
{
  attrs->lastKadFirewalledCheck = snapshot.lastFirewalledCheck;
  attrs->lastKadSourcePublish = snapshot.lastSourcePublish;
  attrs->kadObservedAddresses = snapshot.observedAddresses;
  attrs->kadFirewalled = snapshot.firewalled;
}

} // namespace aria2
