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
#include <cstdint>
#include <limits>

#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "DlAbortEx.h"
#include "Ed2kCommand.h"
#include "Option.h"
#include "RequestGroup.h"
#include "SegmentMan.h"
#include "SimpleRandomizer.h"
#include "a2functional.h"
#include "Command.h"
#include "ed2k_hash.h"
#include "ed2k_endpoint.h"
#include "ed2k_kad.h"
#include "ed2k_policy.h"
#include "prefs.h"
#include "util.h"
#include "wallclock.h"

namespace aria2 {

namespace {
class Ed2kPeerScheduleCommand : public Command {
public:
  Ed2kPeerScheduleCommand(cuid_t cuid, RequestGroup* requestGroup,
                          DownloadEngine* e)
      : Command(cuid), requestGroup_(requestGroup), e_(e)
  {
    setStatus(Command::STATUS_ONESHOT_REALTIME);
  }

  bool execute() CXX11_OVERRIDE
  {
    if (!requestGroup_->downloadFinished() && !requestGroup_->isHaltRequested()) {
      schedulePendingEd2kPeers(requestGroup_, e_);
    }
    return true;
  }

private:
  RequestGroup* requestGroup_;
  DownloadEngine* e_;
};
} // namespace

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

uint32_t createEd2kKadUdpVerifyKey()
{
  uint32_t key = 0;
  SimpleRandomizer::getInstance()->getRandomBytes(
      reinterpret_cast<unsigned char*>(&key), sizeof(key));
  return key == 0 ? 1 : key;
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

ed2k::PeerState* findEd2kPeerStateByEndpointOrUdp(Ed2kAttribute* attrs,
                                                  const ed2k::Endpoint& peer)
{
  if (!attrs || peer.host.empty() || peer.port == 0) {
    return nullptr;
  }
  auto i = std::find_if(attrs->peerStates.begin(), attrs->peerStates.end(),
                        [&](const ed2k::PeerState& state) {
                          return state.endpoint.host == peer.host &&
                                 (state.endpoint.port == peer.port ||
                                  state.udpPort == peer.port);
                        });
  return i == attrs->peerStates.end() ? nullptr : &*i;
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

bool addEd2kKadSourcePeer(Ed2kAttribute* attrs,
                          const ed2k::KadSourceEndpoint& source,
                          uint32_t sourceFlag)
{
  if (!attrs || source.endpoint.host.empty() || source.endpoint.port == 0) {
    return false;
  }
  if (source.sourceType != 0 && source.sourceType != 1 &&
      source.sourceType != 4 && source.sourceType != 3 &&
      source.sourceType != 5 && source.sourceType != 6) {
    return false;
  }
  const bool buddyCallback =
      source.sourceType == 3 || source.sourceType == 5;
  const bool directCallback = source.sourceType == 6;
  const bool callbackOnly = buddyCallback || directCallback;
  const uint32_t lowIdClientId = callbackOnly
                                     ? ed2k::ipv4ToEndpointValue(
                                           source.endpoint.host)
                                     : 0;
  const auto added =
      callbackOnly ? false : addEd2kPeer(attrs, source.endpoint, sourceFlag);
  auto state = getEd2kPeerState(attrs, source.endpoint);
  if (state) {
    state->sourceFlags |= sourceFlag;
    if (source.udpPort != 0) {
      state->udpPort = source.udpPort;
      if (state->udpVersion == 0) {
        state->udpVersion = 4;
      }
    }
    if (callbackOnly) {
      state->clientId = lowIdClientId;
      state->lowId = true;
      state->callbackRequested = true;
      state->callbackImpossible = false;
      state->lowIdCallbackState = ed2k::LowIdCallbackState::REQUESTED;
      state->callbackDeadline = 0;
      state->callbackKind = directCallback ? ed2k::CallbackKind::DIRECT
                                           : ed2k::CallbackKind::BUDDY;
      if (buddyCallback) {
        state->callbackBuddy.host = ed2k::ipv4FromEndpoint(source.buddyIp);
        state->callbackBuddy.port = source.buddyPort;
        state->callbackBuddyId =
            source.buddyId.empty() ? ed2k::ed2kHashToKadId(source.buddyHash)
                                   : source.buddyId;
      }
      else {
        state->callbackBuddy = ed2k::Endpoint();
        state->callbackBuddyId.clear();
      }
    }
  }
  return added;
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
    state->lowIdCallbackState = ed2k::LowIdCallbackState::REQUESTED;
    state->callbackKind = ed2k::CallbackKind::BUDDY;
    state->lastCallbackTime = 0;
    state->callbackDeadline = 0;
  }
  else if (!state->callbackRequested) {
    state->callbackImpossible = true;
    state->lowIdCallbackState = ed2k::LowIdCallbackState::IMPOSSIBLE;
  }
  state->connecting = false;
  state->accepted = false;
  state->queued = false;
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
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

bool markEd2kPeerUdpReaskSent(Ed2kAttribute* attrs,
                              const ed2k::Endpoint& peer, int64_t now)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->udpReaskPending = true;
  state->lastUdpReaskTime = now;
  state->nextUdpReaskTime = now + ed2k::PEER_UDP_REASK_INTERVAL;
  return true;
}

bool markEd2kPeerUdpReaskAck(Ed2kAttribute* attrs,
                             const ed2k::Endpoint& peer, uint16_t rank,
                             const std::vector<bool>& partStatus,
                             int64_t now)
{
  auto state = findEd2kPeerStateByEndpointOrUdp(attrs, peer);
  if (!state) {
    return false;
  }
  state->queued = true;
  state->dead = false;
  state->noFile = false;
  state->cancelled = false;
  state->remoteQueueFull = false;
  state->udpReaskPending = false;
  state->queueRank = rank;
  if (!partStatus.empty()) {
    state->partStatus = partStatus;
  }
  state->lastUdpReaskTime = now;
  state->nextUdpReaskTime = now + ed2k::PEER_UDP_REASK_INTERVAL;
  return true;
}

bool markEd2kPeerQueueFull(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                           int64_t now, int64_t baseRetrySeconds)
{
  auto state = findEd2kPeerStateByEndpointOrUdp(attrs, peer);
  if (!state) {
    return false;
  }
  const auto endpoint = state->endpoint;
  if (!markEd2kPeerFailure(attrs, endpoint, now, baseRetrySeconds)) {
    return false;
  }
  state = getEd2kPeerState(attrs, endpoint);
  state->queued = true;
  state->dead = true;
  state->noFile = false;
  state->remoteQueueFull = true;
  state->udpReaskPending = false;
  state->queueRank = 0;
  state->nextUdpReaskTime = state->nextRetryTime;
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
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
  state->requestedParts.clear();
  return true;
}

bool markEd2kCallbackRequestSent(Ed2kAttribute* attrs, uint32_t clientId,
                                 int64_t now, int64_t timeoutSeconds)
{
  if (!attrs || clientId == 0 || timeoutSeconds <= 0) {
    return false;
  }
  auto i = std::find_if(attrs->peerStates.begin(), attrs->peerStates.end(),
                        [&](const ed2k::PeerState& state) {
                          return state.lowId && state.clientId == clientId;
                        });
  if (i == attrs->peerStates.end()) {
    return false;
  }
  i->callbackRequested = true;
  i->callbackImpossible = false;
  i->lowIdCallbackState = ed2k::LowIdCallbackState::REQUESTED;
  i->lastCallbackTime = now;
  i->callbackDeadline = now + timeoutSeconds;
  i->connecting = false;
  i->accepted = false;
  i->queued = false;
  releaseEd2kRequestedRanges(attrs, i->requestedParts);
  i->requestedParts.clear();
  return true;
}

bool markEd2kCallbackAccepted(Ed2kAttribute* attrs, uint32_t clientId,
                              const ed2k::Endpoint& peer, int64_t now)
{
  if (!attrs || clientId == 0 || peer.host.empty() || peer.port == 0) {
    return false;
  }
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->clientId = clientId;
  state->lowId = false;
  state->callbackRequested = false;
  state->callbackImpossible = false;
  state->callbackKind = ed2k::CallbackKind::NONE;
  state->lowIdCallbackState = ed2k::LowIdCallbackState::ACCEPTED;
  state->lastCallbackTime = now;
  state->callbackDeadline = 0;
  state->connecting = false;
  state->accepted = false;
  state->queued = false;
  state->dead = false;
  state->cancelled = false;
  state->noFile = false;
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
  state->requestedParts.clear();
  for (auto& existing : attrs->peerStates) {
    if (&existing == state || existing.clientId != clientId ||
        !existing.lowId) {
      continue;
    }
    existing.callbackRequested = false;
    existing.callbackImpossible = false;
    existing.callbackKind = ed2k::CallbackKind::NONE;
    existing.lowIdCallbackState = ed2k::LowIdCallbackState::ACCEPTED;
    existing.lastCallbackTime = now;
    existing.callbackDeadline = 0;
    existing.connecting = false;
    existing.accepted = false;
    existing.queued = false;
    releaseEd2kRequestedRanges(attrs, existing.requestedParts);
    existing.requestedParts.clear();
  }
  return true;
}

bool markEd2kDirectCallbackAccepted(Ed2kAttribute* attrs,
                                    const ed2k::Endpoint& peer,
                                    int64_t now)
{
  if (!attrs || peer.host.empty() || peer.port == 0 ||
      peer.userHash.size() != ed2k::HASH_LENGTH) {
    return false;
  }
  auto i = std::find_if(attrs->peerStates.begin(), attrs->peerStates.end(),
                        [&](const ed2k::PeerState& state) {
                          return state.callbackKind ==
                                     ed2k::CallbackKind::DIRECT &&
                                 state.endpoint.host == peer.host &&
                                 state.endpoint.port == peer.port &&
                                 state.endpoint.userHash == peer.userHash;
                        });
  if (i == attrs->peerStates.end()) {
    return false;
  }
  i->lowId = false;
  i->callbackRequested = false;
  i->callbackImpossible = false;
  i->callbackKind = ed2k::CallbackKind::NONE;
  i->lowIdCallbackState = ed2k::LowIdCallbackState::ACCEPTED;
  i->lastCallbackTime = now;
  i->callbackDeadline = 0;
  i->connecting = false;
  i->accepted = false;
  i->queued = false;
  i->dead = false;
  i->cancelled = false;
  i->noFile = false;
  if (peer.cryptOptions != 0 && i->endpoint.cryptOptions == 0) {
    i->endpoint.cryptOptions = peer.cryptOptions;
  }
  releaseEd2kRequestedRanges(attrs, i->requestedParts);
  i->requestedParts.clear();
  return true;
}

bool markEd2kCallbackCompleted(Ed2kAttribute* attrs,
                               const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->lowIdCallbackState = ed2k::LowIdCallbackState::COMPLETED;
  state->callbackRequested = false;
  state->callbackImpossible = false;
  state->callbackKind = ed2k::CallbackKind::NONE;
  state->callbackDeadline = 0;
  return true;
}

bool markEd2kCallbackFailed(Ed2kAttribute* attrs, uint32_t clientId)
{
  return markEd2kCallbackFailed(attrs, clientId, 0, 0);
}

bool markEd2kCallbackFailed(Ed2kAttribute* attrs, uint32_t clientId,
                            int64_t now, int64_t baseRetrySeconds)
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
  i->callbackKind = ed2k::CallbackKind::NONE;
  i->lowIdCallbackState = ed2k::LowIdCallbackState::FAILED;
  i->lastCallbackTime = now;
  i->callbackDeadline = 0;
  i->connecting = false;
  i->accepted = false;
  i->queued = false;
  releaseEd2kRequestedRanges(attrs, i->requestedParts);
  i->requestedParts.clear();
  if (baseRetrySeconds > 0) {
    i->dead = true;
    ++i->failCount;
    i->lastFailureTime = now;
    const auto multiplier = std::min<uint32_t>(i->failCount, 6);
    i->nextRetryTime = now + baseRetrySeconds * multiplier;
  }
  return true;
}

size_t expireEd2kCallbackWaits(Ed2kAttribute* attrs, int64_t now)
{
  if (!attrs) {
    return 0;
  }
  size_t expired = 0;
  for (auto& state : attrs->peerStates) {
    const bool retryExpired =
        state.dead && state.nextRetryTime != 0 && state.nextRetryTime <= now &&
        (state.lowIdCallbackState == ed2k::LowIdCallbackState::FAILED ||
         state.lowIdCallbackState == ed2k::LowIdCallbackState::TIMED_OUT);
    const bool deadlineExpired =
        state.lowIdCallbackState == ed2k::LowIdCallbackState::REQUESTED &&
        state.callbackDeadline != 0 && state.callbackDeadline <= now;
    if (!retryExpired && !deadlineExpired) {
      continue;
    }
    state.callbackRequested = false;
    state.callbackImpossible = true;
    state.callbackKind = ed2k::CallbackKind::NONE;
    state.lowIdCallbackState = deadlineExpired
                                   ? ed2k::LowIdCallbackState::TIMED_OUT
                                   : ed2k::LowIdCallbackState::IMPOSSIBLE;
    state.dead = false;
    state.connecting = false;
    state.accepted = false;
    state.queued = false;
    releaseEd2kRequestedRanges(attrs, state.requestedParts);
    state.requestedParts.clear();
    state.nextRetryTime = 0;
    ++expired;
  }
  return expired;
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
  return updateEd2kPeerRequestedParts(attrs, peer, ranges, 0);
}

bool updateEd2kPeerRequestedParts(
    Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
    const std::vector<ed2k::PartRange>& ranges, int64_t now)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
  state->requestedParts = ranges;
  markEd2kRequestedRanges(attrs, ranges);
  if (!ranges.empty() && now != 0) {
    state->lastPartRequestTime = now;
    state->lastTransferProgressTime = now;
    state->cancelTransferSent = false;
  }
  return true;
}

