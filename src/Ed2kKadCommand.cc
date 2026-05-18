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
#include "Ed2kKadCommand.h"

#include <algorithm>
#include <array>

#include "DlAbortEx.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Ed2kAttribute.h"
#include "LogFactory.h"
#include "Logger.h"
#include "Option.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SimpleRandomizer.h"
#include "SocketCore.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_kad_search.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"
#include "ed2k_search.h"
#include "ed2k_server.h"
#include "fmt.h"
#include "prefs.h"
#include "util.h"
#include "wallclock.h"

namespace aria2 {

namespace {

std::string clientKadId(const DownloadEngine* e)
{
  auto id = e->getSessionId();
  if (id.size() >= ed2k::HASH_LENGTH) {
    return id.substr(0, ed2k::HASH_LENGTH);
  }
  id.append(ed2k::HASH_LENGTH - id.size(), '\0');
  return id;
}

ed2k::Endpoint toEndpoint(const ed2k::KadContact& contact)
{
  ed2k::Endpoint endpoint;
  endpoint.host = contact.host;
  endpoint.port = contact.udpPort;
  return endpoint;
}

constexpr int64_t SERVER_STATUS_POLL_INTERVAL = 45;
constexpr int64_t FIREWALLED_CHECK_INTERVAL = 3600;
constexpr int64_t SOURCE_PUBLISH_INTERVAL = 1800;

int64_t peerRetryWait(const DownloadEngine* e)
{
  return std::max<int64_t>(1, e->getOption()->getAsInt(PREF_RETRY_WAIT));
}

uint32_t createChallenge()
{
  uint32_t challenge = 0;
  SimpleRandomizer::getInstance()->getRandomBytes(
      reinterpret_cast<unsigned char*>(&challenge), sizeof(challenge));
  return challenge == 0 ? 1 : challenge;
}

ed2k::Endpoint serverUdpEndpoint(const ed2k::Endpoint& server)
{
  ed2k::Endpoint endpoint;
  endpoint.host = server.host;
  endpoint.port = server.port + 4;
  return endpoint;
}

bool publishableAddress(const std::string& host)
{
  return !host.empty() && host != "0.0.0.0" && host != "127.0.0.1" &&
         host.compare(0, 4, "127.") != 0 && !util::inPrivateAddress(host);
}

} // namespace

Ed2kKadCommand::Ed2kKadCommand(cuid_t cuid, RequestGroup* requestGroup,
                               DownloadEngine* e)
    : Command(cuid),
      requestGroup_(requestGroup),
      e_(e),
      socket_(std::make_shared<SocketCore>(SOCK_DGRAM)),
      initialized_(false),
      sourceSearchSent_(false),
      keywordSearchSent_(false),
      lastServerStatusPoll_(0)
{
  setStatusRealtime();
}

Ed2kKadCommand::~Ed2kKadCommand()
{
  if (initialized_) {
    e_->deleteSocketForReadCheck(socket_, this);
  }
}

uint16_t Ed2kKadCommand::getLocalUdpPort() const
{
  return socket_->getAddrInfo().port;
}

bool Ed2kKadCommand::waitLocalUdpReadable(time_t timeout) const
{
  return socket_->isReadable(timeout);
}

int64_t Ed2kKadCommand::nowSeconds() const
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             global::wallclock().getTime().time_since_epoch())
      .count();
}

void Ed2kKadCommand::init()
{
  socket_->bind(nullptr, 0, AF_INET);
  socket_->setNonBlockingMode();
  e_->addSocketForReadCheck(socket_, this);
  initialized_ = true;

  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable) {
    attrs->kadRoutingTable =
        std::make_shared<ed2k::KadRoutingTable>(clientKadId(e_));
  }
  for (const auto& source : attrs->link.sources) {
    ed2k::Endpoint endpoint;
    endpoint.host = source.host;
    endpoint.port = source.port == 0 ? 4672 : source.port;
    attrs->kadRoutingTable->addRouterNode(endpoint);
  }
  queueBootstrap();
}

void Ed2kKadCommand::queuePacket(const ed2k::Endpoint& endpoint, uint8_t opcode,
                                 const std::string& payload)
{
  outbox_.push_back(std::make_pair(
      endpoint, ed2k::createPacket(ed2k::KAD_PROTOCOL, opcode, payload)));
}

