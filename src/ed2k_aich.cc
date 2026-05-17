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

} // namespace ed2k

} // namespace aria2
