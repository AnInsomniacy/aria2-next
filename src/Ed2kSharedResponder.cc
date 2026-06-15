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
#include "Ed2kSharedResponder.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "Ed2kShareIndex.h"
#include "Ed2kSharedFile.h"
#include "Ed2kUploadQueue.h"
#include "RequestGroupMan.h"
#include "ed2k_aich.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"

namespace aria2 {

namespace ed2k {

SharedResponder::SharedResponder(UploadQueue* uploadQueue, RequestGroupMan* rgman,
                                 const Endpoint& endpoint,
                                 const std::string& userHash,
                                 std::deque<std::string>& outbox)
    : uploadQueue_(uploadQueue),
      outbox_(&outbox),
      endpoint_(endpoint),
      userHash_(userHash),
      rgman_(rgman)
{
}

std::unique_ptr<SharedSource> SharedResponder::findFile(
    const std::string& hash) const
{
  return rgman_ ? findSharedSource(rgman_, hash) : nullptr;
}

bool SharedResponder::hasFile(const std::string& hash) const
{
  return findFile(hash) != nullptr;
}

void SharedResponder::queuePacket(uint8_t protocol, uint8_t opcode,
                                  const std::string& payload)
{
  outbox_->push_back(createPacket(protocol, opcode, payload));
}

void SharedResponder::queueNoFile(const std::string& fileHash)
{
  if (fileHash.size() == HASH_LENGTH) {
    queuePacket(PROTO_EDONKEY, OP_FILEREQANSNOFIL, fileHash);
  }
}

bool SharedResponder::queueFileNameAnswer(const std::string& fileHash)
{
  auto file = findFile(fileHash);
  if (!file) {
    queueNoFile(fileHash);
    return false;
  }
  queuePacket(PROTO_EDONKEY, OP_REQFILENAMEANSWER,
              createSharedFileNameAnswerPayload(*file));
  return true;
}

bool SharedResponder::queueFileStatusAnswer(const std::string& fileHash)
{
  auto file = findFile(fileHash);
  if (!file) {
    queueNoFile(fileHash);
    return false;
  }
  queuePacket(PROTO_EDONKEY, OP_FILESTATUS,
              createSharedFileStatusPayload(*file));
  return true;
}

bool SharedResponder::queueHashSetAnswer(const std::string& fileHash)
{
  auto file = findFile(fileHash);
  if (!file) {
    queueNoFile(fileHash);
    return false;
  }
  std::string payload;
  if (!createSharedFileHashSetPayload(payload, *file)) {
    return false;
  }
  queuePacket(PROTO_EDONKEY, OP_HASHSETANSWER, payload);
  return true;
}

bool SharedResponder::queueSourceExchangeAnswer(const std::string& fileHash,
                                                uint8_t version)
{
  if (!hasFile(fileHash)) {
    return false;
  }
  std::vector<SourceExchangeEntry> entries;
  if (version >= 2) {
    queuePacket(PROTO_EMULE, OP_ANSWERSOURCES2,
                createAnswerSources2Payload(fileHash, version, entries));
  }
  else {
    queuePacket(PROTO_EMULE, OP_ANSWERSOURCES,
                createAnswerSourcesPayload(fileHash, version, entries));
  }
  return true;
}

bool SharedResponder::queueAichFileHashAnswer(const std::string& fileHash)
{
  auto file = findFile(fileHash);
  if (!file || file->aichRootHash().empty()) {
    return false;
  }
  queuePacket(PROTO_EMULE, OP_AICHFILEHASHANS,
              createAichFileHashAnswerPayload(fileHash,
                                              file->aichRootHash()));
  return true;
}

bool SharedResponder::queueMultipacketAnswer(const std::string& requestPayload,
                                             bool extendedMultipacket,
                                             uint8_t extendedRequestsVersion,
                                             uint8_t sourceExchangeVersion)
{
  const size_t fixedSize = HASH_LENGTH + (extendedMultipacket ? 8 : 0);
  if (requestPayload.size() < fixedSize) {
    return false;
  }
  size_t offset = 0;
  auto fileHash = readBytes(requestPayload, offset, HASH_LENGTH);
  auto file = findFile(fileHash);
  if (extendedMultipacket && offset + 8 <= requestPayload.size()) {
    const auto requestedSize =
        static_cast<int64_t>(readUInt64(readBytes(requestPayload, offset, 8).data()));
    if (file && requestedSize != file->size()) {
      queueNoFile(fileHash);
      return false;
    }
  }
  if (!file) {
    queueNoFile(fileHash);
    return false;
  }
  std::string answer = fileHash;
  while (offset < requestPayload.size()) {
    const auto opcode = readByte(requestPayload, offset);
    switch (opcode) {
    case OP_REQUESTFILENAME:
      if (extendedRequestsVersion > 0) {
        std::vector<bool> ignored;
        if (!parsePartStatusPayload(ignored, requestPayload, offset)) {
          return false;
        }
      }
      if (extendedRequestsVersion > 1) {
        if (offset + 2 > requestPayload.size()) {
          return false;
        }
        offset += 2;
      }
      answer.push_back(static_cast<char>(OP_REQFILENAMEANSWER));
      {
        auto nameAnswer = createSharedFileNameAnswerPayload(*file);
        answer.append(nameAnswer, HASH_LENGTH, nameAnswer.size() - HASH_LENGTH);
      }
      break;
    case OP_SETREQFILEID:
      answer.push_back(static_cast<char>(OP_FILESTATUS));
      {
        auto statusAnswer = createSharedFileStatusPayload(*file);
        answer.append(statusAnswer, HASH_LENGTH,
                      statusAnswer.size() - HASH_LENGTH);
      }
      break;
    case OP_AICHFILEHASHREQ:
      if (!file->aichRootHash().empty()) {
        answer.push_back(static_cast<char>(OP_AICHFILEHASHANS));
        answer += file->aichRootHash();
      }
      break;
    case OP_REQUESTSOURCES:
      queueSourceExchangeAnswer(fileHash,
                                std::max<uint8_t>(1, sourceExchangeVersion));
      break;
    case OP_REQUESTSOURCES2: {
      if (offset + 3 > requestPayload.size()) {
        return false;
      }
      const auto version = readByte(requestPayload, offset);
      offset += 2;
      queueSourceExchangeAnswer(fileHash, version);
      break;
    }
    default:
      return false;
    }
  }
  if (answer.size() > HASH_LENGTH) {
    queuePacket(PROTO_EMULE, OP_MULTIPACKETANSWER, answer);
  }
  return true;
}

bool SharedResponder::queueAichAnswer(const std::string& fileHash,
                                      const std::string& requestPayload)
{
  AichRequest request;
  if (!parseAichRequestPayload(request, requestPayload, fileHash)) {
    return false;
  }
  auto file = findFile(fileHash);
  if (!file || file->aichRootHash().empty() ||
      request.rootHash != file->aichRootHash()) {
    queuePacket(PROTO_EMULE, OP_AICHANSWER, fileHash);
    return false;
  }
  std::string payload;
  if (!createSharedFileAichAnswerPayload(payload, *file, request.partIndex,
                                         request.rootHash)) {
    queuePacket(PROTO_EMULE, OP_AICHANSWER, fileHash);
    return false;
  }
  queuePacket(PROTO_EMULE, OP_AICHANSWER, payload);
  return true;
}

bool SharedResponder::requestUploadSlot(const std::string& fileHash,
                                        int64_t now)
{
  if (!hasFile(fileHash)) {
    queueNoFile(fileHash);
    return false;
  }
  if (!uploadQueue_) {
    queuePacket(PROTO_EDONKEY, OP_ACCEPTUPLOADREQ, "");
    return true;
  }
  if (uploadQueue_->requestUpload(endpoint_, userHash_, fileHash, now,
                                  rgman_)) {
    queuePacket(PROTO_EDONKEY, OP_ACCEPTUPLOADREQ, "");
    return true;
  }
  queuePacket(PROTO_EDONKEY, OP_QUEUERANKING,
              createQueueRankPayload(uploadQueue_->queueRank(endpoint_)));
  return false;
}

bool SharedResponder::queuePartAnswers(const std::string& requestPayload,
                                       bool use64BitOffsets)
{
  std::vector<PartRange> ranges;
  std::string fileHash;
  if (!parsePartRequestPayload(ranges, fileHash, requestPayload,
                               use64BitOffsets)) {
    return false;
  }
  auto file = findFile(fileHash);
  if (!file) {
    queueNoFile(fileHash);
    return false;
  }
  if (uploadQueue_ && !uploadQueue_->isUploading(endpoint_)) {
    queuePacket(PROTO_EDONKEY, OP_QUEUERANKING,
                createQueueRankPayload(uploadQueue_->queueRank(endpoint_)));
    return false;
  }
  for (const auto& range : ranges) {
    std::string payload;
    if (!createSharedFilePartPayload(payload, *file, range,
                                     use64BitOffsets)) {
      continue;
    }
    queuePacket(PROTO_EDONKEY,
                use64BitOffsets ? OP_SENDINGPART_I64 : OP_SENDINGPART,
                payload);
    if (uploadQueue_) {
      uploadQueue_->noteUploaded(endpoint_, range.end - range.begin);
    }
    file->recordUpload(static_cast<size_t>(range.end - range.begin));
  }
  return true;
}

} // namespace ed2k

} // namespace aria2