bool markEd2kRequestedRanges(Ed2kAttribute* attrs,
                             const std::vector<ed2k::PartRange>& ranges)
{
  if (!attrs) {
    return false;
  }
  attrs->requestedPartRanges.insert(attrs->requestedPartRanges.end(),
                                    ranges.begin(), ranges.end());
  return true;
}

void releaseEd2kRequestedRanges(Ed2kAttribute* attrs,
                                const std::vector<ed2k::PartRange>& ranges)
{
  if (!attrs || ranges.empty()) {
    return;
  }
  for (const auto& range : ranges) {
    auto i = std::find_if(attrs->requestedPartRanges.begin(),
                          attrs->requestedPartRanges.end(),
                          [&](const ed2k::PartRange& item) {
                            return item.begin == range.begin &&
                                   item.end == range.end;
                          });
    if (i != attrs->requestedPartRanges.end()) {
      attrs->requestedPartRanges.erase(i);
    }
  }
}

size_t removeEd2kPeerCompletedRequestedRange(Ed2kAttribute* attrs,
                                             const ed2k::Endpoint& peer,
                                             int64_t begin, int64_t end,
                                             int64_t now)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state || end <= begin) {
    return 0;
  }
  size_t updated = 0;
  std::vector<ed2k::PartRange> removedRanges;
  std::vector<ed2k::PartRange> addedRanges;
  for (auto& range : state->requestedParts) {
    if (end <= range.begin || begin >= range.end) {
      continue;
    }
    removedRanges.push_back(range);
    if (begin <= range.begin && end < range.end) {
      range.begin = end;
      addedRanges.push_back(range);
    }
    else if (begin > range.begin && end >= range.end) {
      range.end = begin;
      addedRanges.push_back(range);
    }
    else if (begin > range.begin && end < range.end) {
      ed2k::PartRange tail;
      tail.begin = end;
      tail.end = range.end;
      range.end = begin;
      addedRanges.push_back(range);
      state->requestedParts.push_back(tail);
      addedRanges.push_back(tail);
    }
    else {
      range.begin = range.end = 0;
    }
    ++updated;
  }
  state->requestedParts.erase(
      std::remove_if(state->requestedParts.begin(), state->requestedParts.end(),
                     [](const ed2k::PartRange& range) {
                       return range.end <= range.begin;
                     }),
      state->requestedParts.end());
  releaseEd2kRequestedRanges(attrs, removedRanges);
  markEd2kRequestedRanges(attrs, addedRanges);
  if (updated != 0) {
    state->lastTransferProgressTime = now;
    state->cancelTransferSent = false;
  }
  return updated;
}

