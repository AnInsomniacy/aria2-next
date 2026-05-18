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
#include "ed2k_peer.h"

#include <limits>

#include "DlAbortEx.h"
#include "ed2k_endpoint.h"
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

uint32_t emuleMiscOptionsValue(const EmuleMiscOptions& options)
{
  return ((options.aichVersion & 0x07u) << 29) |
         (static_cast<uint32_t>(options.unicode) << 28) |
         ((options.udpVersion & 0x0fu) << 24) |
         ((options.dataCompressionVersion & 0x0fu) << 20) |
         ((options.secureIdentVersion & 0x0fu) << 16) |
         ((options.sourceExchange1Version & 0x0fu) << 12) |
         ((options.extendedRequestsVersion & 0x0fu) << 8) |
         (static_cast<uint32_t>(options.multiPacket) << 1);
}

EmuleMiscOptions parseEmuleMiscOptions(uint32_t value)
{
  EmuleMiscOptions options;
  options.aichVersion = (value >> 29) & 0x07;
  options.unicode = (value >> 28) & 0x01;
  options.udpVersion = (value >> 24) & 0x0f;
  options.dataCompressionVersion = (value >> 20) & 0x0f;
  options.secureIdentVersion = (value >> 16) & 0x0f;
  options.sourceExchange1Version = (value >> 12) & 0x0f;
  options.extendedRequestsVersion = (value >> 8) & 0x0f;
  options.multiPacket = (value >> 1) & 0x01;
  return options;
}

uint32_t emuleMiscOptions2Value(const EmuleMiscOptions2& options)
{
  return (static_cast<uint32_t>(options.supportsLargeFiles) << 4) |
         (static_cast<uint32_t>(options.supportsExtendedMultipacket) << 5) |
         (static_cast<uint32_t>(options.supportsSourceExchange2) << 10);
}

EmuleMiscOptions2 parseEmuleMiscOptions2(uint32_t value)
{
  EmuleMiscOptions2 options;
  options.supportsLargeFiles = (value >> 4) & 0x01;
  options.supportsExtendedMultipacket = (value >> 5) & 0x01;
  options.supportsSourceExchange2 = (value >> 10) & 0x01;
  return options;
}

} // namespace

std::string createFileStatusPayload(const std::string& fileHash,
                                    const std::vector<bool>& bitfield)
{
  validateHashLength(fileHash);
  if (bitfield.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K file status bitfield is too large.");
  }
  std::string payload = fileHash;
  payload += packUInt16(static_cast<uint16_t>(bitfield.size()));
  payload.append((bitfield.size() + 7) / 8, '\0');
  for (size_t i = 0; i < bitfield.size(); ++i) {
    if (bitfield[i]) {
      payload[HASH_LENGTH + 2 + i / 8] |= static_cast<char>(0x80 >> (i & 7));
    }
  }
  return payload;
}

bool parseFileStatusPayload(std::vector<bool>& bitfield,
                            const std::string& payload,
                            const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  size_t offset = 0;
  auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (hash != expectedFileHash) {
    return false;
  }
  auto bitCount = readUInt16(readBytes(payload, offset, 2).data());
  auto byteCount = (bitCount + 7) / 8;
  auto raw = readBytes(payload, offset, byteCount);
  bitfield.assign(bitCount, false);
  for (size_t i = 0; i < bitCount; ++i) {
    bitfield[i] = raw[i / 8] & static_cast<char>(0x80 >> (i & 7));
  }
  return true;
}

std::string createHashSetAnswerPayload(
    const std::string& fileHash, const std::vector<std::string>& pieceHashes)
{
  validateHashLength(fileHash);
  if (pieceHashes.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K hash set is too large.");
  }
  std::string payload = fileHash;
  payload += packUInt16(static_cast<uint16_t>(pieceHashes.size()));
  for (const auto& hash : pieceHashes) {
    validateHashLength(hash);
    payload += hash;
  }
  return payload;
}

