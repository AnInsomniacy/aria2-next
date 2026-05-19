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
#include "Ed2kCommand.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

#include "DlAbortEx.h"
#include "DlRetryEx.h"
#include "DiskAdaptor.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "DownloadFailureException.h"
#include "Ed2kAttribute.h"
#include "Ed2kPeerTransfer.h"
#include "Ed2kSharedResponder.h"
#include "Ed2kUploadQueue.h"
#include "FileEntry.h"
#include "LogFactory.h"
#include "Logger.h"
#include "message.h"
#include "Option.h"
#include "PeerStat.h"
#include "PieceStorage.h"
#include "Request.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "Segment.h"
#include "SegmentMan.h"
#include "SocketCore.h"
#include "ed2k_aich.h"
#include "ed2k_compression.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
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
std::shared_ptr<Request> makeEd2kRequest(const ed2k::Endpoint& endpoint,
                                         bool serverMode)
{
  auto req = std::make_shared<Request>();
  req->setUri(fmt("ed2k-peer://%s:%u/", endpoint.host.c_str(), endpoint.port));
  if (serverMode) {
    req->setUri(fmt("ed2k-server://%s:%u/", endpoint.host.c_str(),
                    endpoint.port));
  }
  return req;
}

std::string clientHash(const DownloadEngine* e)
{
  auto id = e->getSessionId();
  if (id.size() >= ed2k::HASH_LENGTH) {
    id = id.substr(0, ed2k::HASH_LENGTH);
  }
  else {
    id.append(ed2k::HASH_LENGTH - id.size(), '\0');
  }
  id[5] = 14;
  id[14] = 111;
  return id;
}

uint16_t localEd2kTcpPort(const DownloadEngine* e)
{
  auto port = e->getEd2kTcpPort();
  if (port != 0) {
    return port;
  }
  auto configured = e->getOption()->getAsInt(PREF_ED2K_LISTEN_PORT);
  if (configured > 0 &&
      configured <= static_cast<int>(std::numeric_limits<uint16_t>::max())) {
    return static_cast<uint16_t>(configured);
  }
  return 0;
}

std::string createPeerFileRequestPayload(const DownloadContext* dctx,
                                         PieceStorage* pieceStorage,
                                         const std::string& fileHash,
                                         uint8_t extendedRequestsVersion)
{
  auto payload = fileHash;
  if (extendedRequestsVersion == 0) {
    return payload;
  }

  const auto partCount = dctx->getNumPieces();
  if (partCount > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K file has too many parts.");
  }
  payload += ed2k::packUInt16(static_cast<uint16_t>(partCount));

  for (size_t index = 0; index < partCount;) {
    uint8_t bits = 0;
    for (size_t bit = 0; bit < 8 && index < partCount; ++bit, ++index) {
      if (pieceStorage && pieceStorage->hasPiece(index)) {
        bits |= 1u << bit;
      }
    }
    payload.push_back(static_cast<char>(bits));
  }

  if (extendedRequestsVersion > 1) {
    payload += ed2k::packUInt16(0);
  }
  return payload;
}

bool isFileRequestPayloadForHash(const std::string& payload,
                                 const std::string& expectedHash)
{
  return payload.size() >= ed2k::HASH_LENGTH &&
         payload.compare(0, ed2k::HASH_LENGTH, expectedHash) == 0;
}

int64_t nextServerSourceRequestTime()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             global::wallclock().getTime().time_since_epoch())
             .count() +
         60;
}

void storeAichRecoverySet(Ed2kAttribute* attrs,
                          const ed2k::AichRecoverySet& recoverySet)
{
  auto existing = std::find_if(
      attrs->aichRecoverySets.begin(), attrs->aichRecoverySets.end(),
      [&](const ed2k::AichRecoverySet& item) {
        return item.partIndex == recoverySet.partIndex;
      });
  if (existing == attrs->aichRecoverySets.end()) {
    attrs->aichRecoverySets.push_back(recoverySet);
  }
  else {
    *existing = recoverySet;
  }
}
} // namespace

Ed2kCommand::Ed2kCommand(cuid_t cuid, RequestGroup* requestGroup,
                         DownloadEngine* e, ed2k::Endpoint endpoint,
                         bool serverMode, bool countAsDownloadCommand)
    : AbstractCommand(cuid, makeEd2kRequest(endpoint, serverMode),
                      requestGroup->getDownloadContext()->getFirstFileEntry(),
                      requestGroup, e, nullptr, nullptr, true,
                      countAsDownloadCommand),
      mode_(serverMode ? Mode::SERVER : Mode::PEER),
      endpoint_(std::move(endpoint)),
      state_(State::INIT),
      connectedPort_(0),
      headerRead_(0),
      bodyRead_(0),
      peerFileStatusReceived_(false),
      peerFileRequestSent_(false),
      peerAccepted_(false),
      sourceExchangeRequested_(false),
      aichFileHashRequested_(false),
      incoming_(false),
      serverRequestSent_(false),
      use64BitOffsets_(requestGroup->getDownloadContext()->getTotalLength() >
                       std::numeric_limits<uint32_t>::max()),
      localPeerInfo_(ed2k::createLocalEmulePeerInfo())
{
  setTimeout(std::chrono::seconds(getOption()->getAsInt(PREF_CONNECT_TIMEOUT)));
  if (getSegmentMan()) {
    auto peerStat = getRequest()->initPeerStat();
    peerStat->downloadStart();
    getSegmentMan()->registerPeerStat(peerStat);
  }
  disableReadCheckSocket();
  disableWriteCheckSocket();
  if (mode_ == Mode::PEER) {
    markEd2kPeerConnecting(getEd2kAttrs(getDownloadContext()), endpoint_);
  }
}

Ed2kCommand::Ed2kCommand(cuid_t cuid, RequestGroup* requestGroup,
                         DownloadEngine* e, ed2k::Endpoint endpoint,
                         const std::shared_ptr<SocketCore>& socket)
    : AbstractCommand(cuid, makeEd2kRequest(endpoint, false),
                      requestGroup->getDownloadContext()->getFirstFileEntry(),
                      requestGroup, e, socket, nullptr, true, true),
      mode_(Mode::PEER),
      endpoint_(std::move(endpoint)),
      state_(State::READ_HEADER),
      connectedPort_(endpoint_.port),
      headerRead_(0),
      bodyRead_(0),
      peerFileStatusReceived_(false),
      peerFileRequestSent_(false),
      peerAccepted_(false),
      sourceExchangeRequested_(false),
      aichFileHashRequested_(false),
      serverRequestSent_(false),
      use64BitOffsets_(requestGroup->getDownloadContext()->getTotalLength() >
                       std::numeric_limits<uint32_t>::max()),
      incoming_(true),
      localPeerInfo_(ed2k::createLocalEmulePeerInfo())
{
  setTimeout(std::chrono::seconds(getOption()->getAsInt(PREF_CONNECT_TIMEOUT)));
  if (getSegmentMan()) {
    auto peerStat = getRequest()->initPeerStat();
    peerStat->downloadStart();
    getSegmentMan()->registerPeerStat(peerStat);
  }
  markEd2kPeerConnecting(getEd2kAttrs(getDownloadContext()), endpoint_);
}

