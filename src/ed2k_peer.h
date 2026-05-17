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
#ifndef D_ED2K_PEER_H
#define D_ED2K_PEER_H

#include "common.h"
#include "ed2k_link.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

namespace ed2k {

constexpr uint8_t SOURCE_EXCHANGE2_VERSION = 4;

struct PartRange {
  int64_t begin = 0;
  int64_t end = 0;
};

struct SourceExchangeEntry {
  Endpoint endpoint;
  Endpoint server;
  std::string userHash;
  uint8_t cryptOptions = 0;
};

struct SourceExchangeAnswer {
  uint8_t version = 0;
  std::vector<SourceExchangeEntry> entries;
};

struct EmuleMiscOptions {
  uint8_t aichVersion = 0;
  bool unicode = false;
  uint8_t udpVersion = 0;
  uint8_t dataCompressionVersion = 0;
  uint8_t secureIdentVersion = 0;
  uint8_t sourceExchange1Version = 0;
  uint8_t extendedRequestsVersion = 0;
  bool multiPacket = false;
};

struct EmuleMiscOptions2 {
  bool supportsLargeFiles = false;
  bool supportsExtendedMultipacket = false;
  bool supportsSourceExchange2 = false;
};

struct EmulePeerInfo {
  uint8_t version = 0;
  uint8_t protocolVersion = 0;
  EmuleMiscOptions miscOptions;
  EmuleMiscOptions2 miscOptions2;
};

std::string createFileStatusPayload(const std::string& fileHash,
                                    const std::vector<bool>& bitfield);
bool parseFileStatusPayload(std::vector<bool>& bitfield,
                            const std::string& payload,
                            const std::string& expectedFileHash);
std::string createHashSetAnswerPayload(
    const std::string& fileHash, const std::vector<std::string>& pieceHashes);
bool parseHashSetAnswerPayload(std::vector<std::string>& pieceHashes,
                               const std::string& payload,
                               const std::string& expectedFileHash);
std::string createRequestPartsPayload(const std::string& fileHash,
                                      const std::vector<PartRange>& ranges,
                                      bool use64BitOffsets);
std::string createRequestSources2Payload(const std::string& fileHash);
bool parseRequestSources2Payload(uint8_t& version, const std::string& payload,
                                 const std::string& expectedFileHash);
std::string createAnswerSourcesPayload(
    const std::string& fileHash, uint8_t version,
    const std::vector<SourceExchangeEntry>& entries);
bool parseAnswerSourcesPayload(SourceExchangeAnswer& answer,
                               const std::string& payload,
                               const std::string& expectedFileHash,
                               uint8_t sourceExchangeVersion);
std::string createAnswerSources2Payload(
    const std::string& fileHash, uint8_t version,
    const std::vector<SourceExchangeEntry>& entries);
std::string createAnswerSources2Payload(
    const std::string& fileHash,
    const std::vector<SourceExchangeEntry>& entries);
bool parseAnswerSources2Payload(SourceExchangeAnswer& answer,
                                const std::string& payload,
                                const std::string& expectedFileHash);
std::string createEmuleInfoPayload(const EmulePeerInfo& info);
bool parseEmuleInfoPayload(EmulePeerInfo& info, const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_PEER_H
