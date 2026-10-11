/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace aria2::ed2k {

struct AichFileHashAnswer {
  std::string fileHash;
  std::string rootHash;
};

struct AichRequest {
  std::string fileHash;
  uint16_t partIndex = 0;
  std::string rootHash;
};

struct AichAnswer {
  bool failed = false;
  std::string fileHash;
  uint16_t partIndex = 0;
  std::string rootHash;
  std::string recoveryData;
};

struct AichRecoveryHash {
  uint32_t identifier = 0;
  std::string hash;
};

struct AichRecoveryData {
  std::vector<AichRecoveryHash> hashes16;
  std::vector<AichRecoveryHash> hashes32;
};

struct AichRecoveryBlock {
  size_t offset = 0;
  size_t length = 0;
  std::string hash;
};

struct AichRecoverySet {
  size_t partIndex = 0;
  std::vector<AichRecoveryBlock> blocks;
};

std::string createAichFileHashRequestPayload(const std::string& fileHash);
std::string createAichFileHashAnswerPayload(const std::string& fileHash,
                                            const std::string& rootHash);
bool parseAichFileHashAnswerPayload(AichFileHashAnswer& answer,
                                    const std::string& payload,
                                    const std::string& expectedFileHash);
std::string createAichRequestPayload(const std::string& fileHash,
                                     uint16_t partIndex,
                                     const std::string& rootHash);
bool parseAichRequestPayload(AichRequest& request, const std::string& payload,
                             const std::string& expectedFileHash);
std::string createAichAnswerPayload(const std::string& fileHash,
                                    uint16_t partIndex,
                                    const std::string& rootHash,
                                    const std::string& recoveryData);
bool parseAichAnswerPayload(AichAnswer& answer, const std::string& payload,
                            const std::string& expectedFileHash);
bool parseAichRecoveryData(AichRecoveryData& recovery,
                           const std::string& payload, size_t partSize,
                           bool largeFile);
bool verifyAichRecoveryData(const AichRecoveryData& recovery,
                            const std::string& rootHash, size_t fileSize,
                            size_t partIndex);
bool buildAichRecoverySet(AichRecoverySet& recoverySet,
                          const AichRecoveryData& recovery,
                          const std::string& rootHash, size_t fileSize,
                          size_t partIndex);

} // namespace aria2::ed2k
