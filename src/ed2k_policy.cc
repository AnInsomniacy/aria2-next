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
#include "ed2k_policy.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "Segment.h"
#include "SegmentMan.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
#include "ed2k_link.h"

namespace aria2 {

namespace ed2k {

constexpr int64_t SERVER_FRESH_SOURCE_RESPONSE_INTERVAL = 300;

int sourcePriority(uint32_t sourceFlags)
{
  int priority = 0;
  if ((sourceFlags & PEER_SOURCE_SERVER) != 0) {
    priority |= 1 << 5;
  }
  if ((sourceFlags & PEER_SOURCE_KAD) != 0) {
    priority |= 1 << 4;
  }
  if ((sourceFlags & PEER_SOURCE_INCOMING) != 0) {
    priority |= 1 << 3;
  }
  if ((sourceFlags & PEER_SOURCE_RESUME) != 0) {
    priority |= 1 << 2;
  }
  if ((sourceFlags & PEER_SOURCE_EXCHANGE) != 0) {
    priority |= 1 << 1;
  }
  if ((sourceFlags & PEER_SOURCE_INLINE) != 0) {
    priority |= 1 << 0;
  }
  return priority;
}

namespace {
bool retryReady(const PeerState& peer, int64_t now)
{
  return !peer.dead || peer.nextRetryTime <= now;
}

bool canConnect(const PeerState& peer, int64_t now)
{
  if ((peer.endpoint.cryptOptions & SOURCE_CRYPT_REQUIRE) != 0) {
    return false;
  }
  if (peer.lowId) {
    return false;
  }
  if (peer.lowIdCallbackState == LowIdCallbackState::IMPOSSIBLE ||
      peer.lowIdCallbackState == LowIdCallbackState::FAILED ||
      peer.lowIdCallbackState == LowIdCallbackState::TIMED_OUT) {
    return false;
  }
  if (peer.connecting || peer.accepted || peer.queued || peer.noFile ||
      peer.cancelled || peer.outOfParts) {
    return false;
  }
  if (!retryReady(peer, now)) {
    return false;
  }
  return true;
}

bool countsAgainstActiveCap(const PeerState& peer, int64_t now)
{
  const auto lifecycle = classifyPeerLifecycle(peer, now);
  return lifecycle == PeerLifecycle::CONNECTING ||
         lifecycle == PeerLifecycle::DOWNLOADING;
}

bool betterConnectCandidate(const PeerState& peer, const PeerState& selected)
{
  if (peer.failCount != selected.failCount) {
    return peer.failCount < selected.failCount;
  }
  const auto priority = sourcePriority(peer.sourceFlags);
  const auto selectedPriority = sourcePriority(selected.sourceFlags);
  if (priority != selectedPriority) {
    return priority > selectedPriority;
  }
  if (selected.queued != peer.queued) {
    return peer.queued;
  }
  return peer.queueRank != 0 &&
         (selected.queueRank == 0 || peer.queueRank < selected.queueRank);
}

bool retryDue(const ServerState& server, int64_t now)
{
  return server.nextRetryTime == 0 || server.nextRetryTime <= now;
}

bool supportsTcpFileSize(const ServerState& server, int64_t fileSize)
{
  return fileSize <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) ||
         (server.tcpFlags & SRV_TCPFLG_LARGEFILES) != 0;
}

bool supportsUdpFileSize(const ServerState& server, int64_t fileSize)
{
  if (fileSize <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
    return true;
  }
  return (server.udpFlags & SRV_UDPFLG_EXT_GETSOURCES2) != 0 &&
         (server.udpFlags & SRV_UDPFLG_LARGEFILES) != 0;
}

bool sourceResponseFresh(const ServerState& server, int64_t now)
{
  return server.lastSourceCount != 0 && server.lastSourceResponseTime != 0 &&
         now - server.lastSourceResponseTime <
             SERVER_FRESH_SOURCE_RESPONSE_INTERVAL;
}
} // namespace

PeerState* selectConnectPeer(std::vector<PeerState>& peers, int64_t now)
{
  return selectConnectPeer(peers, now, 0);
}

PeerAction selectPeerAction(std::vector<PeerState>& peers, int64_t now)
{
  return selectPeerAction(peers, now, 0);
}

PeerLifecycle classifyPeerLifecycle(const PeerState& peer, int64_t now)
{
  if (peer.cancelled) {
    return PeerLifecycle::CANCELLED;
  }
  if (peer.noFile) {
    return PeerLifecycle::NO_FILE;
  }
  if (peer.lowIdCallbackState == LowIdCallbackState::REQUESTED) {
    return PeerLifecycle::CALLBACK_WAITING;
  }
  if (peer.outOfParts) {
    return PeerLifecycle::NO_NEEDED_PARTS;
  }
  if (peer.dead) {
    return peer.nextRetryTime <= now ? PeerLifecycle::RETRYING
                                     : PeerLifecycle::DEAD;
  }
  if (peer.connecting) {
    return PeerLifecycle::CONNECTING;
  }
  if (peer.accepted) {
    return PeerLifecycle::DOWNLOADING;
  }
  if (peer.queued) {
    return PeerLifecycle::QUEUED;
  }
  return PeerLifecycle::USEFUL;
}

PeerAction selectPeerAction(std::vector<PeerState>& peers, int64_t now,
                            size_t activeSourceCap)
{
  PeerState* connectPeer = nullptr;
  PeerState* retryPeer = nullptr;
  PeerState* reaskPeer = nullptr;
  PeerState* callbackPeer = nullptr;
  PeerState* expirePeer = nullptr;
  size_t active = 0;

  for (auto& peer : peers) {
    const auto lifecycle = classifyPeerLifecycle(peer, now);
    if (lifecycle == PeerLifecycle::CONNECTING ||
        lifecycle == PeerLifecycle::DOWNLOADING) {
      ++active;
    }
    switch (lifecycle) {
    case PeerLifecycle::USEFUL:
      if (canConnect(peer, now) &&
          (!connectPeer || betterConnectCandidate(peer, *connectPeer))) {
        connectPeer = &peer;
      }
      break;
    case PeerLifecycle::QUEUED:
      if (!reaskPeer || betterConnectCandidate(peer, *reaskPeer)) {
        reaskPeer = &peer;
      }
      break;
    case PeerLifecycle::CALLBACK_WAITING:
      if (!callbackPeer) {
        callbackPeer = &peer;
      }
      break;
    case PeerLifecycle::RETRYING:
      if (canConnect(peer, now) &&
          (!retryPeer || betterConnectCandidate(peer, *retryPeer))) {
        retryPeer = &peer;
      }
      if (!expirePeer) {
        expirePeer = &peer;
      }
      break;
    case PeerLifecycle::DEAD:
      break;
    case PeerLifecycle::CONNECTING:
    case PeerLifecycle::DOWNLOADING:
    case PeerLifecycle::NO_NEEDED_PARTS:
    case PeerLifecycle::NO_FILE:
    case PeerLifecycle::CANCELLED:
      break;
    }
  }

  const bool canStartActive =
      activeSourceCap == 0 || active < activeSourceCap;
  if (canStartActive && connectPeer) {
    return PeerAction{PeerActionType::CONNECT, connectPeer};
  }
  if (canStartActive && retryPeer) {
    return PeerAction{PeerActionType::RETRY, retryPeer};
  }
  if (reaskPeer) {
    return PeerAction{PeerActionType::REASK, reaskPeer};
  }
  if (callbackPeer) {
    return PeerAction{PeerActionType::CALLBACK, callbackPeer};
  }
  if (expirePeer) {
    return PeerAction{PeerActionType::EXPIRE, expirePeer};
  }
  return PeerAction{};
}

PeerState* selectConnectPeer(std::vector<PeerState>& peers, int64_t now,
                             size_t activeSourceCap)
{
  size_t active = 0;
  PeerState* selected = nullptr;
  for (auto& peer : peers) {
    if (countsAgainstActiveCap(peer, now)) {
      ++active;
    }
    if (!canConnect(peer, now)) {
      continue;
    }
    if (!selected || betterConnectCandidate(peer, *selected)) {
      selected = &peer;
    }
  }
  if (activeSourceCap > 0 && active >= activeSourceCap) {
    return nullptr;
  }
  return selected;
}

bool serverTcpSourceRequestDue(const ServerState& server, int64_t fileSize,
                               int64_t now)
{
  if (server.connecting || server.connected || !retryDue(server, now)) {
    return false;
  }
  if (!server.handshakeCompleted) {
    return true;
  }
  if (!supportsTcpFileSize(server, fileSize)) {
    return false;
  }
  if (sourceResponseFresh(server, now)) {
    return false;
  }
  return server.nextSourceRequestTime != 0 &&
         server.nextSourceRequestTime <= now;
}

bool serverUdpSourceRequestDue(const ServerState& server, int64_t fileSize,
                               int64_t now)
{
  if (!server.handshakeCompleted || !retryDue(server, now) ||
      sourceResponseFresh(server, now) ||
      !supportsUdpFileSize(server, fileSize)) {
    return false;
  }
  if ((server.udpFlags & (SRV_UDPFLG_EXT_GETSOURCES |
                          SRV_UDPFLG_EXT_GETSOURCES2)) == 0) {
    return false;
  }
  return server.lastUdpSourceRequestTime == 0 ||
         now - server.lastUdpSourceRequestTime >=
             SERVER_UDP_SOURCE_REASK_INTERVAL;
}

std::vector<std::shared_ptr<Segment>>
selectRequestSegments(SegmentMan* segmentMan, cuid_t cuid,
                      const std::vector<bool>& peerAvailability,
                      size_t maxSegments)
{
  std::vector<std::shared_ptr<Segment>> segments;
  if (!segmentMan || maxSegments == 0) {
    return segments;
  }

  std::vector<std::shared_ptr<Segment>> inFlight;
  segmentMan->getInFlightSegment(inFlight, cuid);
  for (const auto& segment : inFlight) {
    if (segments.size() >= maxSegments) {
      return segments;
    }
    if (!segment || segment->complete()) {
      continue;
    }
    const auto index = segment->getIndex();
    if (!peerAvailability.empty() &&
        (index >= peerAvailability.size() || !peerAvailability[index])) {
      continue;
    }
    segments.push_back(segment);
  }

  for (size_t index = 0;
       segments.size() < maxSegments && index < peerAvailability.size();
       ++index) {
    if (!peerAvailability[index]) {
      continue;
    }
    auto segment = segmentMan->getSegmentWithIndex(cuid, index);
    if (!segment) {
      segment = segmentMan->getCleanSegmentIfOwnerIsIdle(cuid, index);
    }
    if (segment) {
      segments.push_back(segment);
    }
  }

  while (segments.size() < maxSegments && peerAvailability.empty()) {
    auto segment = segmentMan->getSegment(cuid, BLOCK_LENGTH);
    if (!segment) {
      break;
    }
    segments.push_back(segment);
  }
  return segments;
}

} // namespace ed2k

} // namespace aria2