bool clearEd2kPeerRequestedParts(Ed2kAttribute* attrs,
                                 const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
  state->requestedParts.clear();
  return true;
}

bool reclaimEd2kStalledRequestedRange(
    Ed2kAttribute* attrs, const ed2k::Endpoint& requester,
    const std::vector<bool>& requesterPartStatus, int64_t now,
    int64_t staleSeconds, ed2k::PartRange& reclaimed)
{
  if (!attrs || requester.host.empty() || requester.port == 0 ||
      staleSeconds <= 0) {
    return false;
  }
  const auto requesterState = getEd2kPeerState(attrs, requester);
  for (auto& state : attrs->peerStates) {
    if (sameEndpoint(state.endpoint, requester) ||
        state.requestedParts.empty() || state.cancelTransferSent) {
      continue;
    }
    const auto lastProgress =
        state.lastTransferProgressTime != 0 ? state.lastTransferProgressTime
                                            : state.lastPartRequestTime;
    if (lastProgress == 0 || now - lastProgress < staleSeconds) {
      continue;
    }
    for (auto range = state.requestedParts.begin();
         range != state.requestedParts.end(); ++range) {
      const auto pieceIndex =
          static_cast<size_t>(range->begin / ed2k::PIECE_LENGTH);
      if (!requesterPartStatus.empty() &&
          (pieceIndex >= requesterPartStatus.size() ||
           !requesterPartStatus[pieceIndex])) {
        continue;
      }
      if (requesterState &&
          std::any_of(requesterState->requestedParts.begin(),
                      requesterState->requestedParts.end(),
                      [&](const ed2k::PartRange& existing) {
                        return existing.begin < range->end &&
                               range->begin < existing.end;
                      })) {
        continue;
      }
      reclaimed = *range;
      releaseEd2kRequestedRanges(attrs, std::vector<ed2k::PartRange>{*range});
      state.requestedParts.erase(range);
      state.cancelTransferSent = true;
      return true;
    }
  }
  return false;
}