void Ed2kKadCommand::queueEd2kUdpPacket(const ed2k::Endpoint& endpoint,
                                        uint8_t opcode,
                                        const std::string& payload)
{
  outbox_.push_back(std::make_pair(
      endpoint, ed2k::createPacket(ed2k::PROTO_EDONKEY, opcode, payload)));
}

void Ed2kKadCommand::queueEmuleUdpPacket(const ed2k::Endpoint& endpoint,
                                         uint8_t opcode,
                                         const std::string& payload)
{
  outbox_.push_back(std::make_pair(
      endpoint, ed2k::createPacket(ed2k::PROTO_EMULE, opcode, payload)));
}

void Ed2kKadCommand::queueServerStatusPoll()
{
  const auto now = nowSeconds();
  if (lastServerStatusPoll_ != 0 &&
      now - lastServerStatusPoll_ < SERVER_STATUS_POLL_INTERVAL) {
    return;
  }
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  for (const auto& server : attrs->servers) {
    auto state = getEd2kServerState(attrs, server);
    if (!state || !state->handshakeCompleted || server.port > 65531) {
      continue;
    }
    state->udpStatusChallenge = createChallenge();
    state->lastUdpStatusTime = now;
    queueEd2kUdpPacket(serverUdpEndpoint(server), ed2k::OP_GLOBSERVSTATREQ,
                       ed2k::packUInt32(state->udpStatusChallenge));
  }
  lastServerStatusPoll_ = now;
}

void Ed2kKadCommand::queueBootstrap()
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable || !attrs->kadRoutingTable->needBootstrap(nowSeconds())) {
    return;
  }
  for (const auto& endpoint : attrs->kadRoutingTable->getRouterNodes()) {
    queuePacket(endpoint, ed2k::KAD_BOOTSTRAP_REQ, std::string());
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.purpose = ed2k::KadTransactionPurpose::BOOTSTRAP;
    tx.expectedOpcode = ed2k::KAD_BOOTSTRAP_RES;
    tx.sentTime = nowSeconds();
    attrs->kadTransactions.add(tx);
  }
}

void Ed2kKadCommand::queueRefresh()
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable || attrs->kadRoutingTable->liveSize() == 0) {
    return;
  }
  std::string targetId;
  if (!attrs->kadRoutingTable->needRefresh(targetId, nowSeconds())) {
    return;
  }
  auto contacts = attrs->kadRoutingTable->findClosest(targetId, 8, true);
  for (const auto& contact : contacts) {
    const auto endpoint = toEndpoint(contact);
    queuePacket(endpoint, ed2k::KAD_REQ,
                ed2k::createKadRequestPayload(ed2k::KAD_FIND_NODE, targetId,
                                               contact.id));
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.contact = contact;
    tx.purpose = ed2k::KadTransactionPurpose::REFRESH;
    tx.expectedOpcode = ed2k::KAD_RES;
    tx.targetId = targetId;
    tx.sentTime = nowSeconds();
    attrs->kadTransactions.add(tx);
  }
}

void Ed2kKadCommand::queueFirewalledCheck()
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable || attrs->kadRoutingTable->liveSize() == 0) {
    return;
  }
  const auto tcpPort =
      static_cast<uint16_t>(e_->getOption()->getAsInt(PREF_ED2K_LISTEN_PORT));
  if (tcpPort == 0) {
    return;
  }
  const auto now = nowSeconds();
  if (attrs->lastKadFirewalledCheck != 0 &&
      now - attrs->lastKadFirewalledCheck < FIREWALLED_CHECK_INTERVAL) {
    return;
  }
  auto contacts = attrs->kadRoutingTable->findClosest(clientKadId(e_), 8, true);
  if (contacts.empty()) {
    return;
  }
  attrs->lastKadFirewalledCheck = now;
  for (const auto& contact : contacts) {
    const auto endpoint = toEndpoint(contact);
    queuePacket(endpoint, ed2k::KAD_FIREWALLED_REQ,
                ed2k::createKadFirewalledRequestPayload(tcpPort,
                                                         clientKadId(e_), 0));
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.contact = contact;
    tx.purpose = ed2k::KadTransactionPurpose::FIREWALLED_CHECK;
    tx.expectedOpcode = ed2k::KAD_FIREWALLED_RES;
    tx.sentTime = now;
    attrs->kadTransactions.add(tx);
  }
}

