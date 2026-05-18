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
constexpr uint32_t PEER_SOURCE_INCOMING = 1u << 0;
constexpr uint32_t PEER_SOURCE_SERVER = 1u << 1;
constexpr uint32_t PEER_SOURCE_KAD = 1u << 2;
constexpr uint32_t PEER_SOURCE_RESUME = 1u << 3;
constexpr uint32_t PEER_SOURCE_EXCHANGE = 1u << 4;
constexpr uint32_t PEER_SOURCE_INLINE = 1u << 5;

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
  std::string userHash;
  uint8_t version = 0;
  uint8_t protocolVersion = 0;
  EmuleMiscOptions miscOptions;
  EmuleMiscOptions2 miscOptions2;
};

struct UdpReask {
  std::string fileHash;
  uint16_t completeSources = 0;
  bool hasCompleteSources = false;
};

struct UdpReaskAck {
  std::vector<bool> bitfield;
  uint16_t rank = 0;
};

struct PeerState {
  Endpoint endpoint;
  std::vector<bool> partStatus;
  std::vector<PartRange> requestedParts;
  uint32_t sourceFlags = 0;
  bool connecting = false;
  bool queued = false;
  uint16_t queueRank = 0;
  bool dead = false;
  bool accepted = false;
  bool outOfParts = false;
  bool cancelled = false;
  bool noFile = false;
  uint32_t failCount = 0;
  int64_t lastFailureTime = 0;
  int64_t nextRetryTime = 0;
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
std::string createQueueRankPayload(uint32_t rank);
bool parseQueueRankPayload(uint16_t& rank, const std::string& payload);
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
bool parsePeerHelloUserHash(std::string& userHash,
                            const std::string& payload,
                            bool helloPacket);
std::string createUdpReaskFilePingPayload(const std::string& fileHash,
                                          uint16_t completeSources = 0);
bool parseUdpReaskFilePingPayload(UdpReask& reask,
                                  const std::string& payload);
std::string createUdpReaskAckPayload(uint16_t rank);
std::string createUdpReaskAckPayload(const std::vector<bool>& bitfield,
                                     uint16_t rank);
bool parseUdpReaskAckPayload(UdpReaskAck& ack, const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_PEER_H
