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

#include "Segment.h"
#include "SegmentMan.h"
#include "ed2k_hash.h"

namespace aria2 {

namespace ed2k {

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
bool canConnect(const PeerState& peer, int64_t now)
{
  if (peer.lowId && (peer.callbackRequested || peer.callbackImpossible)) {
    return false;
  }
  if (peer.connecting || peer.accepted || peer.noFile || peer.cancelled) {
    return false;
  }
  if (peer.dead && peer.nextRetryTime > now) {
    return false;
  }
  return true;
}
} // namespace

PeerState* selectConnectPeer(std::vector<PeerState>& peers, int64_t now)
{
  PeerState* selected = nullptr;
  for (auto& peer : peers) {
    if (!canConnect(peer, now)) {
      continue;
    }
    if (!selected) {
      selected = &peer;
      continue;
    }
    if (peer.failCount != selected->failCount) {
      if (peer.failCount < selected->failCount) {
        selected = &peer;
      }
      continue;
    }
    if (sourcePriority(peer.sourceFlags) >
        sourcePriority(selected->sourceFlags)) {
      selected = &peer;
      continue;
    }
    if (selected->queued != peer.queued && peer.queued) {
      selected = &peer;
      continue;
    }
    if (peer.queueRank != 0 &&
        (selected->queueRank == 0 || peer.queueRank < selected->queueRank)) {
      selected = &peer;
    }
  }
  return selected;
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