Ed2kCommand::~Ed2kCommand()
{
  if (mode_ == Mode::PEER && getDownloadContext()) {
    markEd2kPeerDisconnected(getEd2kAttrs(getDownloadContext()), endpoint_);
  }
  if (mode_ == Mode::PEER && getDownloadEngine() &&
      getDownloadEngine()->getRequestGroupMan() &&
      getDownloadEngine()->getRequestGroupMan()->getEd2kUploadQueue()) {
    getDownloadEngine()->getRequestGroupMan()->getEd2kUploadQueue()->remove(
        endpoint_);
  }
}

bool Ed2kCommand::isExpectedServerEof() const
{
  if (mode_ != Mode::SERVER || !serverRequestSent_) {
    return false;
  }
  auto state = getEd2kServerState(getEd2kAttrs(getDownloadContext()),
                                  endpoint_);
  return state && state->handshakeCompleted;
}

bool Ed2kCommand::execute()
{
  try {
    if (getRequestGroup()->downloadFinished() ||
        getRequestGroup()->isHaltRequested()) {
      return true;
    }
    return executeInternal();
  }
  catch (DlAbortEx& err) {
    getRequestGroup()->setLastErrorCode(err.getErrorCode(), err.what());
    A2_LOG_ERROR_EX(EX_EXCEPTION_CAUGHT, err);
    return true;
  }
  catch (DlRetryEx& err) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         global::wallclock().getTime().time_since_epoch())
                         .count();
    const auto retryWait =
        std::max<int64_t>(1, getOption()->getAsInt(PREF_RETRY_WAIT));
    if (mode_ == Mode::SERVER) {
      if (isExpectedServerEof()) {
        disableReadCheckSocket();
        disableWriteCheckSocket();
        return true;
      }
      updateEd2kServerFailure(getEd2kAttrs(getDownloadContext()), endpoint_,
                              now, retryWait);
    }
    else {
      markEd2kPeerFailure(getEd2kAttrs(getDownloadContext()), endpoint_, now,
                          retryWait);
    }
    A2_LOG_INFO_EX(EX_EXCEPTION_CAUGHT, err);
    return true;
  }
  catch (DownloadFailureException& err) {
    getRequestGroup()->setLastErrorCode(err.getErrorCode(), err.what());
    getRequestGroup()->setHaltRequested(true);
    A2_LOG_ERROR_EX(EX_EXCEPTION_CAUGHT, err);
    return true;
  }
}

void Ed2kCommand::queuePacket(uint8_t protocol, uint8_t opcode,
                              const std::string& payload)
{
  A2_LOG_DEBUG(fmt("CUID#%" PRId64
                   " - Queue ED2K %s packet protocol=0x%02x opcode=0x%02x "
                   "payload=%lu.",
                   getCuid(), mode_ == Mode::SERVER ? "server" : "peer",
                   protocol, opcode,
                   static_cast<unsigned long>(payload.size())));
  outbox_.push_back(ed2k::createPacket(protocol, opcode, payload));
}

void Ed2kCommand::queueServerLogin()
{
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_LOGINREQUEST,
              ed2k::createLoginRequestPayload(clientHash(getDownloadEngine()),
                                              0,
                                              localEd2kTcpPort(getDownloadEngine()),
                                              "aria2-next"));
}

bool Ed2kCommand::queueGetSources()
{
  const auto attrs = getEd2kAttrs(getDownloadContext());
  const auto state = getEd2kServerState(attrs, endpoint_);
  if (attrs->link.size > std::numeric_limits<uint32_t>::max() &&
      state && (state->tcpFlags & ed2k::SRV_TCPFLG_LARGEFILES) == 0) {
    A2_LOG_INFO(fmt("CUID#%" PRId64
                    " - ED2K server %s:%u does not advertise large-file "
                    "source requests for %s.",
                    getCuid(), endpoint_.host.c_str(), endpoint_.port,
                    util::toHex(attrs->link.hash).c_str()));
    return false;
  }
  const bool requestObfuSources =
      state && (state->tcpFlags & ed2k::SRV_TCPFLG_TCPOBFUSCATION) != 0;
  queuePacket(ed2k::PROTO_EDONKEY,
              requestObfuSources ? ed2k::OP_GETSOURCES_OBFU
                                 : ed2k::OP_GETSOURCES,
              ed2k::createGetSourcesPayload(attrs->link.hash,
                                            attrs->link.size));
  serverRequestSent_ = true;
  return true;
}

void Ed2kCommand::queueSearchRequest()
{
  const auto attrs = getEd2kAttrs(getDownloadContext());
  if (!attrs->searchActive) {
    return;
  }
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_SEARCHREQUEST,
              ed2k::createSearchRequestPayload(
                  attrs->searchQuery,
                  attrs->link.size >
                      static_cast<int64_t>(std::numeric_limits<uint32_t>::max())));
  serverRequestSent_ = true;
}

void Ed2kCommand::queueCallbackRequest(uint32_t clientId)
{
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_CALLBACKREQUEST,
              ed2k::createCallbackRequestPayload(clientId));
}

uint32_t Ed2kCommand::localEd2kClientId() const
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  if (!attrs) {
    return 0;
  }
  for (const auto& state : attrs->serverStates) {
    if (state.handshakeCompleted && state.clientId != 0) {
      return state.clientId;
    }
  }
  return 0;
}

ed2k::Endpoint Ed2kCommand::localEd2kServerEndpoint() const
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  if (!attrs) {
    return ed2k::Endpoint();
  }
  for (const auto& state : attrs->serverStates) {
    if (state.handshakeCompleted && state.clientId != 0) {
      return state.endpoint;
    }
  }
  return ed2k::Endpoint();
}

