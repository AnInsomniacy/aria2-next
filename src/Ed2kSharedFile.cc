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
#include "Ed2kSharedFile.h"

#include <algorithm>
#include <limits>

#include "DefaultDiskWriter.h"
#include "DiskWriter.h"
#include "DlAbortEx.h"
#include "Ed2kSharedStore.h"
#include "ed2k_aich.h"
#include "ed2k_hash.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"

namespace aria2 {

namespace ed2k {

namespace {

void validateSharedFile(const SharedFile& file)
{
  if (file.hash.size() != HASH_LENGTH || file.size <= 0 || file.name.empty() ||
      file.path.empty() || !file.completed) {
    throw DL_ABORT_EX("Bad ED2K shared file.");
  }
}

bool checkedRange(const SharedFile& file, int64_t begin, int64_t end)
{
  return file.completed && begin >= 0 && end > begin && end <= file.size &&
         end - begin <= BLOCK_LENGTH;
}

std::string createUInt16String(const std::string& value)
{
  if (value.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K string is too large.");
  }
  return packUInt16(static_cast<uint16_t>(value.size())) + value;
}

size_t countBlocks(size_t length, size_t baseSize)
{
  return (length + baseSize - 1) / baseSize;
}

bool readSharedFileHash(std::string& hash, const SharedFile& file,
                        int64_t begin, size_t length)
{
  std::string data;
  if (!readSharedFileRange(data, file, begin, begin + length)) {
    return false;
  }
  hash = aichRootHash(data.data(), data.size());
  return true;
}

bool appendAichHashForRange(std::string& recovery, const SharedFile& file,
                            uint32_t ident, int64_t begin, size_t length,
                            bool use32BitIdent)
{
  std::string hash;
  if (!readSharedFileHash(hash, file, begin, length)) {
    return false;
  }
  recovery += use32BitIdent ? packUInt32(ident) : packUInt16(ident);
  recovery += hash;
  return true;
}

bool appendAichLowestLevel(std::string& recovery, const SharedFile& file,
                           int64_t nodeBegin, size_t nodeSize,
                           size_t nodeBase, bool leftBranch,
                           uint32_t ident, bool use32BitIdent)
{
  const auto nextIdent = (ident << 1) | (leftBranch ? 1 : 0);
  if (nodeSize <= nodeBase) {
    return appendAichHashForRange(recovery, file, nextIdent, nodeBegin,
                                  nodeSize, use32BitIdent);
  }

  const auto nodeBlocks = countBlocks(nodeSize, nodeBase);
  const auto leftBlocks = (leftBranch ? nodeBlocks + 1 : nodeBlocks) / 2;
  const auto leftSize = std::min(nodeSize, leftBlocks * nodeBase);
  const auto rightSize = nodeSize - leftSize;
  const auto leftBase =
      leftSize <= static_cast<size_t>(PIECE_LENGTH) ? EMBLOCK_LENGTH
                                                    : PIECE_LENGTH;
  const auto rightBase =
      rightSize <= static_cast<size_t>(PIECE_LENGTH) ? EMBLOCK_LENGTH
                                                     : PIECE_LENGTH;
  return appendAichLowestLevel(recovery, file, nodeBegin, leftSize, leftBase,
                               true, nextIdent, use32BitIdent) &&
         appendAichLowestLevel(recovery, file, nodeBegin + leftSize, rightSize,
                               rightBase, false, nextIdent, use32BitIdent);
}

bool appendAichPartRecovery(std::string& recovery, const SharedFile& file,
                            int64_t nodeBegin, size_t nodeSize,
                            size_t nodeBase, bool leftBranch,
                            int64_t targetBegin, size_t targetSize,
                            uint32_t ident, bool use32BitIdent)
{
  if (targetBegin == nodeBegin && targetSize == nodeSize) {
    return appendAichLowestLevel(recovery, file, nodeBegin, nodeSize, nodeBase,
                                 leftBranch, ident, use32BitIdent);
  }
  if (nodeSize <= nodeBase) {
    return false;
  }

  const auto nextIdent = (ident << 1) | (leftBranch ? 1 : 0);
  const auto nodeBlocks = countBlocks(nodeSize, nodeBase);
  const auto leftBlocks = (leftBranch ? nodeBlocks + 1 : nodeBlocks) / 2;
  const auto leftSize = std::min(nodeSize, leftBlocks * nodeBase);
  const auto rightSize = nodeSize - leftSize;
  const auto leftBase =
      leftSize <= static_cast<size_t>(PIECE_LENGTH) ? EMBLOCK_LENGTH
                                                    : PIECE_LENGTH;
  const auto rightBase =
      rightSize <= static_cast<size_t>(PIECE_LENGTH) ? EMBLOCK_LENGTH
                                                     : PIECE_LENGTH;
  if (targetBegin < nodeBegin + static_cast<int64_t>(leftSize)) {
    if (targetBegin + static_cast<int64_t>(targetSize) >
        nodeBegin + static_cast<int64_t>(leftSize)) {
      return false;
    }
    if (!appendAichHashForRange(recovery, file, nextIdent << 1,
                                nodeBegin + leftSize, rightSize,
                                use32BitIdent)) {
      return false;
    }
    return appendAichPartRecovery(recovery, file, nodeBegin, leftSize,
                                  leftBase, true, targetBegin, targetSize,
                                  nextIdent, use32BitIdent);
  }
  if (!appendAichHashForRange(recovery, file, (nextIdent << 1) | 1,
                              nodeBegin, leftSize, use32BitIdent)) {
    return false;
  }
  return appendAichPartRecovery(
      recovery, file, nodeBegin + leftSize, rightSize, rightBase, false,
      targetBegin, targetSize, nextIdent, use32BitIdent);
}

} // namespace

std::vector<bool> createSharedFileBitfield(const SharedFile& file)
{
  validateSharedFile(file);
  auto count = static_cast<size_t>((file.size + PIECE_LENGTH - 1) /
                                   PIECE_LENGTH);
  return std::vector<bool>(count, true);
}

std::string createSharedFileNameAnswerPayload(const SharedFile& file)
{
  validateSharedFile(file);
  return file.hash + createUInt16String(file.name);
}

std::string createSharedFileStatusPayload(const SharedFile& file)
{
  return createFileStatusPayload(file.hash, createSharedFileBitfield(file));
}

bool createSharedFileHashSetPayload(std::string& payload,
                                    const SharedFile& file)
{
  validateSharedFile(file);
  const auto expectedHashCount = hashSetPartCount(file.size);
  if (expectedHashCount == 0) {
    payload = createHashSetAnswerPayload(file.hash, std::vector<std::string>());
    return true;
  }
  if (!file.pieceHashes.empty()) {
    if (file.pieceHashes.size() != expectedHashCount) {
      return false;
    }
    payload = createHashSetAnswerPayload(file.hash, file.pieceHashes);
    return true;
  }
  return false;
}

bool readSharedFileRange(std::string& data, const SharedFile& file,
                         int64_t begin, int64_t end)
{
  if (!checkedRange(file, begin, end)) {
    return false;
  }
  const auto length = static_cast<size_t>(end - begin);
  std::shared_ptr<DiskWriter> writer(new DefaultDiskWriter(file.path));
  writer->enableReadOnly();
  writer->openExistingFile();
  data.assign(length, '\0');
  const auto read = writer->readData(
      reinterpret_cast<unsigned char*>(&data[0]), length, begin);
  return read == static_cast<ssize_t>(length);
}

bool createSharedFilePartPayload(std::string& payload, const SharedFile& file,
                                 const PartRange& range,
                                 bool use64BitOffsets)
{
  std::string data;
  if (!readSharedFileRange(data, file, range.begin, range.end)) {
    return false;
  }
  payload = file.hash;
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
                                       const SharedFile& file,
                                       uint16_t partIndex,
                                       const std::string& rootHash)
{
  validateSharedFile(file);
  if (file.aichRootHash.empty() || file.aichRootHash != rootHash) {
    return false;
  }
  const auto partBegin =
      static_cast<int64_t>(partIndex) * static_cast<int64_t>(PIECE_LENGTH);
  if (partBegin >= file.size) {
    return false;
  }
  const auto partSize = static_cast<size_t>(
      std::min<int64_t>(PIECE_LENGTH, file.size - partBegin));
  if (partSize <= EMBLOCK_LENGTH) {
    return false;
  }
  const auto use32BitIdent = file.size > std::numeric_limits<uint32_t>::max();
  const auto fileBase =
      file.size <= PIECE_LENGTH ? EMBLOCK_LENGTH : PIECE_LENGTH;
  std::string recoveryHashes;
  if (!appendAichPartRecovery(recoveryHashes, file, 0,
                              static_cast<size_t>(file.size), fileBase, true,
                              partBegin, partSize, 0, use32BitIdent)) {
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
                              static_cast<size_t>(file.size), partIndex)) {
    return false;
  }
  payload = createAichAnswerPayload(file.hash, partIndex, rootHash, recovery);
  return true;
}

bool parsePartRequestPayload(std::vector<PartRange>& ranges,
                             std::string& fileHash,
                             const std::string& payload,
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

} // namespace ed2k

} // namespace aria2
