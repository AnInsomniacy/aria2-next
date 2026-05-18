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
#include "Ed2kCommand.h"
#include "RequestGroup.h"
#include "a2functional.h"
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

bool addEd2kPeer(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  if (!attrs || !addUniqueEndpoint(attrs->peers, peer)) {
    return false;
  }
  getEd2kPeerState(attrs, peer);
  return true;
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
  state->queueRank = rank;
  state->partStatus = partStatus;
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

bool markEd2kPeerAccepted(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->accepted = true;
  state->queued = false;
  state->dead = false;
  return true;
}

bool markEd2kPeerOutOfParts(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->outOfParts = true;
  state->accepted = false;
  state->queued = true;
  return true;
}

bool markEd2kPeerCancelled(Ed2kAttribute* attrs, const ed2k::Endpoint& peer)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->cancelled = true;
  state->accepted = false;
  state->queued = false;
  return true;
}

bool markEd2kPeerDead(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                      int64_t now, int64_t baseRetrySeconds)
{
  auto state = getEd2kPeerState(attrs, peer);
  if (!state) {
    return false;
  }
  state->queued = false;
  state->dead = true;
  state->accepted = false;
  state->noFile = true;
  ++state->failCount;
  state->lastFailureTime = now;
  const auto multiplier = std::min<uint32_t>(state->failCount, 6);
  state->nextRetryTime = now + baseRetrySeconds * multiplier;
  return true;
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
  state->udpStatusChallenge = status.challenge;
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

void updateEd2kServerSourceRequestTime(Ed2kAttribute* attrs,
                                       const ed2k::Endpoint& server,
                                       int64_t nextTime)
{
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  state->connected = false;
  state->connecting = false;
  state->nextSourceRequestTime = nextTime;
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
  while (attrs->nextPeerIndex < attrs->peers.size() &&
         requestGroup->getNumStreamCommand() <
             requestGroup->getNumConcurrentCommand()) {
    auto peer = attrs->peers[attrs->nextPeerIndex++];
    auto state = getEd2kPeerState(attrs, peer);
    if (state && state->dead && state->nextRetryTime > now) {
      continue;
    }
    if (state) {
      state->dead = false;
    }
    e->addCommand(make_unique<Ed2kCommand>(e->newCUID(), requestGroup, e,
                                           peer, false));
  }
}

} // namespace aria2