void Ed2kCommand::queuePeerHello()
{
  auto payload = ed2k::createPeerHelloPayload(
      clientHash(getDownloadEngine()), localEd2kClientId(),
      localEd2kTcpPort(getDownloadEngine()), localEd2kServerEndpoint(),
      "aria2-next", localPeerInfo_, true);
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_HELLO, payload);
}

void Ed2kCommand::queuePeerHelloAnswer()
{
  auto payload = ed2k::createPeerHelloPayload(
      clientHash(getDownloadEngine()), localEd2kClientId(),
      localEd2kTcpPort(getDownloadEngine()), localEd2kServerEndpoint(),
      "aria2-next", localPeerInfo_, false);
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_HELLOANSWER, payload);
}

void Ed2kCommand::queueEmuleInfo(bool answer)
{
  queuePacket(ed2k::PROTO_EMULE,
              answer ? ed2k::OP_EMULEINFOANSWER : ed2k::OP_EMULEINFO,
              ed2k::createEmuleInfoPayload(localPeerInfo_));
}

void Ed2kCommand::queuePeerFileRequest()
{
  if (peerFileRequestSent_) {
    return;
  }
  peerFileRequestSent_ = true;
  auto attrs = getEd2kAttrs(getDownloadContext());
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_REQUESTFILENAME,
              createPeerFileRequestPayload(
                  getDownloadContext().get(), getPieceStorage().get(),
                  attrs->link.hash, localPeerInfo_.miscOptions
                                        .extendedRequestsVersion));
}

void Ed2kCommand::queuePeerFileStatusRequest()
{
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_SETREQFILEID,
              getEd2kAttrs(getDownloadContext())->link.hash);
}

void Ed2kCommand::queuePeerHashSetRequest()
{
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_HASHSETREQUEST,
              getEd2kAttrs(getDownloadContext())->link.hash);
}

void Ed2kCommand::queueAichFileHashRequest()
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  if (aichFileHashRequested_ || remotePeerInfo_.miscOptions.aichVersion == 0 ||
      !attrs->aichRootHash.empty()) {
    return;
  }
  aichFileHashRequested_ = true;
  queuePacket(ed2k::PROTO_EMULE, ed2k::OP_AICHFILEHASHREQ,
              ed2k::createAichFileHashRequestPayload(attrs->link.hash));
}

void Ed2kCommand::queueAichRecoveryRequest(size_t pieceIndex)
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  if (remotePeerInfo_.miscOptions.aichVersion == 0 ||
      attrs->aichRootHash.empty() ||
      pieceIndex > std::numeric_limits<uint16_t>::max()) {
    return;
  }
  queuePacket(ed2k::PROTO_EMULE, ed2k::OP_AICHREQUEST,
              ed2k::createAichRequestPayload(
                  attrs->link.hash, static_cast<uint16_t>(pieceIndex),
                  attrs->aichRootHash));
}

void Ed2kCommand::queueSourceExchangeRequest()
{
  if (sourceExchangeRequested_ ||
      (remotePeerInfo_.miscOptions.sourceExchange1Version == 0 &&
       !remotePeerInfo_.miscOptions2.supportsSourceExchange2)) {
    return;
  }
  sourceExchangeRequested_ = true;
  const auto request = ed2k::createRequestSourcesPayload(
      getEd2kAttrs(getDownloadContext())->link.hash, remotePeerInfo_);
  if (request.opcode != 0) {
    queuePacket(ed2k::PROTO_EMULE, request.opcode, request.payload);
  }
}

void Ed2kCommand::queueSourceExchangeAnswer(uint8_t version)
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  std::vector<ed2k::SourceExchangeEntry> entries;
  entries.reserve(std::min<size_t>(attrs->peers.size(), 500));
  for (const auto& peer : attrs->peers) {
    if (entries.size() >= 500) {
      break;
    }
    if (peer.host.empty() || peer.port == 0 ||
        (peer.host == endpoint_.host && peer.port == endpoint_.port)) {
      continue;
    }
    ed2k::SourceExchangeEntry entry;
    entry.endpoint = peer;
    entry.server.host = "0.0.0.0";
    entry.server.port = 0;
    entry.userHash = peer.userHash.empty()
                         ? std::string(ed2k::HASH_LENGTH, '\0')
                         : peer.userHash;
    entry.cryptOptions = static_cast<uint8_t>(peer.cryptOptions & 0xff);
    entries.push_back(std::move(entry));
  }
  if (entries.empty()) {
    return;
  }
  if (version >= 2) {
    queuePacket(ed2k::PROTO_EMULE, ed2k::OP_ANSWERSOURCES2,
                ed2k::createAnswerSources2Payload(attrs->link.hash, version,
                                                  entries));
  }
  else {
    queuePacket(ed2k::PROTO_EMULE, ed2k::OP_ANSWERSOURCES,
                ed2k::createAnswerSourcesPayload(attrs->link.hash, version,
                                                 entries));
  }
}

void Ed2kCommand::queuePeerStartUpload()
{
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_STARTUPLOADREQ,
              getEd2kAttrs(getDownloadContext())->link.hash);
}

void Ed2kCommand::queuePeerPartRequest()
{
  if (getRequestGroup()->downloadFinished()) {
    return;
  }
  std::vector<ed2k::PartRange> ranges;
  const auto attrs = getEd2kAttrs(getDownloadContext());
  auto state = getEd2kPeerState(attrs, endpoint_);
  const auto segments = ed2k::selectRequestSegments(
      getSegmentMan().get(), getCuid(),
      state ? state->partStatus : std::vector<bool>(), 3);
  for (const auto& segment : segments) {
    if (ranges.size() >= 3) {
      break;
    }
    if (!segment || segment->complete()) {
      continue;
    }
    const int64_t begin = segment->getPositionToWrite();
    const int64_t end =
        std::min(begin + static_cast<int64_t>(ed2k::BLOCK_LENGTH),
                 segment->getPosition() + segment->getLength());
    if (end <= begin) {
      continue;
    }
    ed2k::PartRange range;
    range.begin = begin;
    range.end = end;
    ranges.push_back(range);
  }
  if (ranges.empty()) {
    return;
  }
  updateEd2kPeerRequestedParts(attrs, endpoint_, ranges);
  queuePacket(ed2k::PROTO_EDONKEY,
              use64BitOffsets_ ? ed2k::OP_REQUESTPARTS_I64
                               : ed2k::OP_REQUESTPARTS,
              ed2k::createRequestPartsPayload(attrs->link.hash, ranges,
                                              use64BitOffsets_));
}