bool canReclaimEd2kStalledRequestedRange(
    Ed2kAttribute* attrs, const ed2k::Endpoint& requester,
    const std::vector<bool>& requesterPartStatus, int64_t now,
    int64_t staleSeconds)
{
  if (!attrs || requester.host.empty() || requester.port == 0 ||
      staleSeconds <= 0) {
    return false;
  }
  const auto requesterState = getEd2kPeerState(attrs, requester);
  for (const auto& state : attrs->peerStates) {
    if (sameEndpoint(state.endpoint, requester) ||
        state.requestedParts.empty() || state.cancelTransferSent) {
      continue;
    }
    const auto lastProgress =
        state.lastTransferProgressTime != 0 ? state.lastTransferProgressTime
                                            : state.lastPartRequestTime;
    if (lastProgress == 0 || now - lastProgress < staleSeconds) {
      continue;
    }
    for (const auto& range : state.requestedParts) {
      const auto pieceIndex =
          static_cast<size_t>(range.begin / ed2k::PIECE_LENGTH);
      if (!requesterPartStatus.empty() &&
          (pieceIndex >= requesterPartStatus.size() ||
           !requesterPartStatus[pieceIndex])) {
        continue;
      }
      if (requesterState &&
          std::any_of(requesterState->requestedParts.begin(),
                      requesterState->requestedParts.end(),
                      [&](const ed2k::PartRange& existing) {
                        return existing.begin < range.end &&
                               range.begin < existing.end;
                      })) {
        continue;
      }
      return true;
    }
  }
  return false;
}

