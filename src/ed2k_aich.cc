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
#include "ed2k_aich.h"

#include <algorithm>

#include "DlAbortEx.h"
#include "ed2k_hash.h"
#include "ed2k_packet.h"

namespace aria2 {

namespace ed2k {

namespace {

void validateHashLength(const std::string& hash)
{
  if (hash.size() != HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K hash length.");
  }
}

void validateAichHashLength(const std::string& hash)
{
  if (hash.size() != AICH_HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K AICH hash length.");
  }
}

size_t countBlocks(size_t length, size_t baseSize)
{
  return (length + baseSize - 1) / baseSize;
}

size_t aichPartSize(size_t fileSize, size_t partIndex)
{
  const auto begin = partIndex * static_cast<size_t>(PIECE_LENGTH);
  if (begin >= fileSize) {
    return 0;
  }
  return std::min(static_cast<size_t>(PIECE_LENGTH), fileSize - begin);
}

uint32_t firstHashBit(uint32_t ident)
{
  uint32_t bit = 0x80000000u;
  while (bit != 0 && (ident & bit) == 0) {
    bit >>= 1;
  }
  return bit;
}

bool identifierPathReachesRange(uint32_t ident, size_t targetOffset,
                                size_t targetSize, size_t dataSize,
                                size_t baseSize, bool leftBranch)
{
  if (ident == 0 || dataSize == 0 || targetSize == 0 ||
      targetOffset + targetSize > dataSize) {
    return false;
  }

  uint32_t bit = firstHashBit(ident);
  if (bit == 0) {
    return false;
  }
  if (bit == 1) {
    return targetOffset == 0 && targetSize == dataSize;
  }
  bit >>= 1;

  size_t offset = 0;
  size_t length = dataSize;
  size_t currentBase = baseSize;
  bool currentLeft = leftBranch;
  while (bit != 0 && length > currentBase) {
    const auto blocks = countBlocks(length, currentBase);
    const auto leftBlocks = (currentLeft ? blocks + 1 : blocks) / 2;
    const auto leftLength = std::min(length, leftBlocks * currentBase);
    const auto rightLength = length - leftLength;
    if ((ident & bit) != 0) {
      length = leftLength;
      currentLeft = true;
    }
    else {
      offset += leftLength;
      length = rightLength;
      currentLeft = false;
    }
    currentBase = length <= static_cast<size_t>(PIECE_LENGTH)
                      ? EMBLOCK_LENGTH
                      : PIECE_LENGTH;
    bit >>= 1;
  }
  return offset == targetOffset && length == targetSize && bit == 0;
}

std::string hashForRange(const AichRecoveryData& recovery, size_t targetOffset,
                         size_t targetSize, size_t dataSize, size_t baseSize,
                         bool leftBranch)
{
  for (const auto& item : recovery.hashes16) {
    if (identifierPathReachesRange(item.identifier, targetOffset, targetSize,
                                   dataSize, baseSize, leftBranch)) {
      return item.hash;
    }
  }
  for (const auto& item : recovery.hashes32) {
    if (identifierPathReachesRange(item.identifier, targetOffset, targetSize,
                                   dataSize, baseSize, leftBranch)) {
      return item.hash;
    }
  }
  return std::string();
}

std::string recoveryRootForRange(const AichRecoveryData& recovery,
                                 size_t targetOffset, size_t targetSize,
                                 size_t dataSize, size_t rootBaseSize,
                                 size_t nodeBaseSize, bool leftBranch)
{
  auto known = hashForRange(recovery, targetOffset, targetSize, dataSize,
                            rootBaseSize, true);
  if (!known.empty()) {
    return known;
  }
  if (targetSize <= nodeBaseSize) {
    return std::string();
  }

  const auto blocks = countBlocks(targetSize, nodeBaseSize);
  const auto leftBlocks = (leftBranch ? blocks + 1 : blocks) / 2;
  const auto leftSize = std::min(targetSize, leftBlocks * nodeBaseSize);
  const auto rightSize = targetSize - leftSize;
  const auto leftBase = leftSize <= static_cast<size_t>(PIECE_LENGTH)
                            ? EMBLOCK_LENGTH
                            : PIECE_LENGTH;
  const auto rightBase = rightSize <= static_cast<size_t>(PIECE_LENGTH)
                             ? EMBLOCK_LENGTH
                             : PIECE_LENGTH;
  const auto leftHash = recoveryRootForRange(
      recovery, targetOffset, leftSize, dataSize, rootBaseSize, leftBase,
      true);
  const auto rightHash = recoveryRootForRange(
      recovery, targetOffset + leftSize, rightSize, dataSize, rootBaseSize,
      rightBase, false);
  if (leftHash.empty() || rightHash.empty()) {
    return std::string();
  }
  return aichHash(leftHash + rightHash);
}

} // namespace

std::string createAichFileHashRequestPayload(const std::string& fileHash)
{
  validateHashLength(fileHash);
  return fileHash;
}

std::string createAichFileHashAnswerPayload(const std::string& fileHash,
                                            const std::string& rootHash)
{
  validateHashLength(fileHash);
  validateAichHashLength(rootHash);
  return fileHash + rootHash;
}

bool parseAichFileHashAnswerPayload(AichFileHashAnswer& answer,
                                    const std::string& payload,
                                    const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  if (payload.size() != HASH_LENGTH + AICH_HASH_LENGTH ||
      payload.substr(0, HASH_LENGTH) != expectedFileHash) {
    return false;
  }
  answer.fileHash = payload.substr(0, HASH_LENGTH);
  answer.rootHash = payload.substr(HASH_LENGTH, AICH_HASH_LENGTH);
  return true;
}

std::string createAichRequestPayload(const std::string& fileHash,
                                     uint16_t partIndex,
                                     const std::string& rootHash)
{
  validateHashLength(fileHash);
  validateAichHashLength(rootHash);
  return fileHash + packUInt16(partIndex) + rootHash;
}

bool parseAichRequestPayload(AichRequest& request, const std::string& payload,
                             const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  if (payload.size() != HASH_LENGTH + 2 + AICH_HASH_LENGTH ||
      payload.substr(0, HASH_LENGTH) != expectedFileHash) {
    return false;
  }
  request.fileHash = payload.substr(0, HASH_LENGTH);
  request.partIndex = readUInt16(payload.data() + HASH_LENGTH);
  request.rootHash = payload.substr(HASH_LENGTH + 2, AICH_HASH_LENGTH);
  return true;
}

std::string createAichAnswerPayload(const std::string& fileHash,
                                    uint16_t partIndex,
                                    const std::string& rootHash,
                                    const std::string& recoveryData)
{
  return createAichRequestPayload(fileHash, partIndex, rootHash) +
         recoveryData;
}

bool parseAichAnswerPayload(AichAnswer& answer, const std::string& payload,
                            const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  if (payload.size() == HASH_LENGTH &&
      payload.substr(0, HASH_LENGTH) == expectedFileHash) {
    answer.failed = true;
    answer.fileHash = payload;
    answer.partIndex = 0;
    answer.rootHash.clear();
    answer.recoveryData.clear();
    return true;
  }
  if (payload.size() < HASH_LENGTH + 2 + AICH_HASH_LENGTH ||
      payload.substr(0, HASH_LENGTH) != expectedFileHash) {
    return false;
  }
  answer.failed = false;
  answer.fileHash = payload.substr(0, HASH_LENGTH);
  answer.partIndex = readUInt16(payload.data() + HASH_LENGTH);
  answer.rootHash = payload.substr(HASH_LENGTH + 2, AICH_HASH_LENGTH);
  answer.recoveryData.assign(payload.begin() + HASH_LENGTH + 2 +
                                 AICH_HASH_LENGTH,
                             payload.end());
  return true;
}

bool parseAichRecoveryData(AichRecoveryData& recovery,
                           const std::string& payload, size_t partSize,
                           bool largeFile)
{
  if (partSize <= EMBLOCK_LENGTH) {
    return false;
  }
  size_t offset = 0;
  recovery.hashes16.clear();
  recovery.hashes32.clear();
  try {
    const auto count16 = readUInt16(readBytes(payload, offset, 2).data());
    if (count16 != 0) {
      recovery.hashes16.reserve(count16);
      for (uint16_t i = 0; i < count16; ++i) {
        AichRecoveryHash item;
        item.identifier = readUInt16(readBytes(payload, offset, 2).data());
        if (item.identifier == 1 || item.identifier > 0x400000u) {
          return false;
        }
        item.hash = readBytes(payload, offset, AICH_HASH_LENGTH);
        recovery.hashes16.push_back(item);
      }
    }

    if (offset < payload.size()) {
      const auto count32 = readUInt16(readBytes(payload, offset, 2).data());
      if (count32 != 0) {
        if (!largeFile) {
          return false;
        }
        recovery.hashes32.reserve(count32);
        for (uint16_t i = 0; i < count32; ++i) {
          AichRecoveryHash item;
          item.identifier = readUInt32(readBytes(payload, offset, 4).data());
          if (item.identifier == 1 || item.identifier > 0x400000u) {
            return false;
          }
          item.hash = readBytes(payload, offset, AICH_HASH_LENGTH);
          recovery.hashes32.push_back(item);
        }
      }
    }
  }
  catch (RecoverableException&) {
    return false;
  }
  return offset == payload.size() &&
         (!recovery.hashes16.empty() || !recovery.hashes32.empty());
}

bool verifyAichRecoveryData(const AichRecoveryData& recovery,
                            const std::string& rootHash, size_t fileSize,
                            size_t partIndex)
{
  AichRecoverySet recoverySet;
  return buildAichRecoverySet(recoverySet, recovery, rootHash, fileSize,
                              partIndex);
}

bool buildAichRecoverySet(AichRecoverySet& recoverySet,
                          const AichRecoveryData& recovery,
                          const std::string& rootHash, size_t fileSize,
                          size_t partIndex)
{
  validateAichHashLength(rootHash);
  const auto partSize = aichPartSize(fileSize, partIndex);
  if (partSize <= EMBLOCK_LENGTH) {
    return false;
  }
  const auto fileBase = fileSize <= static_cast<size_t>(PIECE_LENGTH)
                            ? EMBLOCK_LENGTH
                            : PIECE_LENGTH;
  const auto partOffset = partIndex * static_cast<size_t>(PIECE_LENGTH);
  const auto partHash = recoveryRootForRange(
      recovery, partOffset, partSize, fileSize, fileBase, fileBase, true);
  if (partHash.empty()) {
    return false;
  }
  AichRecoverySet verified;
  verified.partIndex = partIndex;
  for (size_t offset = 0; offset < partSize; offset += EMBLOCK_LENGTH) {
    const auto blockSize = std::min<size_t>(EMBLOCK_LENGTH, partSize - offset);
    auto hash = hashForRange(recovery, partOffset + offset, blockSize,
                             fileSize, fileBase, true);
    if (hash.empty()) {
      return false;
    }
    AichRecoveryBlock block;
    block.offset = offset;
    block.length = blockSize;
    block.hash = std::move(hash);
    verified.blocks.push_back(std::move(block));
  }
  if (recoveryRootForRange(recovery, 0, fileSize, fileSize, fileBase, fileBase,
                           true) != rootHash) {
    return false;
  }
  recoverySet = std::move(verified);
  return true;
}

} // namespace ed2k

} // namespace aria2