ed2k::SharedResponder Ed2kCommand::createSharedResponder()
{
  auto rgman = getDownloadEngine()->getRequestGroupMan().get();
  auto store = rgman ? rgman->getEd2kSharedStore() : nullptr;
  auto uploadQueue = rgman ? rgman->getEd2kUploadQueue() : nullptr;
  return ed2k::SharedResponder(store, uploadQueue, rgman, endpoint_,
                               remotePeerInfo_.userHash, outbox_);
}

bool Ed2kCommand::updatePeerEndpointFromHello(bool helloPacket)
{
  const auto minimumSize = ed2k::HASH_LENGTH + 6 + (helloPacket ? 1 : 0);
  if (!incoming_ || body_.size() < minimumSize) {
    return true;
  }
  size_t offset = helloPacket ? 1 : 0;
  const auto userHash = body_.substr(offset, ed2k::HASH_LENGTH);
  offset += ed2k::HASH_LENGTH + 4;
  const auto listenPort = ed2k::readUInt16(body_.data() + offset);
  if (listenPort == 0 || listenPort == endpoint_.port) {
    auto state = getEd2kPeerState(getEd2kAttrs(getDownloadContext()), endpoint_);
    if (state && userHash.size() == ed2k::HASH_LENGTH) {
      state->endpoint.userHash = userHash;
    }
    return true;
  }

  auto attrs = getEd2kAttrs(getDownloadContext());
  auto oldEndpoint = endpoint_;
  ed2k::Endpoint newEndpoint = endpoint_;
  newEndpoint.port = listenPort;
  newEndpoint.userHash = userHash;
  auto findState = [&](const ed2k::Endpoint& peer) -> ed2k::PeerState* {
    auto i = std::find_if(attrs->peerStates.begin(), attrs->peerStates.end(),
                          [&](const ed2k::PeerState& state) {
                            return state.endpoint.host == peer.host &&
                                   state.endpoint.port == peer.port;
                          });
    return i == attrs->peerStates.end() ? nullptr : &*i;
  };
  auto oldState = findState(oldEndpoint);
  auto existingState = findState(newEndpoint);
  if (existingState && existingState != oldState &&
      (existingState->connecting || existingState->accepted)) {
    if (oldState) {
      oldState->connecting = false;
      oldState->accepted = false;
      oldState->requestedParts.clear();
    }
    state_ = State::DONE;
    return false;
  }
  endpoint_ = newEndpoint;
  addEd2kPeer(attrs, endpoint_);
  auto newState = getEd2kPeerState(attrs, endpoint_);
  if (newState) {
    newState->connecting = true;
    newState->dead = false;
    newState->endpoint.userHash = userHash;
  }
  oldState = findState(oldEndpoint);
  if (oldState && oldState != newState) {
    oldState->connecting = false;
    oldState->accepted = false;
    oldState->requestedParts.clear();
  }
  return true;
}

void Ed2kCommand::startResolve()
{
  auto ipaddr = resolveHostname(resolvedAddresses_, endpoint_.host,
                                endpoint_.port);
  if (ipaddr.empty()) {
    state_ = State::RESOLVING;
    addCommandSelf();
    return;
  }
  connectedHostname_ = endpoint_.host;
  connectedAddr_ = ipaddr;
  connectedPort_ = endpoint_.port;
  startConnect();
}

void Ed2kCommand::startConnect()
{
  A2_LOG_INFO(fmt("CUID#%" PRId64 " - Connecting to ED2K %s %s:%u.",
                  getCuid(), mode_ == Mode::SERVER ? "server" : "peer",
                  connectedAddr_.c_str(), connectedPort_));
  createSocket();
  getSocket()->establishConnection(connectedAddr_, connectedPort_);
  setWriteCheckSocket(getSocket());
  state_ = State::CONNECTING;
  addCommandSelf();
}

bool Ed2kCommand::flushOutbox()
{
  while (!outbox_.empty()) {
    auto& data = outbox_.front();
    auto written = getSocket()->writeData(data.data(), data.size());
    if (written == 0) {
      setWriteCheckSocketIf(getSocket(), getSocket()->wantWrite());
      setReadCheckSocketIf(getSocket(), getSocket()->wantRead());
      addCommandSelf();
      return false;
    }
    data.erase(0, static_cast<size_t>(written));
    if (!data.empty()) {
      setWriteCheckSocket(getSocket());
      addCommandSelf();
      return false;
    }
    outbox_.pop_front();
  }
  disableWriteCheckSocket();
  setReadCheckSocket(getSocket());
  state_ = State::READ_HEADER;
  return true;
}

bool Ed2kCommand::readHeader()
{
  while (headerRead_ < headerBuf_.size()) {
    size_t len = headerBuf_.size() - headerRead_;
    getSocket()->readData(headerBuf_.data() + headerRead_, len);
    if (len == 0) {
      if (!getSocket()->wantRead() && !getSocket()->wantWrite()) {
        throw DL_RETRY_EX("ED2K connection closed.");
      }
      setReadCheckSocketIf(getSocket(), getSocket()->wantRead());
      setWriteCheckSocketIf(getSocket(), getSocket()->wantWrite());
      addCommandSelf();
      return false;
    }
    headerRead_ += len;
  }
  if (!ed2k::readPacketHeader(currentHeader_, headerBuf_.data(),
                              headerBuf_.size())) {
    throw DL_RETRY_EX("Bad ED2K packet header.");
  }
  if (currentHeader_.protocol != ed2k::PROTO_EDONKEY &&
      currentHeader_.protocol != ed2k::PROTO_PACKED &&
      currentHeader_.protocol != ed2k::PROTO_EMULE) {
    throw DL_RETRY_EX(
        fmt("Unsupported ED2K packet protocol 0x%02x.",
            currentHeader_.protocol));
  }
  if (currentHeader_.payloadSize() > 8_m) {
    throw DL_RETRY_EX("ED2K packet is too large.");
  }
  A2_LOG_DEBUG(fmt("CUID#%" PRId64
                   " - Read ED2K %s packet protocol=0x%02x opcode=0x%02x "
                   "payload=%lu.",
                   getCuid(), mode_ == Mode::SERVER ? "server" : "peer",
                   currentHeader_.protocol, currentHeader_.opcode,
                   static_cast<unsigned long>(currentHeader_.payloadSize())));
  body_.assign(currentHeader_.payloadSize(), '\0');
  bodyRead_ = 0;
  headerRead_ = 0;
  state_ = State::READ_BODY;
  return true;
}

