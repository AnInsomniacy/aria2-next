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
#include <limits>
#include <memory>

#include "DlAbortEx.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Ed2kAttribute.h"
#include "Ed2kUploadQueue.h"
#include "LogFactory.h"
#include "Logger.h"
#include "Option.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SimpleRandomizer.h"
#include "SocketCore.h"
#include "ed2k_constants.h"
#include "ed2k_compression.h"
#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_kad_search.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"
#include "ed2k_policy.h"
#include "ed2k_search.h"
#include "ed2k_server.h"
#include "fmt.h"
#include "prefs.h"
#include "util.h"
#include "wallclock.h"

namespace aria2 {

namespace {

ed2k::Endpoint toEndpoint(const ed2k::KadContact& contact)
{
  ed2k::Endpoint endpoint;
  endpoint.host = contact.host;
  endpoint.port = contact.udpPort;
  return endpoint;
}

std::string createKadDatagram(uint8_t opcode, const std::string& payload)
{
  return ed2k::createDatagram(ed2k::KAD_PROTOCOL, opcode, payload);
}

constexpr int64_t SERVER_STATUS_POLL_INTERVAL = 45;
constexpr int64_t FIREWALLED_CHECK_INTERVAL = 3600;
constexpr int64_t SOURCE_PUBLISH_INTERVAL = 1800;

bool isKadProtocolDatagram(const std::string& datagram)
{
  ed2k::PacketHeader header;
  return ed2k::readDatagramHeader(header, datagram.data(), datagram.size()) &&
         header.protocol == ed2k::KAD_PROTOCOL &&
         header.payloadSize() + 2 == datagram.size();
}

bool isKnownEd2kUdpProtocol(uint8_t protocol)
{
  return protocol == ed2k::KAD_PROTOCOL || protocol == ed2k::PROTO_EDONKEY ||
         protocol == ed2k::PROTO_EMULE || protocol == ed2k::PROTO_PACKED;
}

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

uint16_t localEd2kTcpPort(const DownloadEngine* e)
{
  const auto configured = e->getOption()->getAsInt(PREF_ED2K_LISTEN_PORT);
  if (configured > 0 &&
      configured <= static_cast<int>(std::numeric_limits<uint16_t>::max())) {
    return static_cast<uint16_t>(configured);
  }
  return 0;
}

uint16_t localEd2kUdpPort(const DownloadEngine* e)
{
  const auto configured = e->getOption()->getAsInt(PREF_ED2K_UDP_LISTEN_PORT);
  if (configured > 0 &&
      configured <= static_cast<int>(std::numeric_limits<uint16_t>::max())) {
    return static_cast<uint16_t>(configured);
  }
  return 0;
}

uint32_t localKadUdpVerifyKey(const Ed2kAttribute* attrs,
                              const ed2k::Endpoint& endpoint)
{
  return attrs ? ed2k::createKadUdpVerifyKey(attrs->kadUdpVerifyKey,
                                             endpoint.host)
               : 0;
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

bool directKadTcpSourceType(uint8_t sourceType)
{
  return sourceType == 0 || sourceType == 1 || sourceType == 4;
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
      lastServerStatusPoll_(0),
      lastServerSourcePoll_(0)
{
  setStatusRealtime();
  requestGroup_->increaseNumCommand();
}

Ed2kKadCommand::~Ed2kKadCommand()
{
  if (initialized_) {
    e_->deleteSocketForReadCheck(socket_, this);
  }
  requestGroup_->decreaseNumCommand();
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
  socket_->bind(nullptr, localEd2kUdpPort(e_), AF_INET);
  socket_->setNonBlockingMode();
  e_->addSocketForReadCheck(socket_, this);
  initialized_ = true;
  A2_LOG_INFO(fmt("IPv4 ED2K Kad: listening on UDP port %u",
                  socket_->getAddrInfo().port));

  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (attrs->kadUdpVerifyKey == 0) {
    attrs->kadUdpVerifyKey = createEd2kKadUdpVerifyKey();
  }
  if (!attrs->kadRoutingTable) {
    attrs->kadRoutingTable =
        std::make_shared<ed2k::KadRoutingTable>(
            ed2k::ed2kHashToKadId(attrs->clientHash));
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
      endpoint, ed2k::createDatagram(ed2k::KAD_PROTOCOL, opcode, payload)));
}

void Ed2kKadCommand::queueKadContactPacket(const ed2k::KadContact& contact,
                                           uint8_t opcode,
                                           const std::string& payload)
{
  auto datagram = createKadDatagram(opcode, payload);
  if (contact.version >= 6 && contact.id.size() == ed2k::HASH_LENGTH) {
    uint16_t randomKeyPart = 0;
    SimpleRandomizer::getInstance()->getRandomBytes(
        reinterpret_cast<unsigned char*>(&randomKeyPart),
        sizeof(randomKeyPart));
    datagram = ed2k::createKadObfuscatedDatagram(
        datagram, contact.id, randomKeyPart, contact.udpKey,
        localKadUdpVerifyKey(getEd2kAttrs(requestGroup_->getDownloadContext()),
                             toEndpoint(contact)));
  }
  outbox_.push_back(std::make_pair(toEndpoint(contact), datagram));
}

void Ed2kKadCommand::queueKadResponsePacket(
    const ed2k::Endpoint& endpoint,
    const ed2k::KadObfuscatedDatagram& context, uint8_t opcode,
    const std::string& payload)
{
  auto datagram = createKadDatagram(opcode, payload);
  if (context.senderVerifyKey != 0) {
    uint16_t randomKeyPart = 0;
    SimpleRandomizer::getInstance()->getRandomBytes(
        reinterpret_cast<unsigned char*>(&randomKeyPart),
        sizeof(randomKeyPart));
    datagram = ed2k::createKadObfuscatedDatagram(
        datagram, context.senderVerifyKey, context.receiverVerifyKey,
        randomKeyPart);
  }
  outbox_.push_back(std::make_pair(endpoint, datagram));
}

bool Ed2kKadCommand::tryDecodeKadObfuscatedDatagram(
    ed2k::KadObfuscatedDatagram& parsed, const ed2k::Endpoint& endpoint,
    const std::string& raw)
{
  if (ed2k::parseKadObfuscatedDatagram(
          parsed, raw, localKadUdpVerifyKey(
                           getEd2kAttrs(requestGroup_->getDownloadContext()),
                           endpoint)) &&
      isKadProtocolDatagram(parsed.datagram)) {
    return true;
  }

  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable) {
    return false;
  }
  if (ed2k::parseKadObfuscatedDatagram(
          parsed, raw, ed2k::ed2kHashToKadId(attrs->clientHash)) &&
      isKadProtocolDatagram(parsed.datagram)) {
    return true;
  }
  ed2k::KadContact contact;
  if (!attrs->kadRoutingTable->findByEndpoint(contact, endpoint) ||
      contact.id.size() != ed2k::HASH_LENGTH) {
    return false;
  }
  if (ed2k::parseKadObfuscatedDatagram(parsed, raw, contact.id) &&
      isKadProtocolDatagram(parsed.datagram)) {
    return true;
  }
  return contact.udpKey != 0 &&
         ed2k::parseKadObfuscatedDatagram(parsed, raw, contact.udpKey) &&
         isKadProtocolDatagram(parsed.datagram);
}

void Ed2kKadCommand::queueEd2kUdpPacket(const ed2k::Endpoint& endpoint,
                                        uint8_t opcode,
                                        const std::string& payload)
{
  outbox_.push_back(std::make_pair(
      endpoint, ed2k::createDatagram(ed2k::PROTO_EDONKEY, opcode, payload)));
}

void Ed2kKadCommand::queueEmuleUdpPacket(const ed2k::Endpoint& endpoint,
                                         uint8_t opcode,
                                         const std::string& payload)
{
  outbox_.push_back(std::make_pair(
      endpoint, ed2k::createDatagram(ed2k::PROTO_EMULE, opcode, payload)));
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

void Ed2kKadCommand::queueServerSourcePoll()
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (attrs->link.hash.empty() || attrs->servers.empty()) {
    return;
  }
  const auto now = nowSeconds();
  for (const auto& server : attrs->servers) {
    auto state = getEd2kServerState(attrs, server);
    if (!state || server.port > 65531 ||
        !ed2k::serverUdpSourceRequestDue(*state, attrs->link.size, now)) {
      continue;
    }
    const bool extGetSources2 =
        (state->udpFlags & ed2k::SRV_UDPFLG_EXT_GETSOURCES2) != 0;
    queueEd2kUdpPacket(
        serverUdpEndpoint(server),
        extGetSources2 ? ed2k::OP_GLOBGETSOURCES2
                       : ed2k::OP_GLOBGETSOURCES,
        ed2k::createGlobGetSourcesPayload(attrs->link.hash, attrs->link.size,
                                          extGetSources2));
    A2_LOG_DEBUG(fmt("Queued ED2K UDP source request to %s:%u.",
                     server.host.c_str(), server.port + 4));
    markEd2kServerUdpSourceRequestSent(attrs, server, now);
  }
  lastServerSourcePoll_ = now;
}

void Ed2kKadCommand::queueBootstrap()
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable || !attrs->kadRoutingTable->needBootstrap(nowSeconds())) {
    return;
  }
  size_t queued = 0;
  auto routerContacts = attrs->kadRoutingTable->getRouterContacts();
  for (const auto& contact : routerContacts) {
    const auto endpoint = toEndpoint(contact);
    queueKadContactPacket(contact, ed2k::KAD_BOOTSTRAP_REQ, std::string());
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.contact = contact;
    tx.purpose = ed2k::KadTransactionPurpose::BOOTSTRAP;
    tx.expectedOpcode = ed2k::KAD_BOOTSTRAP_RES;
    tx.sentTime = nowSeconds();
    attrs->kadTransactions.add(tx);
    ++queued;
  }
  for (const auto& endpoint : attrs->kadRoutingTable->getRouterNodes()) {
    const auto duplicate =
        std::find_if(routerContacts.begin(), routerContacts.end(),
                     [&](const ed2k::KadContact& contact) {
                       return contact.host == endpoint.host &&
                              contact.udpPort == endpoint.port;
                     }) != routerContacts.end();
    if (duplicate) {
      continue;
    }
    queuePacket(endpoint, ed2k::KAD_BOOTSTRAP_REQ, std::string());
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.purpose = ed2k::KadTransactionPurpose::BOOTSTRAP;
    tx.expectedOpcode = ed2k::KAD_BOOTSTRAP_RES;
    tx.sentTime = nowSeconds();
    attrs->kadTransactions.add(tx);
    ++queued;
  }
  if (queued != 0) {
    A2_LOG_INFO(fmt("Queued ED2K Kad bootstrap to %lu router node(s).",
                    static_cast<unsigned long>(queued)));
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
    queueKadContactPacket(
        contact, ed2k::KAD_REQ,
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
  const auto tcpPort = localEd2kTcpPort(e_);
  if (tcpPort == 0) {
    return;
  }
  const auto now = nowSeconds();
  if (attrs->lastKadFirewalledCheck != 0 &&
      now - attrs->lastKadFirewalledCheck < FIREWALLED_CHECK_INTERVAL) {
    return;
  }
  const auto kadClientId = ed2k::ed2kHashToKadId(attrs->clientHash);
  auto contacts = attrs->kadRoutingTable->findClosest(kadClientId, 8, true);
  if (contacts.empty()) {
    return;
  }
  attrs->lastKadFirewalledCheck = now;
  for (const auto& contact : contacts) {
    const auto endpoint = toEndpoint(contact);
    queueKadContactPacket(
        contact, ed2k::KAD_FIREWALLED_REQ,
        ed2k::createKadFirewalledRequestPayload(tcpPort, kadClientId, 0));
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
  const auto kadFileId = ed2k::ed2kHashToKadId(attrs->link.hash);
  const auto tcpPort = localEd2kTcpPort(e_);
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
  auto contacts = attrs->kadRoutingTable->findClosest(kadFileId, 8, true);
  if (contacts.empty()) {
    return;
  }
  ed2k::Endpoint source;
  source.host = *observed;
  source.port = tcpPort;
  const auto sourceId = ed2k::ed2kHashToKadId(attrs->clientHash);
  const auto payload = ed2k::createKadPublishSourceRequestPayload(
      kadFileId, source, sourceId, attrs->link.size);
  ed2k::KadPublishSourceRequest request;
  if (!ed2k::parseKadPublishSourceRequestPayload(request, payload)) {
    return;
  }
  attrs->kadSourceIndex.store(kadFileId, request.source);
  attrs->lastKadSourcePublish = now;
  for (const auto& contact : contacts) {
    queueKadContactPacket(contact, ed2k::KAD_PUBLISH_SOURCE_REQ, payload);
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
          traversal.kind() == ed2k::KadTraversalKind::KEYWORD_LOOKUP ||
                  traversal.kind() == ed2k::KadTraversalKind::SOURCE_LOOKUP
              ? ed2k::KAD_FIND_VALUE
              : ed2k::KAD_FIND_NODE;
      queueKadContactPacket(action.contact, ed2k::KAD_REQ,
                            ed2k::createKadRequestPayload(
                                searchType, traversal.targetId(),
                                action.contact.id));
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
      queueKadContactPacket(
          action.contact, ed2k::KAD_SEARCH_KEYS_REQ,
          ed2k::createKadSearchKeysRequestPayload(traversal.targetId(), 0));
    }
    else {
      queueKadContactPacket(action.contact, ed2k::KAD_SEARCH_SOURCES_REQ,
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
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  const auto now = nowSeconds();
  if (!shouldStartEd2kKadSourceSearch(attrs, now)) {
    return;
  }
  const auto kadFileId = ed2k::ed2kHashToKadId(attrs->link.hash);
  auto contacts = attrs->kadRoutingTable->findClosest(kadFileId, 8, true);
  if (contacts.empty()) {
    return;
  }
  attrs->kadSourceTraversal = make_unique<ed2k::KadTraversal>(
      ed2k::KadTraversalKind::SOURCE_LOOKUP, kadFileId,
      attrs->link.size);
  queueTraversalActions(*attrs->kadSourceTraversal,
                        attrs->kadSourceTraversal->start(contacts));
  sourceSearchSent_ = true;
  markEd2kKadSourceSearchStarted(attrs, now);
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

size_t Ed2kKadCommand::queueDuePeerReasks(int64_t now)
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  size_t queued = 0;
  while (auto peer = selectDueEd2kUdpReaskPeer(attrs, now)) {
    ed2k::Endpoint endpoint = peer->endpoint;
    endpoint.port = peer->udpPort;
    queueEmuleUdpPacket(endpoint, ed2k::OP_REASKFILEPING,
                        ed2k::createUdpReaskFilePingPayload(
                            attrs->link.hash));
    markEd2kPeerUdpReaskSent(attrs, peer->endpoint, now);
    ++queued;
  }
  return queued;
}

size_t Ed2kKadCommand::queueDueKadCallbacks(int64_t now)
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  const auto tcpPort = localEd2kTcpPort(e_);
  if (!attrs || tcpPort == 0 || attrs->link.hash.empty()) {
    return 0;
  }
  constexpr int64_t CALLBACK_TIMEOUT = 45;
  size_t queued = 0;
  for (auto& state : attrs->peerStates) {
    if (!state.lowId || !state.callbackRequested ||
        state.lowIdCallbackState != ed2k::LowIdCallbackState::REQUESTED ||
        state.lastCallbackTime != 0 || state.callbackBuddy.host.empty() ||
        state.callbackBuddy.port == 0 ||
        state.callbackBuddyId.size() != ed2k::HASH_LENGTH) {
      continue;
    }
    queuePacket(state.callbackBuddy, ed2k::KAD_CALLBACK_REQ,
                ed2k::createKadCallbackRequestPayload(
                    state.callbackBuddyId,
                    ed2k::ed2kHashToKadId(attrs->link.hash), tcpPort));
    state.lastCallbackTime = now;
    state.callbackDeadline = now + CALLBACK_TIMEOUT;
    ++queued;
    A2_LOG_DEBUG(fmt("Queued ED2K Kad callback request to buddy %s:%u "
                     "for source %s:%u.",
                     state.callbackBuddy.host.c_str(),
                     state.callbackBuddy.port, state.endpoint.host.c_str(),
                     state.endpoint.port));
  }
  return queued;
}

void Ed2kKadCommand::sendQueuedPackets()
{
  while (!outbox_.empty()) {
    auto item = outbox_.front();
    ed2k::PacketHeader header;
    if (ed2k::readDatagramHeader(header, item.second.data(),
                                  item.second.size()) &&
        isKnownEd2kUdpProtocol(header.protocol)) {
      A2_LOG_DEBUG(fmt(
          "Sending ED2K UDP packet to %s:%u protocol=0x%02x opcode=0x%02x payload=%lu.",
          item.first.host.c_str(), item.first.port, header.protocol,
          header.opcode, static_cast<unsigned long>(header.payloadSize())));
    }
    else {
      A2_LOG_DEBUG(fmt(
          "Sending obfuscated ED2K Kad UDP packet to %s:%u payload=%lu.",
          item.first.host.c_str(), item.first.port,
          static_cast<unsigned long>(item.second.size())));
    }
    const auto sent = socket_->writeData(item.second.data(), item.second.size(),
                                        item.first.host, item.first.port);
    if (sent < 0) {
      A2_LOG_DEBUG(fmt("Failed to send ED2K UDP packet to %s:%u.",
                       item.first.host.c_str(), item.first.port));
    }
    outbox_.pop_front();
  }
}

void Ed2kKadCommand::receivePackets()
{
  std::array<unsigned char, 64_k> data;
  while (true) {
    Endpoint sender;
    auto length = socket_->readDataFrom(data.data(), data.size(), sender);
    if (length <= 0) {
      break;
    }
    if (length < 2) {
      continue;
    }
    ed2k::Endpoint endpoint;
    endpoint.host = sender.addr;
    endpoint.port = sender.port;
    std::string raw(reinterpret_cast<const char*>(data.data()),
                    reinterpret_cast<const char*>(data.data()) + length);
    std::unique_ptr<ed2k::KadObfuscatedDatagram> obfuscatedContext;
    ed2k::KadObfuscatedDatagram parsed;
    if (tryDecodeKadObfuscatedDatagram(parsed, endpoint, raw)) {
      raw.swap(parsed.datagram);
      obfuscatedContext.reset(new ed2k::KadObfuscatedDatagram(parsed));
      length = raw.size();
      data.fill(0);
      std::copy(raw.begin(), raw.end(), data.begin());
      A2_LOG_DEBUG(
          fmt("Received obfuscated ED2K Kad UDP packet from %s:%u payload=%lu receiverKey=%u senderKey=%u.",
              endpoint.host.c_str(), endpoint.port,
              static_cast<unsigned long>(length),
              obfuscatedContext->receiverVerifyKey,
              obfuscatedContext->senderVerifyKey));
    }
    ed2k::PacketHeader header;
    if (!ed2k::readDatagramHeader(
            header, raw.data(), static_cast<size_t>(length)) ||
        (header.protocol != ed2k::KAD_PROTOCOL &&
         header.protocol != ed2k::PROTO_EDONKEY &&
         header.protocol != ed2k::PROTO_EMULE &&
         header.protocol != ed2k::PROTO_PACKED) ||
        header.payloadSize() + 2 != static_cast<size_t>(length)) {
      continue;
    }
    A2_LOG_DEBUG(fmt(
        "Received ED2K UDP packet from %s:%u protocol=0x%02x opcode=0x%02x payload=%lu.",
        sender.addr.c_str(), sender.port, header.protocol, header.opcode,
        static_cast<unsigned long>(header.payloadSize())));
    std::string payload(raw.data() + 2, raw.data() + length);
    if (header.protocol == ed2k::PROTO_PACKED) {
      std::string inflated;
      if (!ed2k::inflatePackedPacketPayload(inflated, payload, 64_k)) {
        continue;
      }
      header.protocol = ed2k::PROTO_EMULE;
      payload.swap(inflated);
    }
    if (header.protocol == ed2k::PROTO_EDONKEY ||
        header.protocol == ed2k::PROTO_EMULE) {
      handleEd2kUdpPacket(endpoint, header.opcode, payload);
    }
    else {
      handlePacket(endpoint, obfuscatedContext.get(), header.opcode, payload);
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
      markEd2kPeerUdpReaskAck(
          getEd2kAttrs(requestGroup_->getDownloadContext()), endpoint,
          ack.rank, ack.bitfield, nowSeconds());
    }
    return;
  }
  if (opcode == ed2k::OP_QUEUEFULL) {
    markEd2kPeerQueueFull(getEd2kAttrs(requestGroup_->getDownloadContext()),
                          endpoint, nowSeconds(), peerRetryWait(e_));
    return;
  }
  if (opcode == ed2k::OP_FILENOTFOUND) {
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
    uint16_t rank = 0;
    auto rgman = e_->getRequestGroupMan().get();
    auto uploadQueue = rgman ? rgman->getEd2kUploadQueue() : nullptr;
    if (uploadQueue) {
      rank = uploadQueue->queueRank(endpoint);
    }
    if (rank == 0 && (!uploadQueue || !uploadQueue->isUploading(endpoint))) {
      queueEmuleUdpPacket(endpoint, ed2k::OP_QUEUEFULL, std::string());
      return;
    }
    queueEmuleUdpPacket(endpoint, ed2k::OP_REASKACK,
                        ed2k::createUdpReaskAckPayload(rank));
    return;
  }
  if (opcode == ed2k::OP_GLOBFOUNDSOURCES) {
    auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
    std::vector<ed2k::FoundSource> sources;
    if (!ed2k::parsePackedFoundSourcesPayloads(sources, payload,
                                               attrs->link.hash)) {
      return;
    }
    const auto added =
        mergeEd2kServerSources(attrs, sources, ed2k::PEER_SOURCE_SERVER);
    if (endpoint.port >= 4) {
      ed2k::Endpoint server;
      server.host = endpoint.host;
      server.port = endpoint.port - 4;
      updateEd2kServerSourceResponse(attrs, server, sources.size(),
                                     nowSeconds());
    }
    if (added != 0) {
      A2_LOG_INFO(fmt("ED2K UDP server %s:%u returned %lu source(s).",
                      endpoint.host.c_str(), endpoint.port,
                      static_cast<unsigned long>(sources.size())));
      schedulePendingEd2kPeers(requestGroup_, e_);
    }
    return;
  }
  if (opcode == ed2k::OP_INVALID_LOWID) {
    if (payload.size() >= 4) {
      markEd2kCallbackFailed(getEd2kAttrs(requestGroup_->getDownloadContext()),
                             ed2k::readUInt32(payload.data()));
    }
    return;
  }
  if (opcode == ed2k::OP_GLOBCALLBACKREQ) {
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
  if (!ed2k::parseServerUdpStatusPayload(status, payload) ||
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
  handlePacket(endpoint, nullptr, opcode, payload);
}

void Ed2kKadCommand::handlePacket(
    const ed2k::Endpoint& endpoint,
    const ed2k::KadObfuscatedDatagram* context, uint8_t opcode,
    const std::string& payload)
{
  auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
  if (!attrs->kadRoutingTable) {
    return;
  }
  if (opcode == ed2k::KAD_BOOTSTRAP_REQ) {
    std::string requesterId;
    if (payload.size() >= ed2k::HASH_LENGTH + 3) {
      requesterId.assign(payload.begin(), payload.begin() + ed2k::HASH_LENGTH);
    }
    const auto kadClientId = ed2k::ed2kHashToKadId(attrs->clientHash);
    const auto contacts =
        requesterId.empty()
            ? attrs->kadRoutingTable->findClosest(kadClientId, 20, false)
            : attrs->kadRoutingTable->findClosestExcluding(
                  kadClientId, requesterId, 20, false);
    auto response = ed2k::createKadBootstrapResponsePayload(
        kadClientId, localEd2kTcpPort(e_), 8, contacts);
    if (context) {
      queueKadResponsePacket(endpoint, *context, ed2k::KAD_BOOTSTRAP_RES,
                             response);
    }
    else {
      queuePacket(endpoint, ed2k::KAD_BOOTSTRAP_RES, response);
    }
    return;
  }
  if (opcode == ed2k::KAD_BOOTSTRAP_RES) {
    ed2k::KadBootstrapResponse response;
    if (!ed2k::parseKadBootstrapResponsePayload(response, payload)) {
      return;
    }
    ed2k::KadTransaction tx;
    attrs->kadTransactions.complete(endpoint, opcode, tx);
    ed2k::KadContact sender;
    sender.id = response.id;
    sender.host = endpoint.host;
    sender.udpPort = endpoint.port;
    sender.tcpPort = response.tcpPort;
    sender.version = response.version;
    sender.udpKey = context ? context->senderVerifyKey : 0;
    attrs->kadRoutingTable->nodeSeen(sender, nowSeconds());
    for (const auto& contact : response.contacts) {
      attrs->kadRoutingTable->heardAbout(contact, nowSeconds());
      if (contact.host == endpoint.host && contact.udpPort == endpoint.port) {
        continue;
      }
      queueKadContactPacket(contact, ed2k::KAD_HELLO_REQ,
                            ed2k::createKadHelloPayload(
                                ed2k::ed2kHashToKadId(attrs->clientHash),
                                localEd2kTcpPort(e_), 8));
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
      contact.udpKey = context ? context->senderVerifyKey : 0;
      attrs->kadRoutingTable->nodeSeen(contact, nowSeconds());
    }
    if (opcode == ed2k::KAD_HELLO_REQ) {
      auto response = ed2k::createKadHelloPayload(
          ed2k::ed2kHashToKadId(attrs->clientHash), localEd2kTcpPort(e_), 8);
      if (context) {
        queueKadResponsePacket(endpoint, *context, ed2k::KAD_HELLO_RES,
                               response);
      }
      else {
        queuePacket(endpoint, ed2k::KAD_HELLO_RES, response);
      }
    }
    return;
  }
  if (opcode == ed2k::KAD_REQ) {
    ed2k::KadRequest request;
    if (!ed2k::parseKadRequestPayload(request, payload) ||
        request.receiverId != ed2k::ed2kHashToKadId(attrs->clientHash)) {
      return;
    }
    const auto searchType = request.searchType & 0x1f;
    if (searchType == 0) {
      return;
    }
    auto response = ed2k::createKadResponsePayload(
        request.targetId,
        attrs->kadRoutingTable->findClosest(request.targetId, 32, false));
    if (context) {
      queueKadResponsePacket(endpoint, *context, ed2k::KAD_RES, response);
    }
    else {
      queuePacket(endpoint, ed2k::KAD_RES, response);
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
      auto sources = ed2k::extractKadSourceEndpointDetails(result);
      A2_LOG_DEBUG(fmt("ED2K Kad search response from %s:%u target=%s "
                       "entries=%lu sources=%lu.",
                       endpoint.host.c_str(), endpoint.port,
                       util::toHex(result.targetId).c_str(),
                       static_cast<unsigned long>(result.entries.size()),
                       static_cast<unsigned long>(sources.size())));
      for (const auto& source : sources) {
        const bool added =
            addEd2kKadSourcePeer(attrs, source, ed2k::PEER_SOURCE_KAD);
        A2_LOG_DEBUG(fmt("ED2K Kad source type=%u host=%s tcp=%u udp=%u "
                         "crypt=%u usable=%s added=%s.",
                         source.sourceType, source.endpoint.host.c_str(),
                         source.endpoint.port, source.udpPort,
                         source.endpoint.cryptOptions,
                         directKadTcpSourceType(source.sourceType) ? "yes"
                                                                   : "no",
                         added ? "yes" : "no"));
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
    auto response = ed2k::createKadPublishResultPayload(request.fileId, 1);
    if (context) {
      queueKadResponsePacket(endpoint, *context, ed2k::KAD_PUBLISH_RES,
                             response);
    }
    else {
      queuePacket(endpoint, ed2k::KAD_PUBLISH_RES, response);
    }
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
      auto response = ed2k::createKadSearchResultPayload(
          ed2k::ed2kHashToKadId(attrs->clientHash), request.targetId,
          entries);
      if (context) {
        queueKadResponsePacket(endpoint, *context, ed2k::KAD_SEARCH_RES,
                               response);
      }
      else {
        queuePacket(endpoint, ed2k::KAD_SEARCH_RES, response);
      }
    }
    return;
  }
  if (opcode == ed2k::KAD_FIREWALLED_REQ) {
    ed2k::KadFirewalledRequest request;
    if (!ed2k::parseKadFirewalledRequestPayload(request, payload)) {
      return;
    }
    auto response = ed2k::createKadFirewalledResponsePayload(endpoint.host);
    if (context) {
      queueKadResponsePacket(endpoint, *context, ed2k::KAD_FIREWALLED_RES,
                             response);
    }
    else {
      queuePacket(endpoint, ed2k::KAD_FIREWALLED_RES, response);
    }
    return;
  }
  if (opcode == ed2k::KAD_PING) {
    if (context) {
      queueKadResponsePacket(endpoint, *context, ed2k::KAD_PONG,
                             std::string());
    }
    else {
      queuePacket(endpoint, ed2k::KAD_PONG, std::string());
    }
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
    queueServerSourcePoll();
    queueDuePeerReasks(nowSeconds());
    queueDueKadCallbacks(nowSeconds());
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
