/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/SharedFile.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <algorithm>
#include <limits>

#include "ed2k/Aich.h"
#include "ed2k/Hash.h"
#include "ed2k/Packet.h"
#include "ed2k/PeerProtocol.h"

namespace aria2::ed2k {

namespace {

std::string createUInt16String(const std::string& value)
{
  if (value.size() > std::numeric_limits<uint16_t>::max()) {
    throw ProtocolError("ED2K string is too large.");
  }
  return packUInt16(static_cast<uint16_t>(value.size())) + value;
}

size_t countBlocks(size_t length, size_t baseSize)
{
  return (length + baseSize - 1) / baseSize;
}

bool appendAichHashForRange(std::string& recovery, const SharedSource& source,
                            uint32_t ident, int64_t begin, size_t length,
                            bool use32BitIdent)
{
  std::string data;
  if (!source.readRange(data, begin, begin + length)) {
    return false;
  }
  recovery += use32BitIdent ? packUInt32(ident) : packUInt16(ident);
  recovery += aichRootHash(data.data(), data.size());
  return true;
}

bool appendAichLowestLevel(std::string& recovery, const SharedSource& source,
                           int64_t nodeBegin, size_t nodeSize, size_t nodeBase,
                           bool leftBranch, uint32_t ident, bool use32BitIdent)
{
  const auto nextIdent = (ident << 1) | (leftBranch ? 1 : 0);
  if (nodeSize <= nodeBase) {
    return appendAichHashForRange(recovery, source, nextIdent, nodeBegin,
                                  nodeSize, use32BitIdent);
  }

  const auto nodeBlocks = countBlocks(nodeSize, nodeBase);
  const auto leftBlocks = (leftBranch ? nodeBlocks + 1 : nodeBlocks) / 2;
  const auto leftSize = std::min(nodeSize, leftBlocks * nodeBase);
  const auto rightSize = nodeSize - leftSize;
  const auto leftBase = leftSize <= static_cast<size_t>(PIECE_LENGTH)
                            ? EMBLOCK_LENGTH
                            : PIECE_LENGTH;
  const auto rightBase = rightSize <= static_cast<size_t>(PIECE_LENGTH)
                             ? EMBLOCK_LENGTH
                             : PIECE_LENGTH;
  return appendAichLowestLevel(recovery, source, nodeBegin, leftSize, leftBase,
                               true, nextIdent, use32BitIdent) &&
         appendAichLowestLevel(recovery, source, nodeBegin + leftSize,
                               rightSize, rightBase, false, nextIdent,
                               use32BitIdent);
}

bool appendAichPartRecovery(std::string& recovery, const SharedSource& source,
                            int64_t nodeBegin, size_t nodeSize, size_t nodeBase,
                            bool leftBranch, int64_t targetBegin,
                            size_t targetSize, uint32_t ident,
                            bool use32BitIdent)
{
  if (targetBegin == nodeBegin && targetSize == nodeSize) {
    return appendAichLowestLevel(recovery, source, nodeBegin, nodeSize,
                                 nodeBase, leftBranch, ident, use32BitIdent);
  }
  if (nodeSize <= nodeBase) {
    return false;
  }

  const auto nextIdent = (ident << 1) | (leftBranch ? 1 : 0);
  const auto nodeBlocks = countBlocks(nodeSize, nodeBase);
  const auto leftBlocks = (leftBranch ? nodeBlocks + 1 : nodeBlocks) / 2;
  const auto leftSize = std::min(nodeSize, leftBlocks * nodeBase);
  const auto rightSize = nodeSize - leftSize;
  const auto leftBase = leftSize <= static_cast<size_t>(PIECE_LENGTH)
                            ? EMBLOCK_LENGTH
                            : PIECE_LENGTH;
  const auto rightBase = rightSize <= static_cast<size_t>(PIECE_LENGTH)
                             ? EMBLOCK_LENGTH
                             : PIECE_LENGTH;
  if (targetBegin < nodeBegin + static_cast<int64_t>(leftSize)) {
    if (targetBegin + static_cast<int64_t>(targetSize) >
        nodeBegin + static_cast<int64_t>(leftSize)) {
      return false;
    }
    if (!appendAichHashForRange(recovery, source, nextIdent << 1,
                                nodeBegin + leftSize, rightSize,
                                use32BitIdent)) {
      return false;
    }
    return appendAichPartRecovery(recovery, source, nodeBegin, leftSize,
                                  leftBase, true, targetBegin, targetSize,
                                  nextIdent, use32BitIdent);
  }
  if (!appendAichHashForRange(recovery, source, (nextIdent << 1) | 1, nodeBegin,
                              leftSize, use32BitIdent)) {
    return false;
  }
  return appendAichPartRecovery(recovery, source, nodeBegin + leftSize,
                                rightSize, rightBase, false, targetBegin,
                                targetSize, nextIdent, use32BitIdent);
}

} // namespace

std::string createSharedFileNameAnswerPayload(const SharedSource& source)
{
  return source.hash() + createUInt16String(source.name());
}

std::string createSharedFileStatusPayload(const SharedSource& source)
{
  return createFileStatusPayload(source.hash(), source.bitfield());
}

bool createSharedFileHashSetPayload(std::string& payload,
                                    const SharedSource& source)
{
  const auto expectedHashCount = hashSetPartCount(source.size());
  if (expectedHashCount == 0) {
    payload =
        createHashSetAnswerPayload(source.hash(), std::vector<std::string>());
    return true;
  }
  if (source.pieceHashes().size() != expectedHashCount) {
    return false;
  }
  payload = createHashSetAnswerPayload(source.hash(), source.pieceHashes());
  return true;
}

bool createSharedFilePartPayload(std::string& payload,
                                 const SharedSource& source,
                                 const PartRange& range, bool use64BitOffsets)
{
  std::string data;
  if (!source.readRange(data, range.begin, range.end)) {
    return false;
  }
  payload = source.hash();
  if (use64BitOffsets) {
    payload += packUInt64(range.begin);
    payload += packUInt64(range.end);
  }
  else {
    if (range.begin > std::numeric_limits<uint32_t>::max() ||
        range.end > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    payload += packUInt32(static_cast<uint32_t>(range.begin));
    payload += packUInt32(static_cast<uint32_t>(range.end));
  }
  payload += data;
  return true;
}

bool createSharedFileAichAnswerPayload(std::string& payload,
                                       const SharedSource& source,
                                       uint16_t partIndex,
                                       const std::string& rootHash)
{
  if (!source.complete() || source.aichRootHash().empty() ||
      source.aichRootHash() != rootHash) {
    return false;
  }
  const auto partBegin =
      static_cast<int64_t>(partIndex) * static_cast<int64_t>(PIECE_LENGTH);
  if (partBegin >= source.size()) {
    return false;
  }
  const auto partSize = static_cast<size_t>(
      std::min<int64_t>(PIECE_LENGTH, source.size() - partBegin));
  if (partSize <= EMBLOCK_LENGTH) {
    return false;
  }
  const auto use32BitIdent =
      source.size() > std::numeric_limits<uint32_t>::max();
  const auto fileBase =
      source.size() <= PIECE_LENGTH ? EMBLOCK_LENGTH : PIECE_LENGTH;
  std::string recoveryHashes;
  if (!appendAichPartRecovery(recoveryHashes, source, 0,
                              static_cast<size_t>(source.size()), fileBase,
                              true, partBegin, partSize, 0, use32BitIdent)) {
    return false;
  }
  const auto entrySize = AICH_HASH_LENGTH + (use32BitIdent ? 4 : 2);
  if (recoveryHashes.size() % entrySize != 0) {
    return false;
  }
  const auto hashCount = recoveryHashes.size() / entrySize;
  if (hashCount == 0 || hashCount > std::numeric_limits<uint16_t>::max()) {
    return false;
  }
  std::string recovery;
  if (use32BitIdent) {
    recovery += packUInt16(0);
  }
  recovery += packUInt16(static_cast<uint16_t>(hashCount));
  recovery += recoveryHashes;
  if (!use32BitIdent) {
    recovery += packUInt16(0);
  }
  AichRecoveryData parsed;
  if (!parseAichRecoveryData(parsed, recovery, partSize, use32BitIdent) ||
      !verifyAichRecoveryData(parsed, rootHash,
                              static_cast<size_t>(source.size()), partIndex)) {
    return false;
  }
  payload =
      createAichAnswerPayload(source.hash(), partIndex, rootHash, recovery);
  return true;
}

bool parsePartRequestPayload(std::vector<PartRange>& ranges,
                             std::string& fileHash, const std::string& payload,
                             bool use64BitOffsets)
{
  const auto offsetSize = use64BitOffsets ? 8 : 4;
  const auto expectedSize = HASH_LENGTH + offsetSize * 6;
  if (payload.size() != expectedSize) {
    return false;
  }
  size_t offset = 0;
  fileHash = readBytes(payload, offset, HASH_LENGTH);
  ranges.clear();
  int64_t begins[3] = {0, 0, 0};
  int64_t ends[3] = {0, 0, 0};
  for (auto& begin : begins) {
    begin = use64BitOffsets ? readUInt64(readBytes(payload, offset, 8).data())
                            : readUInt32(readBytes(payload, offset, 4).data());
  }
  for (auto& end : ends) {
    end = use64BitOffsets ? readUInt64(readBytes(payload, offset, 8).data())
                          : readUInt32(readBytes(payload, offset, 4).data());
  }
  for (size_t i = 0; i < 3; ++i) {
    if (begins[i] == 0 && ends[i] == 0) {
      continue;
    }
    if (ends[i] <= begins[i]) {
      return false;
    }
    PartRange range;
    range.begin = begins[i];
    range.end = ends[i];
    ranges.push_back(range);
  }
  return true;
}

} // namespace aria2::ed2k