bool Ed2kCommand::readBody()
{
  while (bodyRead_ < body_.size()) {
    size_t len = body_.size() - bodyRead_;
    getSocket()->readData(&body_[bodyRead_], len);
    if (len == 0) {
      if (!getSocket()->wantRead() && !getSocket()->wantWrite()) {
        throw DL_RETRY_EX("ED2K connection closed.");
      }
      setReadCheckSocketIf(getSocket(), getSocket()->wantRead());
      setWriteCheckSocketIf(getSocket(), getSocket()->wantWrite());
      addCommandSelf();
      return false;
    }
    bodyRead_ += len;
  }
  if (currentHeader_.protocol == ed2k::PROTO_PACKED) {
    std::string inflated;
    if (!ed2k::inflatePackedPacketPayload(inflated, body_, 250_k)) {
      throw DL_RETRY_EX("Bad packed ED2K packet.");
    }
    A2_LOG_DEBUG(fmt("CUID#%" PRId64
                     " - Unpacked ED2K packet opcode=0x%02x, "
                     "compressed=%lu, inflated=%lu.",
                     getCuid(), currentHeader_.opcode, body_.size(),
                     inflated.size()));
    body_.swap(inflated);
    currentHeader_.protocol =
        mode_ == Mode::SERVER ? ed2k::PROTO_EDONKEY : ed2k::PROTO_EMULE;
    currentHeader_.size = static_cast<uint32_t>(body_.size() + 1);
  }
  handlePacket();
  body_.clear();
  bodyRead_ = 0;
  if (state_ == State::READ_BODY) {
    state_ = State::READ_HEADER;
  }
  return true;
}

void Ed2kCommand::addPeer(const ed2k::Endpoint& peer)
{
  addEd2kPeer(getEd2kAttrs(getDownloadContext()), peer,
              ed2k::PEER_SOURCE_SERVER);
}

void Ed2kCommand::addPeers(const std::vector<ed2k::Endpoint>& peers)
{
  for (const auto& peer : peers) {
    addPeer(peer);
  }
}

void Ed2kCommand::schedulePendingPeers()
{
  schedulePendingEd2kPeers(getRequestGroup(), getDownloadEngine());
}

void Ed2kCommand::handlePartData(int64_t begin, const std::string& data)
{
  ed2k::PeerTransfer transfer(getDownloadContext().get(), getPieceStorage().get(),
                              getSegmentMan().get(), getCuid());
  auto completedSegment = transfer.writePartData(begin, data);
  if (!completedSegment) {
    return;
  }
  if (transfer.completeVerifiedSegment(completedSegment->getIndex())) {
    clearEd2kPeerRequestedParts(getEd2kAttrs(getDownloadContext()), endpoint_);
  }
}

void Ed2kCommand::handleServerPacket()
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  if (currentHeader_.opcode == ed2k::OP_IDCHANGE) {
    ed2k::ServerIdChange idChange;
    if (!ed2k::parseServerIdChangePayload(idChange, body_)) {
      throw DL_RETRY_EX("Bad ED2K server ID change.");
    }
    updateEd2kServerIdChange(attrs, endpoint_, idChange);
    A2_LOG_INFO(fmt("CUID#%" PRId64
                    " - ED2K server %s:%u assigned %s ID 0x%08x.",
                    getCuid(), endpoint_.host.c_str(), endpoint_.port,
                    idChange.highId ? "High" : "Low", idChange.clientId));
    if (attrs->searchActive) {
      queueSearchRequest();
    }
    else {
      A2_LOG_INFO(fmt("CUID#%" PRId64
                      " - ED2K server %s:%u requesting sources for %s.",
                      getCuid(), endpoint_.host.c_str(), endpoint_.port,
                      util::toHex(attrs->link.hash).c_str()));
      if (!queueGetSources()) {
        updateEd2kServerSourceRequestTime(attrs, endpoint_,
                                          nextServerSourceRequestTime());
        markEd2kServerSourceRequestFinished(attrs, endpoint_);
        state_ = State::DONE;
        return;
      }
      updateEd2kServerSourceRequestTime(attrs, endpoint_,
                                        nextServerSourceRequestTime());
    }
    state_ = State::WRITE;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_FOUNDSOURCES ||
      currentHeader_.opcode == ed2k::OP_FOUNDSOURCES_OBFU) {
    std::vector<ed2k::FoundSource> sources;
    if (!ed2k::parseFoundSourcesPayload(
            sources, body_, attrs->link.hash,
            currentHeader_.opcode == ed2k::OP_FOUNDSOURCES_OBFU)) {
      A2_LOG_INFO(fmt("CUID#%" PRId64
                      " - ED2K server %s:%u returned unusable sources.",
                      getCuid(), endpoint_.host.c_str(), endpoint_.port));
      markEd2kServerSourceRequestFinished(attrs, endpoint_);
      state_ = State::DONE;
      return;
    }
    auto serverState = getEd2kServerState(attrs, endpoint_);
    const bool canRequestCallback =
        serverState && serverState->handshakeCompleted && serverState->highId;
    for (const auto& source : sources) {
      if (source.lowId) {
        if (canRequestCallback) {
          addEd2kFoundSource(attrs, source, ed2k::PEER_SOURCE_SERVER, true);
          queueCallbackRequest(source.clientId);
        }
        else {
          addEd2kFoundSource(attrs, source, ed2k::PEER_SOURCE_SERVER, false);
        }
      }
    }
    mergeEd2kServerSources(attrs, sources, ed2k::PEER_SOURCE_SERVER);
    A2_LOG_INFO(fmt("CUID#%" PRId64
                    " - ED2K server %s:%u returned %lu source(s).",
                    getCuid(), endpoint_.host.c_str(), endpoint_.port,
                    static_cast<unsigned long>(sources.size())));
    schedulePendingPeers();
    markEd2kServerSourceRequestFinished(attrs, endpoint_);
    state_ = outbox_.empty() ? State::DONE : State::WRITE;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_CALLBACKREQUESTED) {
    ed2k::Endpoint peer;
    if (!ed2k::parseCallbackRequestIncomingPayload(peer, body_)) {
      throw DL_RETRY_EX("Bad ED2K callback request.");
    }
    if ((peer.cryptOptions & ed2k::SOURCE_CRYPT_REQUIRE) == 0) {
      addEd2kPeer(attrs, peer, ed2k::PEER_SOURCE_SERVER);
      auto state = getEd2kPeerState(attrs, peer);
      if (state) {
        state->lowId = false;
        state->callbackRequested = false;
        state->callbackImpossible = false;
      }
    }
    schedulePendingPeers();
    state_ = State::DONE;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_CALLBACK_FAIL) {
    if (body_.size() >= 4) {
      markEd2kCallbackFailed(attrs, ed2k::readUInt32(body_.data()));
    }
    A2_LOG_INFO(fmt("CUID#%" PRId64
                    " - ED2K server %s:%u reported callback failure.",
                    getCuid(), endpoint_.host.c_str(), endpoint_.port));
    state_ = State::READ_HEADER;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_REJECT) {
    A2_LOG_INFO(fmt("CUID#%" PRId64
                    " - ED2K server %s:%u rejected the last command.",
                    getCuid(), endpoint_.host.c_str(), endpoint_.port));
    state_ = State::READ_HEADER;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_SEARCHRESULT) {
    ed2k::SearchResult result;
    if (!ed2k::parseSearchResultPayload(result, body_, "server")) {
      A2_LOG_INFO(fmt("CUID#%" PRId64
                      " - ED2K server %s:%u returned an unusable search result.",
                      getCuid(), endpoint_.host.c_str(), endpoint_.port));
      state_ = State::DONE;
      return;
    }
    addEd2kSearchResults(getEd2kAttrs(getDownloadContext()), result.entries,
                         result.moreResults);
    state_ = State::DONE;
  }
  if (currentHeader_.opcode == ed2k::OP_SERVERSTATUS) {
    ed2k::ServerStatus status;
    if (!ed2k::parseServerStatusPayload(status, body_)) {
      throw DL_RETRY_EX("Bad ED2K server status.");
    }
    updateEd2kServerStatus(attrs, endpoint_, status);
  }
  if (currentHeader_.opcode == ed2k::OP_SERVERMESSAGE) {
    std::string message;
    if (!ed2k::parseServerMessagePayload(message, body_)) {
      throw DL_RETRY_EX("Bad ED2K server message.");
    }
    A2_LOG_INFO(fmt("CUID#%" PRId64 " - ED2K server %s:%u message: %s",
                    getCuid(), endpoint_.host.c_str(), endpoint_.port,
                    message.c_str()));
    updateEd2kServerMessage(attrs, endpoint_, message);
  }
  if (currentHeader_.opcode == ed2k::OP_SERVERIDENT) {
    ed2k::ServerIdent ident;
    if (!ed2k::parseServerIdentPayload(ident, body_)) {
      throw DL_RETRY_EX("Bad ED2K server ident.");
    }
    updateEd2kServerIdent(attrs, endpoint_, ident);
  }
  if (currentHeader_.opcode == ed2k::OP_SERVERLIST) {
    std::vector<ed2k::Endpoint> servers;
    if (!ed2k::parseServerListPayload(servers, body_)) {
      throw DL_RETRY_EX("Bad ED2K server list.");
    }
    for (const auto& server : servers) {
      auto before = attrs->servers.size();
      auto i = std::find_if(attrs->servers.begin(), attrs->servers.end(),
                            [&](const ed2k::Endpoint& item) {
                              return item.host == server.host &&
                                     item.port == server.port;
                            });
      if (i == attrs->servers.end()) {
        attrs->servers.push_back(server);
      }
      if (attrs->servers.size() != before) {
        getEd2kServerState(attrs, server);
      }
    }
  }
}