void Ed2kKadCommand::queueSourcePublish()
{
  if (!requestGroup_->downloadFinished()) {
    return;
  }
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable || attrs->kadRoutingTable->liveSize() == 0 ||
      attrs->link.hash.empty()) {
    return;
  }
  const auto tcpPort =
      static_cast<uint16_t>(e_->getOption()->getAsInt(PREF_ED2K_LISTEN_PORT));
  if (tcpPort == 0) {
    return;
  }
  const auto now = nowSeconds();
  if (attrs->lastKadSourcePublish != 0 &&
      now - attrs->lastKadSourcePublish < SOURCE_PUBLISH_INTERVAL) {
    return;
  }
  auto observed = std::find_if(attrs->kadObservedAddresses.begin(),
                               attrs->kadObservedAddresses.end(),
                               publishableAddress);
  if (observed == attrs->kadObservedAddresses.end()) {
    return;
  }
  auto contacts = attrs->kadRoutingTable->findClosest(attrs->link.hash, 8, true);
  if (contacts.empty()) {
    return;
  }
  ed2k::Endpoint source;
  source.host = *observed;
  source.port = tcpPort;
  const auto sourceId = clientKadId(e_);
  const auto payload = ed2k::createKadPublishSourceRequestPayload(
      attrs->link.hash, source, sourceId, attrs->link.size);
  ed2k::KadPublishSourceRequest request;
  if (!ed2k::parseKadPublishSourceRequestPayload(request, payload)) {
    return;
  }
  attrs->kadSourceIndex.store(attrs->link.hash, request.source);
  attrs->lastKadSourcePublish = now;
  for (const auto& contact : contacts) {
    queuePacket(toEndpoint(contact), ed2k::KAD_PUBLISH_SOURCE_REQ, payload);
  }
}

void Ed2kKadCommand::queueTraversalActions(
    ed2k::KadTraversal& traversal,
    const std::vector<ed2k::KadTraversalAction>& actions)
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  for (const auto& action : actions) {
    const auto endpoint = toEndpoint(action.contact);
    if (action.type == ed2k::KadTraversalActionType::FIND_NODE) {
      const auto searchType =
          traversal.kind() == ed2k::KadTraversalKind::KEYWORD_LOOKUP
              ? ed2k::KAD_FIND_VALUE
              : ed2k::KAD_FIND_NODE;
      queuePacket(endpoint, ed2k::KAD_REQ,
                  ed2k::createKadRequestPayload(
                      searchType, traversal.targetId(), action.contact.id));
      ed2k::KadTransaction tx;
      tx.endpoint = endpoint;
      tx.contact = action.contact;
      tx.purpose =
          traversal.kind() == ed2k::KadTraversalKind::KEYWORD_LOOKUP
              ? ed2k::KadTransactionPurpose::KEYWORD_LOOKUP
              : ed2k::KadTransactionPurpose::SOURCE_LOOKUP;
      tx.expectedOpcode = ed2k::KAD_RES;
      tx.targetId = traversal.targetId();
      tx.sentTime = nowSeconds();
      attrs->kadTransactions.add(tx);
      continue;
    }

    if (traversal.kind() == ed2k::KadTraversalKind::KEYWORD_LOOKUP) {
      queuePacket(endpoint, ed2k::KAD_SEARCH_KEYS_REQ,
                  ed2k::createKadSearchKeysRequestPayload(
                      traversal.targetId(), 0));
    }
    else {
      queuePacket(endpoint, ed2k::KAD_SEARCH_SOURCES_REQ,
                  ed2k::createKadSearchSourcesRequestPayload(
                      traversal.targetId(), 0, traversal.size()));
    }
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.contact = action.contact;
    tx.purpose = traversal.kind() == ed2k::KadTraversalKind::KEYWORD_LOOKUP
                     ? ed2k::KadTransactionPurpose::KEYWORD_LOOKUP
                     : ed2k::KadTransactionPurpose::SOURCE_LOOKUP;
    tx.expectedOpcode = ed2k::KAD_SEARCH_RES;
    tx.targetId = traversal.targetId();
    tx.sentTime = nowSeconds();
    attrs->kadTransactions.add(tx);
  }
}

