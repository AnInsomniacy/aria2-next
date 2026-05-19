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
#ifndef D_ED2K_ATTRIBUTE_H
#define D_ED2K_ATTRIBUTE_H

#include "ContextAttribute.h"
#include "Ed2kKadState.h"
#include "ed2k_aich.h"
#include "ed2k_link.h"
#include "ed2k_peer.h"
#include "ed2k_search.h"
#include "ed2k_server.h"

#include <memory>
#include <vector>

namespace aria2 {

class Command;
class DownloadEngine;
class RequestGroup;

class DownloadContext;

struct Ed2kAttribute : public ContextAttribute {
  ed2k::Link link;
  std::vector<ed2k::Endpoint> servers;
  std::vector<ed2k::ServerState> serverStates;
  std::vector<ed2k::Endpoint> peers;
  std::vector<ed2k::PeerState> peerStates;
  std::vector<std::string> pieceHashes;
  std::string aichRootHash;
  std::vector<ed2k::AichRecoverySet> aichRecoverySets;
  ed2k::SearchQuery searchQuery;
  std::vector<ed2k::SearchResultEntry> searchResults;
  std::shared_ptr<ed2k::KadRoutingTable> kadRoutingTable;
  ed2k::KadTransactionTable kadTransactions;
  ed2k::KadSourceIndex kadSourceIndex;
  std::unique_ptr<ed2k::KadTraversal> kadSourceTraversal;
  std::unique_ptr<ed2k::KadTraversal> kadKeywordTraversal;
  int64_t lastKadFirewalledCheck = 0;
  int64_t lastKadSourcePublish = 0;
  std::vector<std::string> kadObservedAddresses;
  bool kadFirewalled = true;
  size_t nextServerIndex = 0;
  bool searchActive = false;
  bool searchMoreResults = false;
};

Ed2kAttribute* getEd2kAttrs(DownloadContext* dctx);
Ed2kAttribute* getEd2kAttrs(const std::shared_ptr<DownloadContext>& dctx);
bool addEd2kPeer(Ed2kAttribute* attrs, const ed2k::Endpoint& peer);
bool addEd2kPeer(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                 uint32_t sourceFlag);
bool addEd2kFoundSource(Ed2kAttribute* attrs, const ed2k::FoundSource& source,
                        uint32_t sourceFlag, bool callbackRequested);
size_t mergeEd2kServerSources(Ed2kAttribute* attrs,
                              const std::vector<ed2k::FoundSource>& sources,
                              uint32_t sourceFlag);
size_t mergeEd2kSourceExchangePeers(
    Ed2kAttribute* attrs, const std::vector<ed2k::SourceExchangeEntry>& entries,
    const ed2k::Endpoint& remotePeer);
ed2k::PeerState* getEd2kPeerState(Ed2kAttribute* attrs,
                                  const ed2k::Endpoint& peer);
bool markEd2kPeerQueued(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                        uint16_t rank,
                        const std::vector<bool>& partStatus);
bool markEd2kPeerConnecting(Ed2kAttribute* attrs, const ed2k::Endpoint& peer);
bool markEd2kPeerDisconnected(Ed2kAttribute* attrs,
                              const ed2k::Endpoint& peer);
bool markEd2kCallbackFailed(Ed2kAttribute* attrs, uint32_t clientId);
bool updateEd2kPeerPartStatus(Ed2kAttribute* attrs,
                              const ed2k::Endpoint& peer,
                              const std::vector<bool>& partStatus);
bool updateEd2kPeerRequestedParts(
    Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
    const std::vector<ed2k::PartRange>& ranges);
bool clearEd2kPeerRequestedParts(Ed2kAttribute* attrs,
                                 const ed2k::Endpoint& peer);
bool markEd2kPeerAccepted(Ed2kAttribute* attrs, const ed2k::Endpoint& peer);
bool markEd2kPeerOutOfParts(Ed2kAttribute* attrs,
                            const ed2k::Endpoint& peer);
bool markEd2kPeerCancelled(Ed2kAttribute* attrs, const ed2k::Endpoint& peer);
bool markEd2kPeerFailure(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                         int64_t now, int64_t baseRetrySeconds);
bool markEd2kPeerDead(Ed2kAttribute* attrs, const ed2k::Endpoint& peer,
                      int64_t now, int64_t baseRetrySeconds);
ed2k::ServerState* getEd2kServerState(Ed2kAttribute* attrs,
                                      const ed2k::Endpoint& server);
ed2k::ServerState* updateEd2kServerConnecting(Ed2kAttribute* attrs,
                                              const ed2k::Endpoint& server);
ed2k::ServerState* updateEd2kServerConnected(Ed2kAttribute* attrs,
                                             const ed2k::Endpoint& server);
void updateEd2kServerIdChange(Ed2kAttribute* attrs,
                              const ed2k::Endpoint& server,
                              const ed2k::ServerIdChange& idChange);
void updateEd2kServerStatus(Ed2kAttribute* attrs,
                            const ed2k::Endpoint& server,
                            const ed2k::ServerStatus& status);
void updateEd2kServerUdpStatus(Ed2kAttribute* attrs,
                               const ed2k::Endpoint& server,
                               const ed2k::ServerStatus& status, int64_t now);
void updateEd2kServerMessage(Ed2kAttribute* attrs,
                             const ed2k::Endpoint& server,
                             const std::string& message);
void updateEd2kServerSourceRequestTime(Ed2kAttribute* attrs,
                                       const ed2k::Endpoint& server,
                                       int64_t nextTime);
void markEd2kServerSourceRequestFinished(Ed2kAttribute* attrs,
                                         const ed2k::Endpoint& server);
void updateEd2kServerFailure(Ed2kAttribute* attrs,
                             const ed2k::Endpoint& server, int64_t now,
                             int64_t baseRetrySeconds);
size_t addEd2kSearchResults(Ed2kAttribute* attrs,
                            const std::vector<ed2k::SearchResultEntry>& entries,
                            bool moreResults);
void schedulePendingEd2kServers(RequestGroup* requestGroup, DownloadEngine* e);
void schedulePendingEd2kServers(std::vector<std::unique_ptr<Command>>& commands,
                                RequestGroup* requestGroup,
                                DownloadEngine* e);
void schedulePendingEd2kPeers(RequestGroup* requestGroup, DownloadEngine* e);
ed2k::KadRoutingSnapshot createEd2kKadSnapshot(const Ed2kAttribute* attrs);
void restoreEd2kKadOperationalState(Ed2kAttribute* attrs,
                                    const ed2k::KadRoutingSnapshot& snapshot);

} // namespace aria2

#endif // D_ED2K_ATTRIBUTE_H