bool parseHashSetAnswerPayload(std::vector<std::string>& pieceHashes,
                               const std::string& payload,
                               const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  size_t offset = 0;
  auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (hash != expectedFileHash) {
    return false;
  }
  auto count = readUInt16(readBytes(payload, offset, 2).data());
  pieceHashes.clear();
  pieceHashes.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    pieceHashes.push_back(readBytes(payload, offset, HASH_LENGTH));
  }
  return true;
}

std::string createRequestPartsPayload(const std::string& fileHash,
                                      const std::vector<PartRange>& ranges,
                                      bool use64BitOffsets)
{
  validateHashLength(fileHash);
  if (ranges.size() > 3) {
    throw DL_ABORT_EX("Too many ED2K part ranges.");
  }
  std::string payload = fileHash;
  for (size_t i = 0; i < 3; ++i) {
    auto value = i < ranges.size() ? ranges[i].begin : 0;
    if (value < 0) {
      throw DL_ABORT_EX("Bad ED2K part range.");
    }
    payload += use64BitOffsets ? packUInt64(value) : packUInt32(value);
  }
  for (size_t i = 0; i < 3; ++i) {
    auto value = i < ranges.size() ? ranges[i].end : 0;
    if (value < 0 || (i < ranges.size() && value <= ranges[i].begin)) {
      throw DL_ABORT_EX("Bad ED2K part range.");
    }
    payload += use64BitOffsets ? packUInt64(value) : packUInt32(value);
  }
  return payload;
}

std::string createQueueRankPayload(uint32_t rank)
{
  return packUInt32(rank);
}

bool parseQueueRankPayload(uint16_t& rank, const std::string& payload)
{
  if (payload.size() == 2) {
    rank = readUInt16(payload.data());
    return true;
  }
  if (payload.size() == 4) {
    const auto value = readUInt32(payload.data());
    if (value > std::numeric_limits<uint16_t>::max()) {
      return false;
    }
    rank = static_cast<uint16_t>(value);
    return true;
  }
  return false;
}

std::string createRequestSources2Payload(const std::string& fileHash)
{
  validateHashLength(fileHash);
  std::string payload;
  payload.push_back(static_cast<char>(SOURCE_EXCHANGE2_VERSION));
  payload += packUInt16(0);
  payload += fileHash;
  return payload;
}

bool parseRequestSources2Payload(uint8_t& version, const std::string& payload,
                                 const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  if (payload.size() == HASH_LENGTH) {
    version = 1;
    return payload == expectedFileHash;
  }
  if (payload.size() != HASH_LENGTH + 3) {
    return false;
  }
  size_t offset = 0;
  const auto parsedVersion = readByte(payload, offset);
  readUInt16(readBytes(payload, offset, 2).data());
  const auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (parsedVersion == 0 || parsedVersion > SOURCE_EXCHANGE2_VERSION ||
      hash != expectedFileHash) {
    return false;
  }
  version = parsedVersion;
  return true;
}

std::string createAnswerSourcesPayload(
    const std::string& fileHash, uint8_t version,
    const std::vector<SourceExchangeEntry>& entries)
{
  validateHashLength(fileHash);
  if (version == 0 || version > SOURCE_EXCHANGE2_VERSION) {
    throw DL_ABORT_EX("Bad ED2K source exchange version.");
  }
  if (entries.size() > 500) {
    throw DL_ABORT_EX("Too many ED2K source exchange entries.");
  }
  std::string payload = fileHash;
  payload += packUInt16(static_cast<uint16_t>(entries.size()));
  for (const auto& entry : entries) {
    payload += packEndpoint(entry.endpoint);
    payload += packEndpoint(entry.server);
    if (version >= 2) {
      validateHashLength(entry.userHash);
      payload += entry.userHash;
    }
    if (version >= 4) {
      payload.push_back(static_cast<char>(entry.cryptOptions));
    }
  }
  return payload;
}