bool activelyReclaimEd2kStalledRequestedRange(
    Ed2kAttribute* attrs, const ed2k::Endpoint& requester,
    const std::vector<bool>& requesterPartStatus, int64_t now,
    ed2k::PartRange& reclaimed)
{
  return reclaimEd2kStalledRequestedRange(
      attrs, requester, requesterPartStatus, now,
      ed2k::ACTIVE_ENDGAME_RECLAIM_STALL_SECONDS, reclaimed);
}

bool expireEd2kStalledPeerTransfer(Ed2kAttribute* attrs,
                                   SegmentMan* segmentMan,
                                   const ed2k::Endpoint& peer,
                                   int64_t cuid, int64_t now,
                                   int64_t timeoutSeconds,
                                   int64_t baseRetrySeconds)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state || !segmentMan || timeoutSeconds <= 0 ||
      state->requestedParts.empty()) {
    return false;
  }
  const auto lastProgress =
      state->lastTransferProgressTime != 0 ? state->lastTransferProgressTime
                                           : state->lastPartRequestTime;
  if (lastProgress == 0 || now - lastProgress < timeoutSeconds) {
    return false;
  }
  segmentMan->cancelSegment(cuid);
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
  state->requestedParts.clear();
  state->cancelTransferSent = true;
  if (!markEd2kPeerFailure(attrs, peer, now, baseRetrySeconds)) {
    return false;
  }
  state = getEd2kPeerState(attrs, peer);
  state->queued = true;
  state->cancelTransferSent = true;
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
  if (state->lowIdCallbackState == ed2k::LowIdCallbackState::ACCEPTED) {
    state->lowIdCallbackState = ed2k::LowIdCallbackState::COMPLETED;
  }
  state->udpReaskPending = false;
  state->remoteQueueFull = false;
  state->outOfParts = false;
  state->cancelTransferSent = false;
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
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
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
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
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
  state->udpReaskPending = false;
  releaseEd2kRequestedRanges(attrs, state->requestedParts);
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
    state.remoteQueueFull = false;
    state.udpReaskPending = false;
    state.nextRetryTime = 0;
    releaseEd2kRequestedRanges(attrs, state.requestedParts);
    state.requestedParts.clear();
    ++expired;
  }
  return expired;
}