void Ed2kKadCommand::queueSourceSearch()
{
  if (sourceSearchSent_) {
    return;
  }
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable || attrs->kadRoutingTable->liveSize() == 0) {
    return;
  }
  auto contacts = attrs->kadRoutingTable->findClosest(attrs->link.hash, 8, true);
  if (contacts.empty()) {
    return;
  }
  attrs->kadSourceTraversal = make_unique<ed2k::KadTraversal>(
      ed2k::KadTraversalKind::SOURCE_LOOKUP, attrs->link.hash,
      attrs->link.size);
  queueTraversalActions(*attrs->kadSourceTraversal,
                        attrs->kadSourceTraversal->start(contacts));
  sourceSearchSent_ = true;
}

void Ed2kKadCommand::queueKeywordSearch()
{
  if (keywordSearchSent_) {
    return;
  }
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->searchActive || !attrs->kadRoutingTable ||
      attrs->kadRoutingTable->liveSize() == 0) {
    return;
  }
  const auto targetId = ed2k::createKadKeywordTarget(attrs->searchQuery.keyword);
  auto contacts = attrs->kadRoutingTable->findClosest(targetId, 8, true);
  if (contacts.empty()) {
    return;
  }
  attrs->kadKeywordTraversal = make_unique<ed2k::KadTraversal>(
      ed2k::KadTraversalKind::KEYWORD_LOOKUP, targetId, 0);
  queueTraversalActions(*attrs->kadKeywordTraversal,
                        attrs->kadKeywordTraversal->start(contacts));
  keywordSearchSent_ = true;
}

void Ed2kKadCommand::sendQueuedPackets()
{
  while (!outbox_.empty()) {
    auto item = outbox_.front();
    socket_->writeData(item.second.data(), item.second.size(), item.first.host,
                       item.first.port);
    outbox_.pop_front();
  }
}

void Ed2kKadCommand::receivePackets()
{
  std::array<unsigned char, 64_k> data;
  while (true) {
    Endpoint sender;
    const auto length = socket_->readDataFrom(data.data(), data.size(), sender);
    if (length <= 0) {
      break;
    }
    if (length < 6) {
      continue;
    }
    ed2k::PacketHeader header;
    if (!ed2k::readPacketHeader(header, reinterpret_cast<const char*>(data.data()),
                                static_cast<size_t>(length)) ||
        (header.protocol != ed2k::KAD_PROTOCOL &&
         header.protocol != ed2k::PROTO_EDONKEY &&
         header.protocol != ed2k::PROTO_EMULE) ||
        header.payloadSize() + 6 != static_cast<size_t>(length)) {
      continue;
    }
    ed2k::Endpoint endpoint;
    endpoint.host = sender.addr;
    endpoint.port = sender.port;
    std::string payload(reinterpret_cast<const char*>(data.data()) + 6,
                        reinterpret_cast<const char*>(data.data()) + length);
    if (header.protocol == ed2k::PROTO_EDONKEY ||
        header.protocol == ed2k::PROTO_EMULE) {
      handleEd2kUdpPacket(endpoint, header.opcode, payload);
    }
    else {
      handlePacket(endpoint, header.opcode, payload);
    }
  }
}

void Ed2kKadCommand::handleEd2kUdpPacket(const ed2k::Endpoint& endpoint,
                                         uint8_t opcode,
                                         const std::string& payload)
{
  if (opcode == ed2k::OP_REASKACK) {
    ed2k::UdpReaskAck ack;
    if (ed2k::parseUdpReaskAckPayload(ack, payload)) {
      markEd2kPeerQueued(getEd2kAttrs(requestGroup_->getDownloadContext()),
                         endpoint, ack.rank, ack.bitfield);
    }
    return;
  }
  if (opcode == ed2k::OP_QUEUEFULL || opcode == ed2k::OP_FILENOTFOUND) {
    markEd2kPeerDead(getEd2kAttrs(requestGroup_->getDownloadContext()),
                     endpoint, nowSeconds(), peerRetryWait(e_));
    return;
  }
  if (opcode == ed2k::OP_REASKFILEPING) {
    ed2k::UdpReask reask;
    auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
    if (!ed2k::parseUdpReaskFilePingPayload(reask, payload) ||
        reask.fileHash != attrs->link.hash) {
      queueEmuleUdpPacket(endpoint, ed2k::OP_FILENOTFOUND, std::string());
      return;
    }
    queueEmuleUdpPacket(endpoint, ed2k::OP_REASKACK,
                       ed2k::createUdpReaskAckPayload(0));
    return;
  }
  if (opcode != ed2k::OP_GLOBSERVSTATRES || endpoint.port < 4) {
    return;
  }
  ed2k::Endpoint server;
  server.host = endpoint.host;
  server.port = endpoint.port - 4;
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  auto state = getEd2kServerState(attrs, server);
  if (!state) {
    return;
  }
  ed2k::ServerStatus status;
  if (!ed2k::parseServerStatusPayload(status, payload) ||
      status.challenge == 0 ||
      status.challenge != state->udpStatusChallenge) {
    return;
  }
  updateEd2kServerUdpStatus(attrs, server, status, nowSeconds());
}

