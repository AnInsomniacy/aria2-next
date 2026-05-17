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
#include "ed2k_kad.h"

#include <limits>

#include "DlAbortEx.h"
#include "Ed2kKadState.h"
#include "ed2k_hash.h"

namespace aria2 {

namespace ed2k {

namespace {

constexpr char KAD_ROUTING_STATE_MAGIC[] = "A2ED2KKAD";
constexpr uint32_t KAD_ROUTING_STATE_VERSION = 1;

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
    throw DL_ABORT_EX("ED2K Kad routing state string is too large.");
  }
  payload += packUInt16(static_cast<uint16_t>(value.size()));
  payload += value;
}

std::string readString(const std::string& payload, size_t& offset)
{
  const auto size = readUInt16(readBytes(payload, offset, 2).data());
  return readBytes(payload, offset, size);
}

void appendEndpoint(std::string& payload, const Endpoint& endpoint)
{
  appendString(payload, endpoint.host);
  payload += packUInt16(endpoint.port);
}

Endpoint readStateEndpoint(const std::string& payload, size_t& offset)
{
  Endpoint endpoint;
  endpoint.host = readString(payload, offset);
  endpoint.port = readUInt16(readBytes(payload, offset, 2).data());
  return endpoint;
}

void appendKadRoutingNode(std::string& payload, const KadRoutingNode& node)
{
  validateHashLength(node.contact.id);
  payload += node.contact.id;
  appendString(payload, node.contact.host);
  payload += packUInt16(node.contact.udpPort);
  payload += packUInt16(node.contact.tcpPort);
  appendByte(payload, node.contact.version);
  appendByte(payload, node.confirmed ? 1 : 0);
  appendByte(payload, node.seed ? 1 : 0);
  payload += packUInt32(node.failCount);
  appendInt64(payload, node.firstSeen);
  appendInt64(payload, node.lastSeen);
}

KadRoutingNode readKadRoutingNode(const std::string& payload, size_t& offset)
{
  KadRoutingNode node;
  node.contact.id = readBytes(payload, offset, HASH_LENGTH);
  node.contact.host = readString(payload, offset);
  node.contact.udpPort = readUInt16(readBytes(payload, offset, 2).data());
  node.contact.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
  node.contact.version = readByte(payload, offset);
  node.confirmed = readByte(payload, offset) != 0;
  node.seed = readByte(payload, offset) != 0;
  node.failCount = readUInt32(readBytes(payload, offset, 4).data());
  node.firstSeen = readInt64(payload, offset);
  node.lastSeen = readInt64(payload, offset);
  return node;
}

void appendKadRoutingNodes(std::string& payload,
                           const std::vector<KadRoutingNode>& nodes)
{
  if (nodes.size() > std::numeric_limits<uint32_t>::max()) {
    throw DL_ABORT_EX("ED2K Kad routing state has too many nodes.");
  }
  payload += packUInt32(static_cast<uint32_t>(nodes.size()));
  for (const auto& node : nodes) {
    appendKadRoutingNode(payload, node);
  }
}

std::vector<KadRoutingNode> readKadRoutingNodes(const std::string& payload,
                                                size_t& offset)
{
  const auto count = readUInt32(readBytes(payload, offset, 4).data());
  std::vector<KadRoutingNode> nodes;
  nodes.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    nodes.push_back(readKadRoutingNode(payload, offset));
  }
  return nodes;
}

} // namespace

KadContact readKadContact(const std::string& data, size_t& offset)
{
  KadContact contact;
  contact.id = readBytes(data, offset, HASH_LENGTH);
  contact.host = ipv4FromEndpoint(readUInt32(readBytes(data, offset, 4).data()));
  contact.udpPort = readUInt16(readBytes(data, offset, 2).data());
  contact.tcpPort = readUInt16(readBytes(data, offset, 2).data());
  contact.version = readByte(data, offset);
  return contact;
}