ed2k::PeerState* selectDueEd2kUdpReaskPeer(Ed2kAttribute* attrs, int64_t now)
{
  if (!attrs) {
    return nullptr;
  }
  ed2k::PeerState* selected = nullptr;
  for (auto& state : attrs->peerStates) {
    if (!state.queued || state.dead || state.noFile || state.cancelled ||
        state.outOfParts || state.accepted || state.connecting ||
        state.udpReaskPending || state.udpPort == 0 ||
        state.udpVersion == 0) {
      continue;
    }
    if (state.nextUdpReaskTime != 0 && state.nextUdpReaskTime > now) {
      continue;
    }
    if (!selected ||
        ed2k::sourcePriority(state.sourceFlags) >
            ed2k::sourcePriority(selected->sourceFlags) ||
        (ed2k::sourcePriority(state.sourceFlags) ==
             ed2k::sourcePriority(selected->sourceFlags) &&
         state.queueRank != 0 &&
         (selected->queueRank == 0 || state.queueRank < selected->queueRank))) {
      selected = &state;
    }
  }
  return selected;
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

void markEd2kServerTcpSourceRequestSent(Ed2kAttribute* attrs,
                                        const ed2k::Endpoint& server,
                                        int64_t now)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->nextSourceRequestTime = now + ed2k::SERVER_TCP_SOURCE_REASK_INTERVAL;
}

void markEd2kServerUdpSourceRequestSent(Ed2kAttribute* attrs,
                                        const ed2k::Endpoint& server,
                                        int64_t now)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->lastUdpSourceRequestTime = now;
}

