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

#include <array>

#include "DlAbortEx.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Ed2kAttribute.h"
#include "LogFactory.h"
#include "Logger.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SimpleRandomizer.h"
#include "SocketCore.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_kad_search.h"
#include "ed2k_packet.h"
#include "ed2k_search.h"
#include "ed2k_server.h"
#include "fmt.h"
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
    tx.expectedOpcode = ed2k::KAD_BOOTSTRAP_RES;
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
  for (const auto& contact : contacts) {
    const auto endpoint = toEndpoint(contact);
    queuePacket(endpoint, ed2k::KAD_SEARCH_SOURCES_REQ,
                ed2k::createKadSearchSourcesRequestPayload(
                    attrs->link.hash, 0, attrs->link.size));
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.expectedOpcode = ed2k::KAD_SEARCH_RES;
    tx.targetId = attrs->link.hash;
    tx.sentTime = nowSeconds();
    attrs->kadTransactions.add(tx);
  }
  sourceSearchSent_ = !contacts.empty();
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
  for (const auto& contact : contacts) {
    const auto endpoint = toEndpoint(contact);
    queuePacket(endpoint, ed2k::KAD_SEARCH_KEYS_REQ,
                ed2k::createKadSearchKeysRequestPayload(targetId, 0));
    ed2k::KadTransaction tx;
    tx.endpoint = endpoint;
    tx.expectedOpcode = ed2k::KAD_SEARCH_RES;
    tx.targetId = targetId;
    tx.sentTime = nowSeconds();
    attrs->kadTransactions.add(tx);
  }
  keywordSearchSent_ = !contacts.empty();
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
         header.protocol != ed2k::PROTO_EDONKEY) ||
        header.payloadSize() + 6 != static_cast<size_t>(length)) {
      continue;
    }
    ed2k::Endpoint endpoint;
    endpoint.host = sender.addr;
    endpoint.port = sender.port;
    std::string payload(reinterpret_cast<const char*>(data.data()) + 6,
                        reinterpret_cast<const char*>(data.data()) + length);
    if (header.protocol == ed2k::PROTO_EDONKEY) {
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
  if (opcode == ed2k::KAD_SEARCH_RES) {
    ed2k::KadSearchResult result;
    if (ed2k::parseKadSearchResultPayload(result, payload)) {
      auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
      if (attrs->searchActive) {
        auto entries = ed2k::kadSearchEntriesToSearchResults(result.entries,
                                                            "kad");
        addEd2kSearchResults(attrs, entries, false);
        return;
      }
      auto peers = ed2k::extractKadSourceEndpoints(result);
      for (const auto& peer : peers) {
        addEd2kPeer(attrs, peer);
      }
      schedulePendingEd2kPeers(requestGroup_, e_);
    }
  }
}

bool Ed2kKadCommand::execute()
{
  if (requestGroup_->downloadFinished() || requestGroup_->isHaltRequested() ||
      e_->getRequestGroupMan()->downloadFinished() || e_->isHaltRequested()) {
    return true;
  }
  try {
    if (!initialized_) {
      init();
    }
    receivePackets();
    auto attrs = getEd2kAttrs(requestGroup_->getDownloadContext());
    attrs->kadTransactions.expire(nowSeconds(), 12);
    schedulePendingEd2kServers(requestGroup_, e_);
    queueServerStatusPoll();
    queueBootstrap();
    if (attrs->searchActive) {
      queueKeywordSearch();
    }
    else {
      queueSourceSearch();
    }
    sendQueuedPackets();
  }
  catch (DlAbortEx& e) {
    A2_LOG_INFO_EX("Exception thrown while handling ED2K Kad.", e);
  }
  e_->addRoutineCommand(std::unique_ptr<Command>(this));
  return false;
}

} // namespace aria2
