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
#include "Ed2kSharedPeerCommand.h"

#include <algorithm>

#include "DlRetryEx.h"
#include "DownloadEngine.h"
#include "Ed2kSharedResponder.h"
#include "Ed2kSharedStore.h"
#include "Ed2kUploadQueue.h"
#include "LogFactory.h"
#include "Logger.h"
#include "Option.h"
#include "RequestGroupMan.h"
#include "SocketCore.h"
#include "ed2k_aich.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
#include "ed2k_peer.h"
#include "ed2k_server.h"
#include "fmt.h"
#include "prefs.h"
#include "wallclock.h"

namespace aria2 {

namespace {

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
  info.miscOptions2.supportsLargeFiles = true;
  info.miscOptions2.supportsSourceExchange2 = true;
  return info;
}

} // namespace

Ed2kSharedPeerCommand::Ed2kSharedPeerCommand(
    cuid_t cuid, DownloadEngine* e, const ed2k::Endpoint& endpoint,
    const std::shared_ptr<SocketCore>& socket)
    : Command(cuid),
      e_(e),
      endpoint_(endpoint),
      socket_(socket),
      state_(State::READ_HEADER),
      headerRead_(0),
      bodyRead_(0),
      localPeerInfo_(createLocalPeerInfo()),
      writeCheck_(false)
{
  socket_->setNonBlockingMode();
  e_->addSocketForReadCheck(socket_, this);
}

Ed2kSharedPeerCommand::~Ed2kSharedPeerCommand()
{
  e_->deleteSocketForReadCheck(socket_, this);
  if (writeCheck_) {
    e_->deleteSocketForWriteCheck(socket_, this);
  }
  if (e_ && e_->getRequestGroupMan() &&
      e_->getRequestGroupMan()->getEd2kUploadQueue()) {
    e_->getRequestGroupMan()->getEd2kUploadQueue()->remove(endpoint_);
  }
}

void Ed2kSharedPeerCommand::addCommandSelf()
{
  e_->addCommand(std::unique_ptr<Command>(this));
}

void Ed2kSharedPeerCommand::setReadCheck()
{
  e_->addSocketForReadCheck(socket_, this);
}

void Ed2kSharedPeerCommand::setWriteCheck(bool enabled)
{
  if (enabled && !writeCheck_) {
    writeCheck_ = true;
    e_->addSocketForWriteCheck(socket_, this);
  }
  else if (!enabled && writeCheck_) {
    writeCheck_ = false;
    e_->deleteSocketForWriteCheck(socket_, this);
  }
}

void Ed2kSharedPeerCommand::queuePacket(uint8_t protocol, uint8_t opcode,
                                        const std::string& payload)
{
  outbox_.push_back(ed2k::createPacket(protocol, opcode, payload));
}

void Ed2kSharedPeerCommand::queuePeerHelloAnswer()
{
  auto payload = ed2k::createPeerHelloPayload(
      clientHash(e_), 0, localEd2kTcpPort(e_), ed2k::Endpoint(),
      "aria2-next", localPeerInfo_, false);
  queuePacket(ed2k::PROTO_EDONKEY, ed2k::OP_HELLOANSWER, payload);
}

void Ed2kSharedPeerCommand::queueEmuleInfo(bool answer)
{
  queuePacket(ed2k::PROTO_EMULE,
              answer ? ed2k::OP_EMULEINFOANSWER : ed2k::OP_EMULEINFO,
              ed2k::createEmuleInfoPayload(localPeerInfo_));
}

bool Ed2kSharedPeerCommand::flushOutbox()
{
  while (!outbox_.empty()) {
    auto& data = outbox_.front();
    auto written = socket_->writeData(data.data(), data.size());
    if (written == 0) {
      setWriteCheck(socket_->wantWrite());
      if (socket_->wantRead()) {
        setReadCheck();
      }
      addCommandSelf();
      return false;
    }
    data.erase(0, static_cast<size_t>(written));
    if (!data.empty()) {
      setWriteCheck(true);
      addCommandSelf();
      return false;
    }
    outbox_.pop_front();
  }
  setWriteCheck(false);
  setReadCheck();
  state_ = State::READ_HEADER;
  return true;
}

bool Ed2kSharedPeerCommand::readHeader()
{
  while (headerRead_ < headerBuf_.size()) {
    size_t len = headerBuf_.size() - headerRead_;
    socket_->readData(headerBuf_.data() + headerRead_, len);
    if (len == 0) {
      if (!socket_->wantRead() && !socket_->wantWrite()) {
        throw DL_RETRY_EX("ED2K shared peer connection closed.");
      }
      if (socket_->wantRead()) {
        setReadCheck();
      }
      setWriteCheck(socket_->wantWrite());
      addCommandSelf();
      return false;
    }
    headerRead_ += len;
  }
  if (!ed2k::readPacketHeader(currentHeader_, headerBuf_.data(),
                              headerBuf_.size())) {
    throw DL_RETRY_EX("Bad ED2K shared peer packet header.");
  }
  if (currentHeader_.protocol != ed2k::PROTO_EDONKEY &&
      currentHeader_.protocol != ed2k::PROTO_EMULE) {
    throw DL_RETRY_EX("Unsupported ED2K shared peer protocol.");
  }
  if (currentHeader_.payloadSize() > 8_m) {
    throw DL_RETRY_EX("ED2K shared peer packet is too large.");
  }
  body_.assign(currentHeader_.payloadSize(), '\0');
  bodyRead_ = 0;
  headerRead_ = 0;
  state_ = State::READ_BODY;
  return true;
}