void updateEd2kServerSourceResponse(Ed2kAttribute* attrs,
                                    const ed2k::Endpoint& server,
                                    size_t sourceCount, int64_t now)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->lastSourceResponseTime = now;
  state->lastSourceCount = static_cast<uint32_t>(
      std::min<size_t>(sourceCount, std::numeric_limits<uint32_t>::max()));
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
    if (!attrs->searchActive &&
        !ed2k::serverTcpSourceRequestDue(*state, attrs->link.size, now)) {
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
  expireEd2kCallbackWaits(attrs, now);
  while (requestGroup->getNumStreamCommand() <
         requestGroup->getNumConcurrentCommand()) {
    auto action = ed2k::selectPeerAction(attrs->peerStates, now);
    if (action.type != ed2k::PeerActionType::CONNECT &&
        action.type != ed2k::PeerActionType::RETRY) {
      return;
    }
    auto state = action.peer;
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

void scheduleEd2kPeerCheck(RequestGroup* requestGroup, DownloadEngine* e)
{
  e->addCommand(
      make_unique<Ed2kPeerScheduleCommand>(e->newCUID(), requestGroup, e));
  e->setNoWait(true);
}

ed2k::KadRoutingSnapshot createEd2kKadSnapshot(const Ed2kAttribute* attrs)
{
  ed2k::KadRoutingSnapshot snapshot = attrs->kadRoutingTable->snapshot();
  snapshot.lastFirewalledCheck = attrs->lastKadFirewalledCheck;
  snapshot.lastSourcePublish = attrs->lastKadSourcePublish;
  snapshot.lastSourceSearch = attrs->lastKadSourceSearch;
  snapshot.sourceSearchCount = attrs->kadSourceSearchCount;
  snapshot.udpVerifyKey = attrs->kadUdpVerifyKey;
  snapshot.observedAddresses = attrs->kadObservedAddresses;
  snapshot.firewalled = attrs->kadFirewalled;
  return snapshot;
}

void restoreEd2kKadOperationalState(Ed2kAttribute* attrs,
                                    const ed2k::KadRoutingSnapshot& snapshot)
{
  attrs->lastKadFirewalledCheck = snapshot.lastFirewalledCheck;
  attrs->lastKadSourcePublish = snapshot.lastSourcePublish;
  attrs->lastKadSourceSearch = snapshot.lastSourceSearch;
  attrs->kadSourceSearchCount = snapshot.sourceSearchCount;
  attrs->kadUdpVerifyKey = snapshot.udpVerifyKey;
  attrs->kadObservedAddresses = snapshot.observedAddresses;
  attrs->kadFirewalled = snapshot.firewalled;
}

bool shouldStartEd2kKadSourceSearch(const Ed2kAttribute* attrs, int64_t now)
{
  if (!attrs || attrs->link.hash.empty() || !attrs->kadRoutingTable ||
      attrs->kadRoutingTable->usefulSize() == 0) {
    return false;
  }
  constexpr size_t MAX_SOURCES_PER_FILE_UDP = 50;
  if (attrs->peerStates.size() >= MAX_SOURCES_PER_FILE_UDP) {
    return false;
  }
  if (attrs->kadSourceTraversal && !attrs->kadSourceTraversal->done()) {
    return false;
  }
  const auto count = std::max<uint32_t>(
      1, std::min<uint32_t>(attrs->kadSourceSearchCount, 7));
  const auto delay = static_cast<int64_t>(3600) * count;
  return attrs->lastKadSourceSearch == 0 ||
         now >= attrs->lastKadSourceSearch + delay;
}

void markEd2kKadSourceSearchStarted(Ed2kAttribute* attrs, int64_t now)
{
  if (!attrs) {
    return;
  }
  if (attrs->kadSourceSearchCount < 7) {
    ++attrs->kadSourceSearchCount;
  }
  attrs->lastKadSourceSearch = now;
}

} // namespace aria2