std::string packKadContact(const KadContact& contact)
{
  validateHashLength(contact.id);
  std::string payload = contact.id;
  payload += packUInt32(ipv4ToEndpointValue(contact.host));
  payload += packUInt16(contact.udpPort);
  payload += packUInt16(contact.tcpPort);
  payload.push_back(static_cast<char>(contact.version));
  return payload;
}

std::string createKadHelloPayload(const std::string& id, uint16_t tcpPort,
                                  uint8_t version)
{
  validateHashLength(id);
  std::string payload = id;
  payload += packUInt16(tcpPort);
  payload.push_back(static_cast<char>(version));
  payload.push_back('\0');
  return payload;
}

bool parseKadHelloPayload(KadHello& hello, const std::string& payload)
{
  if (payload.size() < HASH_LENGTH + 4) {
    return false;
  }
  size_t offset = 0;
  hello.id = readBytes(payload, offset, HASH_LENGTH);
  hello.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
  hello.version = readByte(payload, offset);
  const auto tagCount = readByte(payload, offset);
  if (payload.size() - offset < tagCount * 2) {
    return false;
  }

  std::string tagPayload = packUInt32(tagCount);
  tagPayload.append(payload.begin() + offset, payload.end());
  if (!parseTagList(hello.tags, tagPayload)) {
    return false;
  }
  offset = payload.size();
  return true;
}

std::string createKadBootstrapResponsePayload(
    const std::string& id, uint16_t tcpPort, uint8_t version,
    const std::vector<KadContact>& contacts)
{
  validateHashLength(id);
  if (contacts.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("Too many ED2K Kad contacts.");
  }
  std::string payload = id;
  payload += packUInt16(tcpPort);
  payload.push_back(static_cast<char>(version));
  payload += packUInt16(static_cast<uint16_t>(contacts.size()));
  for (const auto& contact : contacts) {
    payload += packKadContact(contact);
  }
  return payload;
}

bool parseKadBootstrapResponsePayload(KadBootstrapResponse& response,
                                      const std::string& payload)
{
  if (payload.size() < HASH_LENGTH + 5) {
    return false;
  }
  size_t offset = 0;
  response.id = readBytes(payload, offset, HASH_LENGTH);
  response.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
  response.version = readByte(payload, offset);
  const auto count = readUInt16(readBytes(payload, offset, 2).data());
  if (payload.size() - offset != static_cast<size_t>(count) * 25) {
    return false;
  }
  response.contacts.clear();
  response.contacts.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    response.contacts.push_back(readKadContact(payload, offset));
  }
  return true;
}

std::string createKadRequestPayload(uint8_t searchType,
                                    const std::string& targetId,
                                    const std::string& receiverId)
{
  validateHashLength(targetId);
  validateHashLength(receiverId);
  std::string payload;
  payload.push_back(static_cast<char>(searchType));
  payload += targetId;
  payload += receiverId;
  return payload;
}

bool parseKadRequestPayload(KadRequest& request, const std::string& payload)
{
  if (payload.size() != HASH_LENGTH * 2 + 1) {
    return false;
  }
  size_t offset = 0;
  request.searchType = readByte(payload, offset);
  request.targetId = readBytes(payload, offset, HASH_LENGTH);
  request.receiverId = readBytes(payload, offset, HASH_LENGTH);
  return true;
}

std::string createKadResponsePayload(const std::string& targetId,
                                     const std::vector<KadContact>& contacts)
{
  validateHashLength(targetId);
  if (contacts.size() > std::numeric_limits<uint8_t>::max()) {
    throw DL_ABORT_EX("Too many ED2K Kad response contacts.");
  }
  std::string payload = targetId;
  payload.push_back(static_cast<char>(contacts.size()));
  for (const auto& contact : contacts) {
    payload += packKadContact(contact);
  }
  return payload;
}