void Ed2kCommand::handlePeerPacket()
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  if (currentHeader_.protocol == ed2k::PROTO_EMULE) {
    switch (currentHeader_.opcode) {
    case ed2k::OP_EMULEINFO:
      if (ed2k::parseEmuleInfoPayload(remotePeerInfo_, body_)) {
        queueEmuleInfo(true);
        state_ = State::WRITE;
      }
      break;
    case ed2k::OP_EMULEINFOANSWER:
      if (!ed2k::parseEmuleInfoPayload(remotePeerInfo_, body_)) {
        throw DL_RETRY_EX("Bad eMule info answer.");
      }
      queueAichFileHashRequest();
      if (!outbox_.empty()) {
        state_ = State::WRITE;
      }
      break;
    case ed2k::OP_ANSWERSOURCES2: {
      ed2k::SourceExchangeAnswer answer;
      if (!ed2k::parseAnswerSources2Payload(answer, body_, attrs->link.hash)) {
        throw DL_RETRY_EX("Bad ED2K source exchange answer.");
      }
      mergeEd2kSourceExchangePeers(attrs, answer.entries, endpoint_);
      schedulePendingPeers();
      break;
    }
    case ed2k::OP_ANSWERSOURCES: {
      ed2k::SourceExchangeAnswer answer;
      const auto version = remotePeerInfo_.miscOptions.sourceExchange1Version;
      if (!ed2k::parseAnswerSourcesPayload(answer, body_, attrs->link.hash,
                                           version)) {
        throw DL_RETRY_EX("Bad ED2K source exchange answer.");
      }
      mergeEd2kSourceExchangePeers(attrs, answer.entries, endpoint_);
      schedulePendingPeers();
      break;
    }
    case ed2k::OP_REQUESTSOURCES:
      if (body_ == attrs->link.hash) {
        queueSourceExchangeAnswer(
            std::max<uint8_t>(1, remotePeerInfo_.miscOptions.sourceExchange1Version));
        if (!outbox_.empty()) {
          state_ = State::WRITE;
        }
      }
      else if (createSharedResponder().queueSourceExchangeAnswer(
            body_,
            std::max<uint8_t>(1, remotePeerInfo_.miscOptions.sourceExchange1Version))) {
        if (!outbox_.empty()) {
          state_ = State::WRITE;
        }
      }
      break;
    case ed2k::OP_REQUESTSOURCES2: {
      uint8_t version = 0;
      if (ed2k::parseRequestSources2Payload(version, body_,
                                            attrs->link.hash)) {
        queueSourceExchangeAnswer(version);
        if (!outbox_.empty()) {
          state_ = State::WRITE;
        }
      }
      else if (body_.size() >= ed2k::HASH_LENGTH + 3 &&
               ed2k::parseRequestSources2Payload(
                   version, body_, body_.substr(3, ed2k::HASH_LENGTH)) &&
               createSharedResponder().queueSourceExchangeAnswer(
                   body_.substr(3, ed2k::HASH_LENGTH), version)) {
        if (!outbox_.empty()) {
          state_ = State::WRITE;
        }
      }
      else {
        throw DL_RETRY_EX("Bad ED2K source exchange request.");
      }
      break;
    }
    case ed2k::OP_AICHFILEHASHREQ:
      if (body_.size() != ed2k::HASH_LENGTH ||
          !createSharedResponder().queueAichFileHashAnswer(body_)) {
        break;
      }
      state_ = State::WRITE;
      break;
    case ed2k::OP_AICHFILEHASHANS: {
      ed2k::AichFileHashAnswer answer;
      if (!ed2k::parseAichFileHashAnswerPayload(answer, body_,
                                                attrs->link.hash)) {
        throw DL_RETRY_EX("Bad ED2K AICH file hash answer.");
      }
      if (attrs->aichRootHash.empty()) {
        attrs->aichRootHash = answer.rootHash;
      }
      break;
    }
    case ed2k::OP_AICHANSWER: {
      ed2k::AichAnswer answer;
      if (!ed2k::parseAichAnswerPayload(answer, body_, attrs->link.hash)) {
        throw DL_RETRY_EX("Bad ED2K AICH answer.");
      }
      if (!answer.failed) {
        if (answer.rootHash != attrs->aichRootHash) {
          throw DL_RETRY_EX("Bad ED2K AICH recovery root.");
        }
        const auto partSize =
            std::min<int64_t>(ed2k::PIECE_LENGTH,
                              getDownloadContext()->getTotalLength() -
                                  static_cast<int64_t>(answer.partIndex) *
                                      ed2k::PIECE_LENGTH);
        ed2k::AichRecoveryData recovery;
        ed2k::AichRecoverySet recoverySet;
        if (partSize <= 0 ||
            !ed2k::parseAichRecoveryData(
                recovery, answer.recoveryData, static_cast<size_t>(partSize),
                use64BitOffsets_) ||
            !ed2k::buildAichRecoverySet(
                recoverySet, recovery, attrs->aichRootHash,
                static_cast<size_t>(getDownloadContext()->getTotalLength()),
                answer.partIndex)) {
          throw DL_RETRY_EX("Bad ED2K AICH recovery data.");
        }
        storeAichRecoverySet(attrs, recoverySet);
      }
      break;
    }
    case ed2k::OP_AICHREQUEST:
      if (body_.size() >= ed2k::HASH_LENGTH) {
        createSharedResponder().queueAichAnswer(
            body_.substr(0, ed2k::HASH_LENGTH), body_);
        if (!outbox_.empty()) {
          state_ = State::WRITE;
        }
      }
      break;
    default:
      break;
    }
    return;
  }
  switch (currentHeader_.opcode) {
  case ed2k::OP_HELLO:
    if (!updatePeerEndpointFromHello(true)) {
      break;
    }
    ed2k::parsePeerHelloPayload(remotePeerInfo_, body_, true);
    queuePeerHelloAnswer();
    queueEmuleInfo(true);
    queuePeerFileRequest();
    state_ = State::WRITE;
    break;
  case ed2k::OP_HELLOANSWER:
    if (!updatePeerEndpointFromHello(false)) {
      break;
    }
    ed2k::parsePeerHelloPayload(remotePeerInfo_, body_, false);
    queueEmuleInfo(false);
    queuePeerFileRequest();
    state_ = State::WRITE;
    break;
  case ed2k::OP_REQFILENAMEANSWER:
    if (body_.size() < ed2k::HASH_LENGTH ||
        body_.substr(0, ed2k::HASH_LENGTH) != attrs->link.hash) {
      throw DL_RETRY_EX("ED2K file answer hash mismatch.");
    }
    if (!peerFileStatusReceived_ &&
        getDownloadContext()->getTotalLength() > ed2k::PIECE_LENGTH) {
      queuePeerFileStatusRequest();
      state_ = State::WRITE;
    }
    else if (ed2k::hashSetPartCount(getDownloadContext()->getTotalLength()) >
                 0 &&
             attrs->pieceHashes.empty()) {
      queuePeerHashSetRequest();
      state_ = State::WRITE;
    }
    else {
      queueSourceExchangeRequest();
      queuePeerStartUpload();
      state_ = State::WRITE;
    }
    break;
  case ed2k::OP_REQUESTFILENAME:
    if (body_.size() < ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K file request.");
    }
    createSharedResponder().queueFileNameAnswer(
        body_.substr(0, ed2k::HASH_LENGTH));
    state_ = State::WRITE;
    break;
  case ed2k::OP_SETREQFILEID:
    if (body_.size() != ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K file status request.");
    }
    createSharedResponder().queueFileStatusAnswer(body_);
    state_ = State::WRITE;
    break;
  case ed2k::OP_FILESTATUS: {
    std::vector<bool> bitfield;
    if (!ed2k::parseFileStatusPayload(bitfield, body_, attrs->link.hash)) {
      throw DL_RETRY_EX("ED2K file status hash mismatch.");
    }
    updateEd2kPeerPartStatus(attrs, endpoint_, bitfield);
    peerFileStatusReceived_ = true;
    if (ed2k::hashSetPartCount(getDownloadContext()->getTotalLength()) > 0 &&
        attrs->pieceHashes.empty()) {
      queuePeerHashSetRequest();
    }
    else {
      queueSourceExchangeRequest();
      queuePeerStartUpload();
    }
    state_ = State::WRITE;
    break;
  }
  case ed2k::OP_HASHSETANSWER: {
    std::vector<std::string> pieceHashes;
    if (!ed2k::parseHashSetAnswerPayload(pieceHashes, body_,
                                         attrs->link.hash) ||
        pieceHashes.size() !=
            ed2k::hashSetPartCount(getDownloadContext()->getTotalLength()) ||
        ed2k::rootHash(pieceHashes) != attrs->link.hash) {
      throw DOWNLOAD_FAILURE_EXCEPTION2("Bad ED2K hash set.",
                                        error_code::CHECKSUM_ERROR);
    }
    attrs->pieceHashes = std::move(pieceHashes);
    queueSourceExchangeRequest();
    queuePeerStartUpload();
    state_ = State::WRITE;
    break;
  }
  case ed2k::OP_HASHSETREQUEST:
    if (body_.size() != ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K hash set request.");
    }
    createSharedResponder().queueHashSetAnswer(body_);
    state_ = State::WRITE;
    break;
  case ed2k::OP_STARTUPLOADREQ:
    if (body_.size() != ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K upload request.");
    }
    createSharedResponder().requestUploadSlot(
        body_, std::chrono::duration_cast<std::chrono::seconds>(
                   global::wallclock().getTime().time_since_epoch())
                   .count());
    state_ = State::WRITE;
    break;
  case ed2k::OP_ACCEPTUPLOADREQ:
    peerAccepted_ = true;
    markEd2kPeerAccepted(attrs, endpoint_);
    queuePeerPartRequest();
    state_ = State::WRITE;
    break;
  case ed2k::OP_OUTOFPARTREQS:
    markEd2kPeerOutOfParts(attrs, endpoint_);
    state_ = State::DONE;
    break;
  case ed2k::OP_FILEREQANSNOFIL:
    markEd2kPeerDead(
        attrs, endpoint_,
        std::chrono::duration_cast<std::chrono::seconds>(
            global::wallclock().getTime().time_since_epoch())
            .count(),
        std::max<int64_t>(1, getOption()->getAsInt(PREF_RETRY_WAIT)));
    state_ = State::DONE;
    break;
  case ed2k::OP_CANCELTRANSFER:
    markEd2kPeerCancelled(attrs, endpoint_);
    state_ = State::DONE;
    break;
  case ed2k::OP_QUEUERANK:
  case ed2k::OP_QUEUERANKING: {
    uint16_t rank = 0;
    if (!ed2k::parseQueueRankPayload(rank, body_)) {
      throw DL_RETRY_EX("Bad ED2K queue rank.");
    }
    auto peerState = getEd2kPeerState(attrs, endpoint_);
    const std::vector<bool> partStatus =
        peerState ? peerState->partStatus : std::vector<bool>();
    markEd2kPeerQueued(attrs, endpoint_, rank, partStatus);
    state_ = State::DONE;
    break;
  }
  case ed2k::OP_SENDINGPART:
  case ed2k::OP_SENDINGPART_I64: {
    const bool is64 = currentHeader_.opcode == ed2k::OP_SENDINGPART_I64;
    const size_t metaLength = is64 ? 32 : 24;
    if (body_.size() < metaLength) {
      throw DL_RETRY_EX("Truncated ED2K part packet.");
    }
    auto hash = body_.substr(0, ed2k::HASH_LENGTH);
    if (hash != attrs->link.hash) {
      throw DL_RETRY_EX("ED2K part hash mismatch.");
    }
    const int64_t begin =
        is64 ? ed2k::readUInt64(body_.data() + 16)
             : ed2k::readUInt32(body_.data() + 16);
    const int64_t end =
        is64 ? ed2k::readUInt64(body_.data() + 24)
             : ed2k::readUInt32(body_.data() + 20);
    if (begin < 0 || end <= begin ||
        static_cast<size_t>(end - begin) != body_.size() - metaLength) {
      throw DL_RETRY_EX("Bad ED2K part range.");
    }
    const auto data = body_.substr(metaLength);
    if (!remotePeerInfo_.userHash.empty()) {
      auto rgman = getDownloadEngine()->getRequestGroupMan().get();
      if (rgman && rgman->getEd2kUploadQueue()) {
        rgman->getEd2kUploadQueue()->noteDownloaded(remotePeerInfo_.userHash,
                                                    data.size());
      }
    }
    handlePartData(begin, data);
    if (getRequestGroup()->downloadFinished()) {
      state_ = State::DONE;
    }
    else {
      queuePeerPartRequest();
      state_ = State::WRITE;
    }
    break;
  }
  case ed2k::OP_REQUESTPARTS:
    createSharedResponder().queuePartAnswers(body_, false);
    if (!outbox_.empty()) {
      state_ = State::WRITE;
    }
    break;
  case ed2k::OP_REQUESTPARTS_I64:
    createSharedResponder().queuePartAnswers(body_, true);
    if (!outbox_.empty()) {
      state_ = State::WRITE;
    }
    break;
  case ed2k::OP_COMPRESSEDPART:
  case ed2k::OP_COMPRESSEDPART_I64: {
    const bool is64 = currentHeader_.opcode == ed2k::OP_COMPRESSEDPART_I64;
    ed2k::CompressedPartHeader header;
    std::string compressedData;
    if (!ed2k::parseCompressedPartPayload(header, compressedData, body_,
                                          attrs->link.hash, is64)) {
      throw DL_RETRY_EX("Bad compressed ED2K part packet.");
    }
    ed2k::PeerTransfer transfer(getDownloadContext().get(),
                                getPieceStorage().get(), getSegmentMan().get(),
                                getCuid());
    const auto expectedLength = transfer.expectedPartLength(header.begin);
    if (expectedLength <= 0) {
      throw DL_RETRY_EX("Unexpected compressed ED2K part range.");
    }
    std::string data;
    if (!ed2k::inflateCompressedPartData(
            data, compressedData, static_cast<size_t>(expectedLength))) {
      throw DL_RETRY_EX("Bad compressed ED2K part data.");
    }
    handlePartData(header.begin, data);
    if (getRequestGroup()->downloadFinished()) {
      state_ = State::DONE;
    }
    else {
      queuePeerPartRequest();
      state_ = State::WRITE;
    }
    break;
  }
  default:
    break;
  }
}