void Ed2kKadCommand::handlePacket(const ed2k::Endpoint& endpoint,
                                  uint8_t opcode,
                                  const std::string& payload)
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable) {
    return;
  }
  if (opcode == ed2k::KAD_BOOTSTRAP_RES) {
    ed2k::KadBootstrapResponse response;
    if (!ed2k::parseKadBootstrapResponsePayload(response, payload)) {
      return;
    }
    ed2k::KadContact sender;
    sender.id = response.id;
    sender.host = endpoint.host;
    sender.udpPort = endpoint.port;
    sender.tcpPort = response.tcpPort;
    sender.version = response.version;
    attrs->kadRoutingTable->nodeSeen(sender, nowSeconds());
    for (const auto& contact : response.contacts) {
      attrs->kadRoutingTable->heardAbout(contact, nowSeconds());
      queuePacket(toEndpoint(contact), ed2k::KAD_HELLO_REQ,
                  ed2k::createKadHelloPayload(clientKadId(e_), 0, 8));
    }
    return;
  }
  if (opcode == ed2k::KAD_HELLO_REQ || opcode == ed2k::KAD_HELLO_RES) {
    ed2k::KadHello hello;
    if (ed2k::parseKadHelloPayload(hello, payload)) {
      ed2k::KadContact contact;
      contact.id = hello.id;
      contact.host = endpoint.host;
      contact.udpPort = endpoint.port;
      contact.tcpPort = hello.tcpPort;
      contact.version = hello.version;
      attrs->kadRoutingTable->nodeSeen(contact, nowSeconds());
    }
    if (opcode == ed2k::KAD_HELLO_REQ) {
      queuePacket(endpoint, ed2k::KAD_HELLO_RES,
                  ed2k::createKadHelloPayload(clientKadId(e_), 0, 8));
    }
    return;
  }
  if (opcode == ed2k::KAD_RES) {
    ed2k::KadResponse response;
    if (!ed2k::parseKadResponsePayload(response, payload)) {
      return;
    }
    ed2k::KadTransaction tx;
    const auto knownResponse =
        attrs->kadTransactions.complete(endpoint, opcode, response.targetId,
                                        tx);
    if (!knownResponse) {
      return;
    }
    attrs->kadRoutingTable->nodeSeen(tx.contact, nowSeconds());
    for (const auto& contact : response.contacts) {
      attrs->kadRoutingTable->heardAbout(contact, nowSeconds());
    }
    if (tx.purpose == ed2k::KadTransactionPurpose::KEYWORD_LOOKUP &&
        attrs->kadKeywordTraversal) {
      queueTraversalActions(*attrs->kadKeywordTraversal,
                            attrs->kadKeywordTraversal->onResponse(
                                tx.contact, response.contacts));
    }
    else if (tx.purpose == ed2k::KadTransactionPurpose::SOURCE_LOOKUP &&
             attrs->kadSourceTraversal) {
      queueTraversalActions(*attrs->kadSourceTraversal,
                            attrs->kadSourceTraversal->onResponse(
                                tx.contact, response.contacts));
    }
    return;
  }
  if (opcode == ed2k::KAD_SEARCH_RES) {
    ed2k::KadSearchResult result;
    if (ed2k::parseKadSearchResultPayload(result, payload)) {
      auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
      ed2k::KadTransaction tx;
      attrs->kadTransactions.complete(endpoint, opcode, result.targetId, tx);
      if (attrs->searchActive) {
        auto entries = ed2k::kadSearchEntriesToSearchResults(result.entries,
                                                            "kad");
        addEd2kSearchResults(attrs, entries, false);
        return;
      }
      auto peers = ed2k::extractKadSourceEndpoints(result);
      for (const auto& peer : peers) {
        addEd2kPeer(attrs, peer, ed2k::PEER_SOURCE_KAD);
      }
      schedulePendingEd2kPeers(requestGroup_, e_);
    }
    return;
  }
  if (opcode == ed2k::KAD_FIREWALLED_RES) {
    ed2k::KadFirewalledResponse response;
    if (!ed2k::parseKadFirewalledResponsePayload(response, payload)) {
      return;
    }
    ed2k::KadTransaction tx;
    if (attrs->kadTransactions.complete(endpoint, opcode, tx)) {
      attrs->kadRoutingTable->nodeSeen(tx.contact, nowSeconds());
    }
    if (std::find(attrs->kadObservedAddresses.begin(),
                  attrs->kadObservedAddresses.end(),
                  response.ipAddress) == attrs->kadObservedAddresses.end()) {
      attrs->kadObservedAddresses.push_back(response.ipAddress);
    }
    attrs->kadFirewalled = response.ipAddress.empty();
    return;
  }
  if (opcode == ed2k::KAD_PUBLISH_SOURCE_REQ) {
    ed2k::KadPublishSourceRequest request;
    if (!ed2k::parseKadPublishSourceRequestPayload(request, payload)) {
      return;
    }
    attrs->kadSourceIndex.store(request.fileId, request.source);
    queuePacket(endpoint, ed2k::KAD_PUBLISH_RES,
                ed2k::createKadPublishResultPayload(request.fileId, 1));
    return;
  }
  if (opcode == ed2k::KAD_SEARCH_SOURCES_REQ) {
    ed2k::KadSearchSourcesRequest request;
    if (!ed2k::parseKadSearchSourcesRequestPayload(request, payload)) {
      return;
    }
    auto entries = attrs->kadSourceIndex.find(request.targetId,
                                             request.startPosition, 50);
    if (!entries.empty()) {
      queuePacket(endpoint, ed2k::KAD_SEARCH_RES,
                  ed2k::createKadSearchResultPayload(
                      clientKadId(e_), request.targetId, entries));
    }
    return;
  }
  if (opcode == ed2k::KAD_FIREWALLED_REQ) {
    ed2k::KadFirewalledRequest request;
    if (!ed2k::parseKadFirewalledRequestPayload(request, payload)) {
      return;
    }
    queuePacket(endpoint, ed2k::KAD_FIREWALLED_RES,
                ed2k::createKadFirewalledResponsePayload(endpoint.host));
    return;
  }
}