bool parseKadResponsePayload(KadResponse& response,
                             const std::string& payload)
{
  if (payload.size() < HASH_LENGTH + 1) {
    return false;
  }
  size_t offset = 0;
  response.targetId = readBytes(payload, offset, HASH_LENGTH);
  const auto count = readByte(payload, offset);
  if (payload.size() - offset != static_cast<size_t>(count) * 25) {
    return false;
  }
  response.contacts.clear();
  response.contacts.reserve(count);
  for (uint8_t i = 0; i < count; ++i) {
    response.contacts.push_back(readKadContact(payload, offset));
  }
  return true;
}

std::string createKadPublishResultPayload(const std::string& fileId,
                                          uint8_t count)
{
  validateHashLength(fileId);
  std::string payload = fileId;
  payload.push_back(static_cast<char>(count));
  return payload;
}

bool parseKadPublishResultPayload(KadPublishResult& result,
                                  const std::string& payload)
{
  if (payload.size() != HASH_LENGTH + 1) {
    return false;
  }
  size_t offset = 0;
  result.fileId = readBytes(payload, offset, HASH_LENGTH);
  result.count = readByte(payload, offset);
  return true;
}

std::string createKadFirewalledRequestPayload(uint16_t tcpPort,
                                              const std::string& id,
                                              uint8_t options)
{
  validateHashLength(id);
  std::string payload = packUInt16(tcpPort);
  payload += id;
  payload.push_back(static_cast<char>(options));
  return payload;
}

bool parseKadFirewalledRequestPayload(KadFirewalledRequest& request,
                                      const std::string& payload)
{
  if (payload.size() != HASH_LENGTH + 3) {
    return false;
  }
  size_t offset = 0;
  request.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
  request.id = readBytes(payload, offset, HASH_LENGTH);
  request.options = readByte(payload, offset);
  return true;
}

std::string createKadFirewalledResponsePayload(const std::string& ipAddress)
{
  return packUInt32(ipv4ToEndpointValue(ipAddress));
}

bool parseKadFirewalledResponsePayload(KadFirewalledResponse& response,
                                       const std::string& payload)
{
  if (payload.size() != 4) {
    return false;
  }
  response.ipAddress = ipv4FromEndpoint(readUInt32(payload.data()));
  return true;
}

std::string createKadFirewalledUdpPayload(uint8_t errorCode,
                                          uint16_t tcpPort)
{
  std::string payload;
  payload.push_back(static_cast<char>(errorCode));
  payload += packUInt16(tcpPort);
  return payload;
}

bool parseKadFirewalledUdpPayload(KadFirewalledUdp& response,
                                  const std::string& payload)
{
  if (payload.size() != 3) {
    return false;
  }
  size_t offset = 0;
  response.errorCode = readByte(payload, offset);
  response.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
  return true;
}

std::string createKadRoutingStatePayload(const KadRoutingSnapshot& snapshot)
{
  validateHashLength(snapshot.selfId);
  if (snapshot.buckets.size() > std::numeric_limits<uint16_t>::max() ||
      snapshot.routerNodes.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K Kad routing state is too large.");
  }
  std::string payload;
  payload.append(KAD_ROUTING_STATE_MAGIC, sizeof(KAD_ROUTING_STATE_MAGIC) - 1);
  payload += packUInt32(KAD_ROUTING_STATE_VERSION);
  payload += snapshot.selfId;
  appendInt64(payload, snapshot.lastBootstrap);
  appendInt64(payload, snapshot.lastRefresh);
  appendInt64(payload, snapshot.lastSelfRefresh);
  payload += packUInt16(static_cast<uint16_t>(snapshot.routerNodes.size()));
  for (const auto& endpoint : snapshot.routerNodes) {
    appendEndpoint(payload, endpoint);
  }
  payload += packUInt16(static_cast<uint16_t>(snapshot.buckets.size()));
  for (const auto& bucket : snapshot.buckets) {
    appendInt64(payload, bucket.lastActive);
    appendKadRoutingNodes(payload, bucket.live);
    appendKadRoutingNodes(payload, bucket.replacements);
  }
  return payload;
}

