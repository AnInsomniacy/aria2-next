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
#include "FileEntry.h"
#include "LogFactory.h"
#include "Logger.h"
#include "message.h"
#include "Option.h"
#include "Piece.h"
#include "PieceStorage.h"
#include "Request.h"
#include "RequestGroup.h"
#include "Segment.h"
#include "SegmentMan.h"
#include "SocketCore.h"
#include "ed2k_aich.h"
#include "ed2k_compression.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
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
    return id.substr(0, ed2k::HASH_LENGTH);
  }
  id.append(ed2k::HASH_LENGTH - id.size(), '\0');
  return id;
}

ed2k::EmulePeerInfo createLocalPeerInfo()
{
  ed2k::EmulePeerInfo info;
  info.version = 0x3c;
  info.protocolVersion = 0x01;
  info.miscOptions.aichVersion = 1;
  info.miscOptions.unicode = true;
  info.miscOptions.dataCompressionVersion = 1;
  info.miscOptions.sourceExchange1Version = 3;
  info.miscOptions.extendedRequestsVersion = 2;
  info.miscOptions.multiPacket = true;
  info.miscOptions2.supportsLargeFiles = true;
  info.miscOptions2.supportsSourceExchange2 = true;
  return info;
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
      use64BitOffsets_(requestGroup->getDownloadContext()->getTotalLength() >
                       std::numeric_limits<uint32_t>::max()),
      localPeerInfo_(createLocalPeerInfo())
{
  setTimeout(std::chrono::seconds(getOption()->getAsInt(PREF_CONNECT_TIMEOUT)));
  disableReadCheckSocket();
  disableWriteCheckSocket();
}

Ed2kCommand::~Ed2kCommand() = default;

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
    if (mode_ == Mode::SERVER) {
      updateEd2kServerFailure(
          getEd2kAttrs(getDownloadContext()), endpoint_,
          std::chrono::duration_cast<std::chrono::seconds>(
              global::wallclock().getTime().time_since_epoch())
              .count(),
          std::max<int64_t>(1, getOption()->getAsInt(PREF_RETRY_WAIT)));
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
  outbox_.push_back(ed2k::createPacket(protocol, opcode, payload));
}

void Ed2kCommand::queueServerLogin()
{
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_LOGINREQUEST,
              ed2k::createLoginRequestPayload(clientHash(getDownloadEngine()),
                                              0, "aria2-next"));
}

void Ed2kCommand::queueGetSources()
{
  const auto attrs = getEd2kAttrs(getDownloadContext());
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_GETSOURCES,
              ed2k::createGetSourcesPayload(attrs->link.hash,
                                            attrs->link.size));
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
}

void Ed2kCommand::queueCallbackRequest(uint32_t clientId)
{
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_CALLBACKREQUEST,
              ed2k::createCallbackRequestPayload(clientId));
}

void Ed2kCommand::queuePeerHello()
{
  std::string payload;
  payload.push_back(static_cast<char>(ed2k::HASH_LENGTH));
  payload += ed2k::createLoginRequestPayload(clientHash(getDownloadEngine()),
                                             0, "aria2-next");
  payload += std::string(6, '\0');
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_HELLO, payload);
}

void Ed2kCommand::queuePeerHelloAnswer()
{
  std::string payload;
  payload += ed2k::createLoginRequestPayload(clientHash(getDownloadEngine()),
                                             0, "aria2-next");
  payload += std::string(6, '\0');
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
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_REQUESTFILENAME,
              getEd2kAttrs(getDownloadContext())->link.hash);
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
  queuePacket(ed2k::PROTO_EMULE, ed2k::OP_REQUESTSOURCES2,
              ed2k::createRequestSources2Payload(
                  getEd2kAttrs(getDownloadContext())->link.hash));
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
  std::vector<std::shared_ptr<Segment>> inFlight;
  getSegmentMan()->getInFlightSegment(inFlight, getCuid());
  for (const auto& segment : inFlight) {
    if (ranges.size() >= 3) {
      break;
    }
    if (segment->complete()) {
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
  while (ranges.size() < 3) {
    auto segment = getSegmentMan()->getSegment(getCuid(), ed2k::BLOCK_LENGTH);
    if (!segment) {
      break;
    }
    const int64_t begin = segment->getPositionToWrite();
    const int64_t end =
        std::min(begin + static_cast<int64_t>(ed2k::BLOCK_LENGTH),
                 segment->getPosition() + segment->getLength());
    if (end <= begin) {
      getSegmentMan()->cancelSegment(getCuid(), segment);
      break;
    }
    ed2k::PartRange range;
    range.begin = begin;
    range.end = end;
    ranges.push_back(range);
  }
  if (ranges.empty()) {
    return;
  }
  const auto attrs = getEd2kAttrs(getDownloadContext());
  queuePacket(ed2k::PROTO_EDONKEY,
              use64BitOffsets_ ? ed2k::OP_REQUESTPARTS_I64
                               : ed2k::OP_REQUESTPARTS,
              ed2k::createRequestPartsPayload(attrs->link.hash, ranges,
                                              use64BitOffsets_));
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
      currentHeader_.protocol != ed2k::PROTO_EMULE) {
    throw DL_RETRY_EX("Unsupported ED2K packet protocol.");
  }
  if (currentHeader_.payloadSize() > 8_m) {
    throw DL_RETRY_EX("ED2K packet is too large.");
  }
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
      setReadCheckSocketIf(getSocket(), getSocket()->wantRead());
      setWriteCheckSocketIf(getSocket(), getSocket()->wantWrite());
      addCommandSelf();
      return false;
    }
    bodyRead_ += len;
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
  addEd2kPeer(getEd2kAttrs(getDownloadContext()), peer);
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