bool Ed2kKadCommand::execute()
{
  if (requestGroup_->isHaltRequested() || e_->isHaltRequested() ||
      (e_->getRequestGroupMan()->downloadFinished() &&
       !requestGroup_->downloadFinished())) {
    return true;
  }
  try {
    if (!initialized_) {
      init();
    }
    receivePackets();
    auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
    const auto expired = attrs->kadTransactions.expire(nowSeconds(), 12);
    if (attrs->kadRoutingTable) {
      for (const auto& tx : expired) {
        attrs->kadRoutingTable->nodeFailed(tx.contact);
        if (tx.purpose == ed2k::KadTransactionPurpose::KEYWORD_LOOKUP &&
            attrs->kadKeywordTraversal) {
          queueTraversalActions(*attrs->kadKeywordTraversal,
                                attrs->kadKeywordTraversal->onFailure(
                                    tx.contact));
        }
        else if (tx.purpose == ed2k::KadTransactionPurpose::SOURCE_LOOKUP &&
                 attrs->kadSourceTraversal) {
          queueTraversalActions(*attrs->kadSourceTraversal,
                                attrs->kadSourceTraversal->onFailure(
                                    tx.contact));
        }
      }
    }
    schedulePendingEd2kServers(requestGroup_, e_);
    queueServerStatusPoll();
    queueBootstrap();
    if (!requestGroup_->downloadFinished()) {
      if (attrs->searchActive) {
        queueKeywordSearch();
      }
      else {
        queueSourceSearch();
      }
    }
    queueRefresh();
    queueFirewalledCheck();
    queueSourcePublish();
    sendQueuedPackets();
    if (requestGroup_->downloadFinished()) {
      return true;
    }
  }
  catch (DlAbortEx& e) {
    A2_LOG_INFO_EX("Exception thrown while handling ED2K Kad.", e);
  }
  e_->addRoutineCommand(std::unique_ptr<Command>(this));
  return false;
}

} // namespace aria2