bool parseKadRoutingStatePayload(KadRoutingSnapshot& snapshot,
                                 const std::string& payload)
{
  try {
    size_t offset = 0;
    const auto magic =
        readBytes(payload, offset, sizeof(KAD_ROUTING_STATE_MAGIC) - 1);
    if (magic != std::string(KAD_ROUTING_STATE_MAGIC,
                             sizeof(KAD_ROUTING_STATE_MAGIC) - 1)) {
      return false;
    }
    const auto version = readUInt32(readBytes(payload, offset, 4).data());
    if (version != KAD_ROUTING_STATE_VERSION) {
      return false;
    }
    KadRoutingSnapshot parsed;
    parsed.selfId = readBytes(payload, offset, HASH_LENGTH);
    parsed.lastBootstrap = readInt64(payload, offset);
    parsed.lastRefresh = readInt64(payload, offset);
    parsed.lastSelfRefresh = readInt64(payload, offset);
    const auto routerCount = readUInt16(readBytes(payload, offset, 2).data());
    parsed.routerNodes.reserve(routerCount);
    for (uint16_t i = 0; i < routerCount; ++i) {
      parsed.routerNodes.push_back(readStateEndpoint(payload, offset));
    }
    const auto bucketCount = readUInt16(readBytes(payload, offset, 2).data());
    parsed.buckets.reserve(bucketCount);
    for (uint16_t i = 0; i < bucketCount; ++i) {
      KadRoutingBucketSnapshot bucket;
      bucket.lastActive = readInt64(payload, offset);
      bucket.live = readKadRoutingNodes(payload, offset);
      bucket.replacements = readKadRoutingNodes(payload, offset);
      parsed.buckets.push_back(std::move(bucket));
    }
    if (offset != payload.size()) {
      return false;
    }
    snapshot = std::move(parsed);
    return true;
  }
  catch (DlAbortEx&) {
    return false;
  }
}

bool parseNodesDat(NodesDat& nodes, const std::string& payload)
{
  if (payload.size() < 4) {
    return false;
  }
  try {
    size_t offset = 0;
    uint32_t count = readUInt32(readBytes(payload, offset, 4).data());
    nodes.version = 0;
    nodes.bootstrapEdition = 0;
    nodes.contacts.clear();
    nodes.verified.clear();

    if (count == 0) {
      if (payload.size() - offset < 4) {
        return false;
      }
      nodes.version = readUInt32(readBytes(payload, offset, 4).data());
      if (nodes.version >= 1 && nodes.version <= 3) {
        if (nodes.version >= 3) {
          if (payload.size() - offset < 4) {
            return false;
          }
          nodes.bootstrapEdition =
              readUInt32(readBytes(payload, offset, 4).data());
        }
        if (payload.size() - offset < 4) {
          return false;
        }
        count = readUInt32(readBytes(payload, offset, 4).data());
      }
    }

    size_t entrySize = 25;
    const bool hasVerifiedData =
        nodes.version >= 2 && nodes.bootstrapEdition == 0;
    if (hasVerifiedData) {
      entrySize += 9;
    }
    if (payload.size() - offset < static_cast<size_t>(count) * entrySize) {
      return false;
    }

    nodes.contacts.reserve(count);
    if (hasVerifiedData) {
      nodes.verified.reserve(count);
    }
    for (uint32_t i = 0; i < count; ++i) {
      nodes.contacts.push_back(readKadContact(payload, offset));
      if (hasVerifiedData) {
        readBytes(payload, offset, 8);
        nodes.verified.push_back(readByte(payload, offset) != 0);
      }
    }
    return offset == payload.size();
  }
  catch (DlAbortEx&) {
    return false;
  }
}

} // namespace ed2k

} // namespace aria2