bool Ed2kSharedPeerCommand::readBody()
{
  while (bodyRead_ < body_.size()) {
    size_t len = body_.size() - bodyRead_;
    socket_->readData(&body_[bodyRead_], len);
    if (len == 0) {
      if (!socket_->wantRead() && !socket_->wantWrite()) {
        throw DL_RETRY_EX("ED2K shared peer connection closed.");
      }
      if (socket_->wantRead()) {
        setReadCheck();
      }
      setWriteCheck(socket_->wantWrite());
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

ed2k::SharedResponder Ed2kSharedPeerCommand::createSharedResponder()
{
  auto rgman = e_->getRequestGroupMan().get();
  auto store = rgman ? rgman->getEd2kSharedStore() : nullptr;
  auto uploadQueue = rgman ? rgman->getEd2kUploadQueue() : nullptr;
  return ed2k::SharedResponder(store, uploadQueue, rgman, endpoint_,
                               remotePeerInfo_.userHash, outbox_);
}

void Ed2kSharedPeerCommand::handleEmulePacket()
{
  switch (currentHeader_.opcode) {
  case ed2k::OP_EMULEINFO:
    if (ed2k::parseEmuleInfoPayload(remotePeerInfo_, body_)) {
      queueEmuleInfo(true);
    }
    break;
  case ed2k::OP_EMULEINFOANSWER:
    if (!ed2k::parseEmuleInfoPayload(remotePeerInfo_, body_)) {
      throw DL_RETRY_EX("Bad ED2K shared peer eMule info answer.");
    }
    break;
  case ed2k::OP_REQUESTSOURCES:
    if (!createSharedResponder().queueSourceExchangeAnswer(
            body_,
            std::max<uint8_t>(1, remotePeerInfo_.miscOptions.sourceExchange1Version))) {
      createSharedResponder().queueNoFile(body_);
    }
    break;
  case ed2k::OP_REQUESTSOURCES2: {
    uint8_t version = 0;
    std::string fileHash;
    if (body_.size() == ed2k::HASH_LENGTH) {
      fileHash = body_;
    }
    else if (body_.size() >= ed2k::HASH_LENGTH + 3) {
      fileHash = body_.substr(3, ed2k::HASH_LENGTH);
    }
    if (fileHash.empty() ||
        !ed2k::parseRequestSources2Payload(version, body_, fileHash)) {
      throw DL_RETRY_EX("Bad ED2K shared peer source request.");
    }
    if (!createSharedResponder().queueSourceExchangeAnswer(fileHash, version)) {
      createSharedResponder().queueNoFile(fileHash);
    }
    break;
  }
  case ed2k::OP_AICHFILEHASHREQ:
    if (body_.size() == ed2k::HASH_LENGTH) {
      createSharedResponder().queueAichFileHashAnswer(body_);
    }
    break;
  case ed2k::OP_AICHREQUEST:
    if (body_.size() >= ed2k::HASH_LENGTH) {
      createSharedResponder().queueAichAnswer(
          body_.substr(0, ed2k::HASH_LENGTH), body_);
    }
    break;
  default:
    break;
  }
}

void Ed2kSharedPeerCommand::handleEdonkeyPacket()
{
  switch (currentHeader_.opcode) {
  case ed2k::OP_HELLO:
    ed2k::parsePeerHelloUserHash(remotePeerInfo_.userHash, body_, true);
    queuePeerHelloAnswer();
    queueEmuleInfo(true);
    break;
  case ed2k::OP_HELLOANSWER:
    ed2k::parsePeerHelloUserHash(remotePeerInfo_.userHash, body_, false);
    queueEmuleInfo(false);
    break;
  case ed2k::OP_STARTUPLOADREQ:
    if (body_.size() != ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K shared peer upload request.");
    }
    createSharedResponder().requestUploadSlot(
        body_, std::chrono::duration_cast<std::chrono::seconds>(
                   global::wallclock().getTime().time_since_epoch())
                   .count());
    break;
  case ed2k::OP_REQUESTFILENAME:
    if (body_.size() < ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K shared peer file request.");
    }
    createSharedResponder().queueFileNameAnswer(
        body_.substr(0, ed2k::HASH_LENGTH));
    break;
  case ed2k::OP_SETREQFILEID:
    if (body_.size() != ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K shared peer status request.");
    }
    createSharedResponder().queueFileStatusAnswer(body_);
    break;
  case ed2k::OP_HASHSETREQUEST:
    if (body_.size() != ed2k::HASH_LENGTH) {
      throw DL_RETRY_EX("Bad ED2K shared peer hashset request.");
    }
    createSharedResponder().queueHashSetAnswer(body_);
    break;
  case ed2k::OP_REQUESTPARTS:
    createSharedResponder().queuePartAnswers(body_, false);
    break;
  case ed2k::OP_REQUESTPARTS_I64:
    createSharedResponder().queuePartAnswers(body_, true);
    break;
  default:
    break;
  }
}

void Ed2kSharedPeerCommand::handlePacket()
{
  if (currentHeader_.protocol == ed2k::PROTO_EMULE) {
    handleEmulePacket();
  }
  else {
    handleEdonkeyPacket();
  }
  if (!outbox_.empty()) {
    state_ = State::WRITE;
  }
}

bool Ed2kSharedPeerCommand::execute()
{
  if (e_->isHaltRequested()) {
    return true;
  }
  try {
    while (true) {
      switch (state_) {
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
  catch (RecoverableException& ex) {
    A2_LOG_DEBUG_EX(fmt("CUID#%" PRId64 " - ED2K shared peer failed.",
                        getCuid()),
                    ex);
    return true;
  }
}

} // namespace aria2