void Ed2kCommand::handlePacket()
{
  if (mode_ == Mode::SERVER) {
    handleServerPacket();
  }
  else {
    handlePeerPacket();
  }
}

bool Ed2kCommand::executeInternal()
{
  while (true) {
    switch (state_) {
    case State::INIT:
      if (incoming_) {
        state_ = State::READ_HEADER;
        break;
      }
      if (mode_ == Mode::SERVER) {
        queueServerLogin();
      }
      else {
        queuePeerHello();
      }
      startResolve();
      return false;
    case State::RESOLVING:
      startResolve();
      return false;
    case State::CONNECTING:
      if (!getSocket()->isWritable(0)) {
        setWriteCheckSocket(getSocket());
        return false;
      }
      if (!checkIfConnectionEstablished(getSocket(), connectedHostname_,
                                         connectedAddr_, connectedPort_)) {
        return true;
      }
      if (mode_ == Mode::SERVER) {
        updateEd2kServerConnected(getEd2kAttrs(getDownloadContext()),
                                  endpoint_);
      }
      state_ = State::WRITE;
      break;
    case State::WRITE:
      if (!flushOutbox()) {
        return false;
      }
      break;
    case State::READ_HEADER:
      if (!readHeader()) {
        return false;
      }
      break;
    case State::READ_BODY:
      if (!readBody()) {
        return false;
      }
      break;
    case State::DONE:
      return true;
    }
    if (state_ != State::DONE && !outbox_.empty()) {
      state_ = State::WRITE;
    }
  }
}

} // namespace aria2