std::string createAnswerSources2Payload(
    const std::string& fileHash, uint8_t version,
    const std::vector<SourceExchangeEntry>& entries)
{
  validateHashLength(fileHash);
  if (version == 0 || version > SOURCE_EXCHANGE2_VERSION) {
    throw DL_ABORT_EX("Bad ED2K source exchange version.");
  }
  if (entries.size() > 500) {
    throw DL_ABORT_EX("Too many ED2K source exchange entries.");
  }
  std::string payload;
  payload.push_back(static_cast<char>(version));
  payload += fileHash;
  payload += packUInt16(static_cast<uint16_t>(entries.size()));
  for (const auto& entry : entries) {
    payload += packEndpoint(entry.endpoint);
    payload += packEndpoint(entry.server);
    if (version >= 2) {
      validateHashLength(entry.userHash);
      payload += entry.userHash;
    }
    if (version >= 4) {
      payload.push_back(static_cast<char>(entry.cryptOptions));
    }
  }
  return payload;
}

std::string createAnswerSources2Payload(
    const std::string& fileHash,
    const std::vector<SourceExchangeEntry>& entries)
{
  return createAnswerSources2Payload(fileHash, SOURCE_EXCHANGE2_VERSION,
                                     entries);
}

bool parseAnswerSourcesPayload(SourceExchangeAnswer& answer,
                               const std::string& payload,
                               const std::string& expectedFileHash,
                               uint8_t sourceExchangeVersion)
{
  validateHashLength(expectedFileHash);
  if (sourceExchangeVersion == 0 ||
      sourceExchangeVersion > SOURCE_EXCHANGE2_VERSION ||
      payload.size() < HASH_LENGTH + 2) {
    return false;
  }
  size_t offset = 0;
  const auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (hash != expectedFileHash) {
    return false;
  }
  const auto count = readUInt16(readBytes(payload, offset, 2).data());
  if (count > 500) {
    return false;
  }
  const size_t entrySize = sourceExchangeVersion == 1   ? 12
                           : sourceExchangeVersion < 4 ? 28
                                                       : 29;
  if (payload.size() - offset != entrySize * count) {
    return false;
  }
  answer.version = sourceExchangeVersion;
  answer.entries.clear();
  answer.entries.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    SourceExchangeEntry entry;
    entry.endpoint = readEndpoint(payload, offset);
    entry.server = readEndpoint(payload, offset);
    if (sourceExchangeVersion >= 2) {
      entry.userHash = readBytes(payload, offset, HASH_LENGTH);
      entry.endpoint.userHash = entry.userHash;
    }
    if (sourceExchangeVersion >= 4) {
      entry.cryptOptions = readByte(payload, offset);
      entry.endpoint.cryptOptions = entry.cryptOptions;
    }
    answer.entries.push_back(std::move(entry));
  }
  return true;
}

bool parseAnswerSources2Payload(SourceExchangeAnswer& answer,
                                const std::string& payload,
                                const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  size_t offset = 0;
  const auto version = readByte(payload, offset);
  const auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (hash != expectedFileHash || version == 0 ||
      version > SOURCE_EXCHANGE2_VERSION) {
    return false;
  }
  const auto count = readUInt16(readBytes(payload, offset, 2).data());
  if (count > 500) {
    return false;
  }
  const size_t entrySize = version == 1   ? 12
                           : version < 4 ? 28
                                         : 29;
  if (payload.size() - offset != entrySize * count) {
    return false;
  }
  answer.version = version;
  answer.entries.clear();
  answer.entries.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    SourceExchangeEntry entry;
    entry.endpoint = readEndpoint(payload, offset);
    entry.server = readEndpoint(payload, offset);
    if (version >= 2) {
      entry.userHash = readBytes(payload, offset, HASH_LENGTH);
    }
    if (version >= 4) {
      entry.cryptOptions = readByte(payload, offset);
      entry.endpoint.cryptOptions = entry.cryptOptions;
      entry.endpoint.userHash = entry.userHash;
    }
    answer.entries.push_back(std::move(entry));
  }
  return true;
}

