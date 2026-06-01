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
#ifndef D_ED2K_POLICY_H
#define D_ED2K_POLICY_H

#include "common.h"

#include <memory>
#include <vector>

#include "Command.h"
#include "ed2k_peer.h"
#include "ed2k_server.h"

namespace aria2 {

class Segment;
class SegmentMan;

namespace ed2k {

constexpr int64_t SERVER_TCP_SOURCE_REASK_INTERVAL = 800;
constexpr int64_t SERVER_UDP_SOURCE_REASK_INTERVAL = 1300;
constexpr int64_t PEER_UDP_REASK_INTERVAL = 1300;
constexpr int DEFAULT_PEER_CONNECTIONS = 20;
constexpr int64_t ENDGAME_RECLAIM_STALL_SECONDS = 10;
constexpr int64_t ACTIVE_ENDGAME_RECLAIM_STALL_SECONDS = 60;

enum class PeerLifecycle {
  USEFUL,
  CONNECTING,
  QUEUED,
  DOWNLOADING,
  NO_NEEDED_PARTS,
  CALLBACK_WAITING,
  DEAD,
  RETRYING,
  NO_FILE,
  CANCELLED
};

enum class PeerActionType {
  WAIT,
  CONNECT,
  REASK,
  REQUEST_CALLBACK,
  RETRY,
  SKIP,
  EXPIRE
};

struct PeerAction {
  PeerAction() = default;
  PeerAction(PeerActionType type, PeerState* peer) : type(type), peer(peer) {}

  PeerActionType type = PeerActionType::WAIT;
  PeerState* peer = nullptr;
};

int sourcePriority(uint32_t sourceFlags);
PeerLifecycle classifyPeerLifecycle(const PeerState& peer, int64_t now);
PeerAction selectPeerAction(std::vector<PeerState>& peers, int64_t now);
PeerAction selectPeerAction(std::vector<PeerState>& peers, int64_t now,
                            size_t activeSourceCap);
PeerState* selectConnectPeer(std::vector<PeerState>& peers, int64_t now);
PeerState* selectConnectPeer(std::vector<PeerState>& peers, int64_t now,
                             size_t activeSourceCap);
bool serverTcpSourceRequestDue(const ServerState& server, int64_t fileSize,
                               int64_t now);
bool serverUdpSourceRequestDue(const ServerState& server, int64_t fileSize,
                               int64_t now);
std::vector<std::shared_ptr<Segment>>
selectRequestSegments(SegmentMan* segmentMan, cuid_t cuid,
                      const std::vector<bool>& peerAvailability,
                      size_t maxSegments);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_POLICY_H