int64_t Ed2kCommand::expectedPartLength(int64_t begin)
{
  std::vector<std::shared_ptr<Segment>> segments;
  getSegmentMan()->getInFlightSegment(segments, getCuid());
  for (const auto& segment : segments) {
    if (segment->getPositionToWrite() == begin) {
      return std::min(begin + static_cast<int64_t>(ed2k::BLOCK_LENGTH),
                      segment->getPosition() + segment->getLength()) -
             begin;
    }
  }
  return 0;
}

void Ed2kCommand::handlePartData(int64_t begin, const std::string& data)
{
  if (begin < 0 || data.empty()) {
    throw DL_RETRY_EX("Bad ED2K part range.");
  }
  getPieceStorage()->getDiskAdaptor()->writeData(
      reinterpret_cast<const unsigned char*>(data.data()), data.size(), begin);
  getDownloadContext()->updateDownload(data.size());

  std::vector<std::shared_ptr<Segment>> segments;
  getSegmentMan()->getInFlightSegment(segments, getCuid());
  for (auto& segment : segments) {
    if (segment->getPositionToWrite() == begin) {
      segment->updateWrittenLength(data.size());
      if (segment->complete()) {
        if (verifyPiece(segment->getIndex())) {
          completeVerifiedSegment(segment->getIndex());
        }
        else {
          segment->clear(getPieceStorage()->getWrDiskCache());
          getSegmentMan()->cancelSegment(getCuid(), segment);
          queueAichRecoveryRequest(segment->getIndex());
          throw DL_RETRY_EX("Bad ED2K piece hash.");
        }
      }
      return;
    }
  }
  throw DL_RETRY_EX("Unexpected ED2K part range.");
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
    if (attrs->searchActive) {
      queueSearchRequest();
    }
    else {
      queueGetSources();
    }
    state_ = State::WRITE;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_FOUNDSOURCES) {
    std::vector<ed2k::FoundSource> sources;
    if (!ed2k::parseFoundSourcesPayload(
            sources, body_, attrs->link.hash)) {
      throw DL_RETRY_EX("ED2K found sources hash mismatch.");
    }
    auto serverState = getEd2kServerState(attrs, endpoint_);
    const bool canRequestCallback =
        serverState && serverState->handshakeCompleted && serverState->highId;
    for (const auto& source : sources) {
      if (source.lowId) {
        if (canRequestCallback) {
          queueCallbackRequest(source.clientId);
        }
      }
      else {
        addPeer(source.endpoint);
      }
    }
    schedulePendingPeers();
    state_ = outbox_.empty() ? State::DONE : State::WRITE;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_CALLBACKREQUESTED) {
    ed2k::Endpoint peer;
    if (!ed2k::parseCallbackRequestIncomingPayload(peer, body_)) {
      throw DL_RETRY_EX("Bad ED2K callback request.");
    }
    addPeer(peer);
    schedulePendingPeers();
    state_ = State::DONE;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_CALLBACK_FAIL) {
    state_ = State::DONE;
    return;
  }
  if (currentHeader_.opcode == ed2k::OP_SEARCHRESULT) {
    ed2k::SearchResult result;
    if (!ed2k::parseSearchResultPayload(result, body_, "server")) {
      throw DL_RETRY_EX("Bad ED2K search result.");
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
    updateEd2kServerMessage(attrs, endpoint_, message);
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
      std::vector<ed2k::Endpoint> peers;
      peers.reserve(answer.entries.size());
      for (const auto& entry : answer.entries) {
        peers.push_back(entry.endpoint);
      }
      addPeers(peers);
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
      std::vector<ed2k::Endpoint> peers;
      peers.reserve(answer.entries.size());
      for (const auto& entry : answer.entries) {
        peers.push_back(entry.endpoint);
      }
      addPeers(peers);
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
      break;
    case ed2k::OP_REQUESTSOURCES2: {
      uint8_t version = 0;
      if (!ed2k::parseRequestSources2Payload(version, body_,
                                             attrs->link.hash)) {
        throw DL_RETRY_EX("Bad ED2K source exchange request.");
      }
      queueSourceExchangeAnswer(version);
      if (!outbox_.empty()) {
        state_ = State::WRITE;
      }
      break;
    }
    case ed2k::OP_AICHFILEHASHREQ:
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
      break;
    }
    case ed2k::OP_AICHREQUEST:
      break;
    default:
      break;
    }
    return;
  }
  switch (currentHeader_.opcode) {
  case ed2k::OP_HELLO:
    queuePeerHelloAnswer();
    queueEmuleInfo(true);
    queuePeerFileRequest();
    state_ = State::WRITE;
    break;
  case ed2k::OP_HELLOANSWER:
    queueEmuleInfo(false);
    queuePeerFileRequest();
    state_ = State::WRITE;
    break;
  case ed2k::OP_REQFILENAMEANSWER:
    if (body_.size() < ed2k::HASH_LENGTH ||
        body_.substr(0, ed2k::HASH_LENGTH) != attrs->link.hash) {
      throw DL_RETRY_EX("ED2K file answer hash mismatch.");
    }
    if (!peerFileStatusReceived_) {
      queuePeerFileStatusRequest();
      state_ = State::WRITE;
    }
    break;
  case ed2k::OP_FILESTATUS: {
    std::vector<bool> bitfield;
    if (!ed2k::parseFileStatusPayload(bitfield, body_, attrs->link.hash)) {
      throw DL_RETRY_EX("ED2K file status hash mismatch.");
    }
    peerFileStatusReceived_ = true;
    if (getDownloadContext()->getTotalLength() > ed2k::PIECE_LENGTH &&
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
        pieceHashes.size() != getDownloadContext()->getNumPieces() ||
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
  case ed2k::OP_ACCEPTUPLOADREQ:
    peerAccepted_ = true;
    queuePeerPartRequest();
    state_ = State::WRITE;
    break;
  case ed2k::OP_OUTOFPARTREQS:
  case ed2k::OP_FILEREQANSNOFIL:
  case ed2k::OP_QUEUERANK:
    state_ = State::DONE;
    break;
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
    handlePartData(begin, body_.substr(metaLength));
    if (getRequestGroup()->downloadFinished()) {
      state_ = State::DONE;
    }
    else {
      queuePeerPartRequest();
      state_ = State::WRITE;
    }
    break;
  }
  case ed2k::OP_COMPRESSEDPART:
  case ed2k::OP_COMPRESSEDPART_I64: {
    const bool is64 = currentHeader_.opcode == ed2k::OP_COMPRESSEDPART_I64;
    ed2k::CompressedPartHeader header;
    std::string compressedData;
    if (!ed2k::parseCompressedPartPayload(header, compressedData, body_,
                                          attrs->link.hash, is64)) {
      throw DL_RETRY_EX("Bad compressed ED2K part packet.");
    }
    const auto expectedLength = expectedPartLength(header.begin);
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

std::string Ed2kCommand::readPiece(size_t index)
{
  auto piece = getPieceStorage()->getPiece(index);
  if (!piece) {
    return std::string();
  }
  std::string data(piece->getLength(), '\0');
  auto nread = getPieceStorage()->getDiskAdaptor()->readData(
      reinterpret_cast<unsigned char*>(&data[0]), data.size(),
      static_cast<int64_t>(index) * getDownloadContext()->getPieceLength());
  if (nread != static_cast<ssize_t>(data.size())) {
    return std::string();
  }
  return data;
}

bool Ed2kCommand::verifyPiece(size_t index)
{
  auto attrs = getEd2kAttrs(getDownloadContext());
  if (attrs->pieceHashes.empty() && getDownloadContext()->getNumPieces() == 1) {
    attrs->pieceHashes.push_back(attrs->link.hash);
  }
  if (index >= attrs->pieceHashes.size()) {
    return false;
  }
  auto data = readPiece(index);
  return !data.empty() && ed2k::md4Digest(data) == attrs->pieceHashes[index];
}

void Ed2kCommand::completeVerifiedSegment(size_t index)
{
  std::vector<std::shared_ptr<Segment>> segments;
  getSegmentMan()->getInFlightSegment(segments, getCuid());
  for (auto& segment : segments) {
    if (segment->getIndex() == index) {
      getSegmentMan()->completeSegment(getCuid(), segment);
      break;
    }
  }
}

bool Ed2kCommand::executeInternal()
{
  while (true) {
    switch (state_) {
    case State::INIT:
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
    if (!outbox_.empty()) {
      state_ = State::WRITE;
    }
  }
}

} // namespace aria2