std::string createEmuleInfoPayload(const EmulePeerInfo& info)
{
  std::string payload;
  payload.push_back(static_cast<char>(info.version));
  payload.push_back(static_cast<char>(info.protocolVersion));
  payload += packUInt32(2);
  payload += createUInt32Tag(0xfb, emuleMiscOptionsValue(info.miscOptions));
  payload += createUInt32Tag(0xf6, emuleMiscOptions2Value(info.miscOptions2));
  return payload;
}

bool parseEmuleInfoPayload(EmulePeerInfo& info, const std::string& payload)
{
  if (payload.size() < 6) {
    return false;
  }
  info.version = static_cast<unsigned char>(payload[0]);
  info.protocolVersion = static_cast<unsigned char>(payload[1]);
  std::vector<Tag> tags;
  if (!parseTagList(tags, payload.substr(2))) {
    return false;
  }
  for (const auto& tag : tags) {
    if (tag.valueType != TagValueType::UINT) {
      continue;
    }
    if (tag.id == 0xfb) {
      info.miscOptions = parseEmuleMiscOptions(tag.intValue);
    }
    else if (tag.id == 0xf6) {
      info.miscOptions2 = parseEmuleMiscOptions2(tag.intValue);
    }
  }
  return true;
}

bool parsePeerHelloUserHash(std::string& userHash, const std::string& payload,
                            bool helloPacket)
{
  const auto offset = helloPacket ? 1 : 0;
  if (payload.size() < offset + HASH_LENGTH) {
    return false;
  }
  userHash = payload.substr(offset, HASH_LENGTH);
  return true;
}

std::string createUdpReaskFilePingPayload(const std::string& fileHash,
                                          uint16_t completeSources)
{
  validateHashLength(fileHash);
  return fileHash + packUInt16(completeSources);
}

bool parseUdpReaskFilePingPayload(UdpReask& reask,
                                  const std::string& payload)
{
  if (payload.size() != HASH_LENGTH && payload.size() != HASH_LENGTH + 2) {
    return false;
  }
  size_t offset = 0;
  UdpReask parsed;
  parsed.fileHash = readBytes(payload, offset, HASH_LENGTH);
  if (offset < payload.size()) {
    parsed.completeSources = readUInt16(readBytes(payload, offset, 2).data());
    parsed.hasCompleteSources = true;
  }
  reask = std::move(parsed);
  return true;
}

std::string createUdpReaskAckPayload(uint16_t rank)
{
  return packUInt16(rank);
}

std::string createUdpReaskAckPayload(const std::vector<bool>& bitfield,
                                     uint16_t rank)
{
  if (bitfield.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K file status bitfield is too large.");
  }
  std::string payload;
  payload += packUInt16(static_cast<uint16_t>(bitfield.size()));
  payload.append((bitfield.size() + 7) / 8, '\0');
  for (size_t i = 0; i < bitfield.size(); ++i) {
    if (bitfield[i]) {
      payload[2 + i / 8] |= static_cast<char>(0x80 >> (i & 7));
    }
  }
  payload += packUInt16(rank);
  return payload;
}

bool parseUdpReaskAckPayload(UdpReaskAck& ack, const std::string& payload)
{
  if (payload.size() != 2 && payload.size() < 4) {
    return false;
  }
  size_t offset = 0;
  UdpReaskAck parsed;
  if (payload.size() > 2) {
    auto bitCount = readUInt16(readBytes(payload, offset, 2).data());
    auto byteCount = (bitCount + 7) / 8;
    if (payload.size() - offset != byteCount + 2) {
      return false;
    }
    auto raw = readBytes(payload, offset, byteCount);
    parsed.bitfield.assign(bitCount, false);
    for (size_t i = 0; i < bitCount; ++i) {
      parsed.bitfield[i] = raw[i / 8] & static_cast<char>(0x80 >> (i & 7));
    }
  }
  parsed.rank = readUInt16(readBytes(payload, offset, 2).data());
  ack = std::move(parsed);
  return true;
}

} // namespace ed2k

} // namespace aria2
