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
#include "ed2k_server.h"

#include <limits>

#include "DlAbortEx.h"
#include "ed2k_endpoint.h"
#include "ed2k_hash.h"
#include "ed2k_packet.h"

namespace aria2 {

namespace ed2k {

namespace {

constexpr char SERVER_STATE_MAGIC[] = "A2ED2KSRV";
constexpr uint32_t SERVER_STATE_VERSION = 3;

constexpr uint8_t SERVER_TAG_NAME = 0x01;
constexpr uint8_t SERVER_TAG_DESCRIPTION = 0x0b;
constexpr uint8_t SERVER_TAG_PREFERENCE = 0x0e;

void validateHashLength(const std::string& hash)
{
  if (hash.size() != HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K hash length.");
  }
}

void appendByte(std::string& payload, uint8_t value)
{
  payload.push_back(static_cast<char>(value));
}

void appendInt64(std::string& payload, int64_t value)
{
  payload += packUInt64(static_cast<uint64_t>(value));
}

int64_t readInt64(const std::string& payload, size_t& offset)
{
  return static_cast<int64_t>(readUInt64(readBytes(payload, offset, 8).data()));
}

void appendString(std::string& payload, const std::string& value)
{
  if (value.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K string is too long.");
  }
  payload += packUInt16(value.size());
  payload += value;
}

std::string readString(const std::string& payload, size_t& offset)
{
  const auto length = readUInt16(readBytes(payload, offset, 2).data());
  return readBytes(payload, offset, length);
}

void appendServerStateEndpoint(std::string& payload, const Endpoint& endpoint)
{
  appendString(payload, endpoint.host);
  payload += packUInt16(endpoint.port);
}

Endpoint readServerStateEndpoint(const std::string& payload, size_t& offset)
{
  Endpoint endpoint;
  endpoint.host = readString(payload, offset);
  endpoint.port = readUInt16(readBytes(payload, offset, 2).data());
  return endpoint;
}

} // namespace

std::vector<ServerMetEntry> parseServerMetEntries(const std::string& data)
{
  size_t offset = 0;
  auto header = readByte(data, offset);
  if (header != 0x0e && header != 0x0f) {
    offset = 0;
  }
  const uint32_t count = readUInt32(readBytes(data, offset, 4).data());
  std::vector<ServerMetEntry> entries;
  entries.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    ServerMetEntry entry;
    entry.endpoint = readEndpoint(data, offset);
    const uint32_t tagCount = readUInt32(readBytes(data, offset, 4).data());
    for (uint32_t j = 0; j < tagCount; ++j) {
      const auto tag = readTag(data, offset);
      if (tag.valueType == TagValueType::STRING) {
        if (tag.id == SERVER_TAG_NAME) {
          entry.name = tag.stringValue;
        }
        else if (tag.id == SERVER_TAG_DESCRIPTION) {
          entry.description = tag.stringValue;
        }
        else if (tag.id == SERVER_TAG_PREFERENCE &&
                 entry.endpoint.host == "0.0.0.0") {
          entry.endpoint.host = tag.stringValue;
        }
      }
    }
    if (!entry.endpoint.host.empty() && entry.endpoint.port) {
      entries.push_back(std::move(entry));
    }
  }
  return entries;
}

std::vector<Endpoint> parseServerMet(const std::string& data)
{
  auto entries = parseServerMetEntries(data);
  std::vector<Endpoint> servers;
  servers.reserve(entries.size());
  for (const auto& entry : entries) {
    servers.push_back(entry.endpoint);
  }
  return servers;
}

std::string createLoginRequestPayload(const std::string& clientHash,
                                      uint16_t listenPort,
                                      const std::string& clientName)
{
  validateHashLength(clientHash);
  std::string payload;
  payload += clientHash;
  payload += packUInt32(0);
  payload += packUInt16(listenPort);
  payload += packUInt32(4);
  payload += createUInt32Tag(0x11, 0x3c);
  payload += createUInt32Tag(0x20, 0x011d);
  payload += createStringTag(0x01, clientName);
  payload += createUInt32Tag(0xfb, (1u << 24) | (1u << 17) | (1u << 7));
  return payload;
}

std::string createGetSourcesPayload(const std::string& fileHash,
                                    int64_t fileSize)
{
  validateHashLength(fileHash);
  if (fileSize < 0) {
    throw DL_ABORT_EX("Bad ED2K file size.");
  }
  std::string payload = fileHash;
  if (fileSize > std::numeric_limits<uint32_t>::max()) {
    payload += packUInt32(0);
    payload += packUInt32(static_cast<uint32_t>(fileSize));
    payload += packUInt32(
        static_cast<uint32_t>(static_cast<uint64_t>(fileSize) >> 32));
  }
  else {
    payload += packUInt32(static_cast<uint32_t>(fileSize));
  }
  return payload;
}

std::string createFoundSourcesPayload(const std::string& fileHash,
                                      const std::vector<Endpoint>& sources)
{
  validateHashLength(fileHash);
  if (sources.size() > 255) {
    throw DL_ABORT_EX("Too many ED2K sources.");
  }
  std::string payload = fileHash;
  payload.push_back(static_cast<char>(sources.size()));
  for (const auto& source : sources) {
    payload += packEndpoint(source);
  }
  return payload;
}

std::vector<Endpoint> parseFoundSourcesPayload(const std::string& payload)
{
  size_t offset = 0;
  readBytes(payload, offset, HASH_LENGTH);
  auto count = readByte(payload, offset);
  std::vector<Endpoint> sources;
  sources.reserve(count);
  for (uint8_t i = 0; i < count; ++i) {
    sources.push_back(readEndpoint(payload, offset));
  }
  return sources;
}

bool parseFoundSourcesPayload(std::vector<Endpoint>& sources,
                              const std::string& payload,
                              const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  size_t offset = 0;
  auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (hash != expectedFileHash) {
    return false;
  }
  auto count = readByte(payload, offset);
  sources.clear();
  sources.reserve(count);
  for (uint8_t i = 0; i < count; ++i) {
    sources.push_back(readEndpoint(payload, offset));
  }
  return true;
}

bool parseFoundSourcesPayload(std::vector<FoundSource>& sources,
                              const std::string& payload,
                              const std::string& expectedFileHash)
{
  validateHashLength(expectedFileHash);
  size_t offset = 0;
  auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (hash != expectedFileHash) {
    return false;
  }
  auto count = readByte(payload, offset);
  sources.clear();
  sources.reserve(count);
  for (uint8_t i = 0; i < count; ++i) {
    FoundSource source;
    source.endpoint = readEndpoint(payload, offset);
    source.clientId = ipv4ToEndpointValue(source.endpoint.host);
    source.lowId = source.clientId <= 0x00ffffffu;
    sources.push_back(source);
  }
  return true;
}

std::string createCallbackRequestPayload(uint32_t clientId)
{
  return packUInt32(clientId);
}

bool parseCallbackRequestIncomingPayload(Endpoint& endpoint,
                                         const std::string& payload)
{
  if (payload.size() != 6) {
    return false;
  }
  size_t offset = 0;
  endpoint = readEndpoint(payload, offset);
  return true;
}

bool parseServerIdChangePayload(ServerIdChange& idChange,
                                const std::string& payload)
{
  if (payload.size() < 4) {
    return false;
  }
  if (payload.size() != 4 && payload.size() != 8 && payload.size() != 12 &&
      payload.size() != 16 && payload.size() != 20) {
    return false;
  }
  ServerIdChange parsed;
  size_t offset = 0;
  parsed.clientId = readUInt32(readBytes(payload, offset, 4).data());
  if (payload.size() >= 8) {
    parsed.tcpFlags = readUInt32(readBytes(payload, offset, 4).data());
  }
  if (payload.size() >= 12) {
    parsed.auxPort = readUInt32(readBytes(payload, offset, 4).data());
  }
  if (payload.size() >= 16) {
    const auto reportedIp = readUInt32(readBytes(payload, offset, 4).data());
    if (reportedIp > 0x00ffffffu) {
      parsed.ipAddress = ipv4FromEndpoint(reportedIp);
    }
  }
  if (payload.size() >= 20) {
    readBytes(payload, offset, 4);
  }
  parsed.highId = parsed.clientId > 0x00ffffffu;
  if (parsed.ipAddress.empty() && parsed.highId) {
    parsed.ipAddress = ipv4FromEndpoint(parsed.clientId);
  }
  idChange = std::move(parsed);
  return true;
}

bool parseServerStatusPayload(ServerStatus& status,
                              const std::string& payload)
{
  if (payload.size() != 8 && payload.size() != 12 && payload.size() != 16 &&
      payload.size() != 24 && payload.size() != 28 && payload.size() != 32 &&
      payload.size() != 40) {
    return false;
  }
  size_t offset = 0;
  ServerStatus parsed;
  if (payload.size() != 8) {
    parsed.challenge = readUInt32(readBytes(payload, offset, 4).data());
  }
  parsed.users = readUInt32(readBytes(payload, offset, 4).data());
  parsed.files = readUInt32(readBytes(payload, offset, 4).data());
  if (payload.size() >= 16) {
    parsed.maxUsers = readUInt32(readBytes(payload, offset, 4).data());
  }
  if (payload.size() >= 24) {
    parsed.softFiles = readUInt32(readBytes(payload, offset, 4).data());
    parsed.hardFiles = readUInt32(readBytes(payload, offset, 4).data());
  }
  if (payload.size() >= 28) {
    parsed.udpFlags = readUInt32(readBytes(payload, offset, 4).data());
  }
  if (payload.size() >= 32) {
    parsed.lowIdUsers = readUInt32(readBytes(payload, offset, 4).data());
  }
  if (payload.size() >= 40) {
    parsed.udpObfuscationPort =
        readUInt16(readBytes(payload, offset, 2).data());
    parsed.tcpObfuscationPort =
        readUInt16(readBytes(payload, offset, 2).data());
    parsed.udpKey = readUInt32(readBytes(payload, offset, 4).data());
  }
  status = parsed;
  return true;
}

bool parseServerMessagePayload(std::string& message,
                               const std::string& payload)
{
  if (payload.size() < 2) {
    return false;
  }
  const auto length = readUInt16(payload.data());
  if (length != payload.size() - 2) {
    return false;
  }
  message.assign(payload.begin() + 2, payload.end());
  return true;
}

std::string createServerListPayload(const std::vector<Endpoint>& servers)
{
  if (servers.size() > 255) {
    throw DL_ABORT_EX("Too many ED2K servers.");
  }
  std::string payload;
  payload.push_back(static_cast<char>(servers.size()));
  for (const auto& server : servers) {
    payload += packEndpoint(server);
  }
  return payload;
}

bool parseServerListPayload(std::vector<Endpoint>& servers,
                            const std::string& payload)
{
  if (payload.empty()) {
    return false;
  }
  size_t offset = 0;
  const auto count = readByte(payload, offset);
  if (payload.size() - offset != static_cast<size_t>(count) * 6) {
    return false;
  }
  servers.clear();
  servers.reserve(count);
  for (uint8_t i = 0; i < count; ++i) {
    servers.push_back(readEndpoint(payload, offset));
  }
  return true;
}

std::string createServerStatePayload(const ServerState& state)
{
  std::string payload;
  payload.append(SERVER_STATE_MAGIC, sizeof(SERVER_STATE_MAGIC) - 1);
  payload += packUInt32(SERVER_STATE_VERSION);
  appendServerStateEndpoint(payload, state.endpoint);
  appendString(payload, state.name);
  appendString(payload, state.description);
  appendByte(payload, state.connected ? 1 : 0);
  appendByte(payload, state.handshakeCompleted ? 1 : 0);
  payload += packUInt32(state.clientId);
  appendByte(payload, state.highId ? 1 : 0);
  appendString(payload, state.ipAddress);
  payload += packUInt32(state.tcpFlags);
  payload += packUInt32(state.users);
  payload += packUInt32(state.files);
  payload += packUInt32(state.maxUsers);
  payload += packUInt32(state.softFiles);
  payload += packUInt32(state.hardFiles);
  payload += packUInt32(state.udpFlags);
  payload += packUInt32(state.lowIdUsers);
  payload += packUInt16(state.udpObfuscationPort);
  payload += packUInt16(state.tcpObfuscationPort);
  payload += packUInt32(state.udpKey);
  payload += packUInt32(state.udpStatusChallenge);
  appendInt64(payload, state.lastUdpStatusTime);
  appendInt64(payload, state.nextSourceRequestTime);
  payload += packUInt32(state.failCount);
  appendInt64(payload, state.lastFailureTime);
  appendInt64(payload, state.nextRetryTime);
  appendString(payload, state.lastMessage);
  return payload;
}

bool parseServerStatePayload(ServerState& state, const std::string& payload)
{
  try {
    size_t offset = 0;
    const auto magic =
        readBytes(payload, offset, sizeof(SERVER_STATE_MAGIC) - 1);
    if (magic !=
        std::string(SERVER_STATE_MAGIC, sizeof(SERVER_STATE_MAGIC) - 1)) {
      return false;
    }
    const auto version = readUInt32(readBytes(payload, offset, 4).data());
    if (version != 1 && version != 2 && version != SERVER_STATE_VERSION) {
      return false;
    }
    ServerState parsed;
    parsed.endpoint = readServerStateEndpoint(payload, offset);
    if (version >= 3) {
      parsed.name = readString(payload, offset);
      parsed.description = readString(payload, offset);
    }
    parsed.connected = readByte(payload, offset) != 0;
    parsed.handshakeCompleted = readByte(payload, offset) != 0;
    parsed.clientId = readUInt32(readBytes(payload, offset, 4).data());
    parsed.highId = readByte(payload, offset) != 0;
    parsed.ipAddress = readString(payload, offset);
    parsed.tcpFlags = readUInt32(readBytes(payload, offset, 4).data());
    parsed.users = readUInt32(readBytes(payload, offset, 4).data());
    parsed.files = readUInt32(readBytes(payload, offset, 4).data());
    parsed.maxUsers = readUInt32(readBytes(payload, offset, 4).data());
    parsed.softFiles = readUInt32(readBytes(payload, offset, 4).data());
    parsed.hardFiles = readUInt32(readBytes(payload, offset, 4).data());
    parsed.udpFlags = readUInt32(readBytes(payload, offset, 4).data());
    parsed.lowIdUsers = readUInt32(readBytes(payload, offset, 4).data());
    parsed.udpObfuscationPort =
        readUInt16(readBytes(payload, offset, 2).data());
    parsed.tcpObfuscationPort =
        readUInt16(readBytes(payload, offset, 2).data());
    parsed.udpKey = readUInt32(readBytes(payload, offset, 4).data());
    parsed.udpStatusChallenge =
        readUInt32(readBytes(payload, offset, 4).data());
    parsed.lastUdpStatusTime = readInt64(payload, offset);
    if (version >= 2) {
      parsed.nextSourceRequestTime = readInt64(payload, offset);
    }
    parsed.failCount = readUInt32(readBytes(payload, offset, 4).data());
    parsed.lastFailureTime = readInt64(payload, offset);
    parsed.nextRetryTime = readInt64(payload, offset);
    parsed.lastMessage = readString(payload, offset);
    if (offset != payload.size()) {
      return false;
    }
    state = std::move(parsed);
    return true;
  }
  catch (DlAbortEx&) {
    return false;
  }
}

} // namespace ed2k

} // namespace aria2
