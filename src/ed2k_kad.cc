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

#include <algorithm>
#include <array>
#include <limits>

#include "ARC4Encryptor.h"
#include "DlAbortEx.h"
#include "Ed2kKadState.h"
#include "MessageDigest.h"
#include "ed2k_constants.h"
#include "ed2k_endpoint.h"
#include "ed2k_hash.h"
#include "message_digest_helper.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

namespace {

constexpr char KAD_ROUTING_STATE_MAGIC[] = "A2ED2KKAD";
constexpr uint32_t KAD_ROUTING_STATE_VERSION = 6;
constexpr uint32_t KAD_UDP_SYNC_CLIENT = 0x395f2ec1;

struct KadObfuscationKey {
  std::string value;
};

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

std::string ipv4FromKadContactValue(uint32_t ip)
{
  return fmt("%u.%u.%u.%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
             (ip >> 8) & 0xff, ip & 0xff);
}

uint32_t ipv4ToKadContactValue(const std::string& host)
{
  const auto ip = ipv4ToEndpointValue(host);
  return ((ip & 0x000000ffU) << 24) | ((ip & 0x0000ff00U) << 8) |
         ((ip & 0x00ff0000U) >> 8) | ((ip & 0xff000000U) >> 24);
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

std::string kadObfuscationKey(const std::string& targetId,
                              uint16_t randomKeyPart)
{
  validateHashLength(targetId);
  std::string keyData = kadIdToObfuscationKeyBytes(targetId);
  keyData += packUInt16(randomKeyPart);
  auto md5 = MessageDigest::create("md5");
  std::array<unsigned char, 16> digest;
  message_digest::digest(digest.data(), digest.size(), md5.get(),
                         keyData.data(), keyData.size());
  return std::string(reinterpret_cast<const char*>(digest.data()),
                     digest.size());
}

std::string packUInt32Be(uint32_t value)
{
  std::string out(4, '\0');
  out[0] = static_cast<char>(value >> 24);
  out[1] = static_cast<char>(value >> 16);
  out[2] = static_cast<char>(value >> 8);
  out[3] = static_cast<char>(value);
  return out;
}

uint32_t readUInt32Be(const char* data)
{
  auto bytes = reinterpret_cast<const unsigned char*>(data);
  return (static_cast<uint32_t>(bytes[0]) << 24) |
         (static_cast<uint32_t>(bytes[1]) << 16) |
         (static_cast<uint32_t>(bytes[2]) << 8) |
         static_cast<uint32_t>(bytes[3]);
}

std::string kadVerifyKeyObfuscationKey(uint32_t receiverVerifyKey,
                                       uint16_t randomKeyPart)
{
  auto keyData = packUInt32(receiverVerifyKey);
  keyData += packUInt16(randomKeyPart);
  auto md5 = MessageDigest::create("md5");
  std::array<unsigned char, 16> digest;
  message_digest::digest(digest.data(), digest.size(), md5.get(),
                         keyData.data(), keyData.size());
  return std::string(reinterpret_cast<const char*>(digest.data()),
                     digest.size());
}

void rc4Crypt(std::string& out, const std::string& data,
              const std::string& key)
{
  ARC4Encryptor rc4;
  rc4.init(reinterpret_cast<const unsigned char*>(key.data()), key.size());
  out.resize(data.size());
  rc4.encrypt(data.size(), reinterpret_cast<unsigned char*>(&out[0]),
              reinterpret_cast<const unsigned char*>(data.data()));
}

uint8_t kadObfuscationMarker(bool receiverKeyUsed)
{
  return receiverKeyUsed ? 0x0a : 0x08;
}

bool possibleKadObfuscatedDatagram(const std::string& datagram)
{
  if (datagram.size() <= 16) {
    return false;
  }
  const auto marker = static_cast<unsigned char>(datagram[0]);
  switch (marker) {
  case PROTO_EDONKEY:
  case KAD_PROTOCOL:
  case PROTO_PACKED:
  case PROTO_EMULE:
    return false;
  default:
    return (marker & 0x01) == 0;
  }
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

void appendKadContact(std::string& payload, const KadContact& contact)
{
  validateHashLength(contact.id);
  payload += contact.id;
  appendString(payload, contact.host);
  payload += packUInt16(contact.udpPort);
  payload += packUInt16(contact.tcpPort);
  appendByte(payload, contact.version);
  payload += packUInt32(contact.udpKey);
}

KadContact readStateKadContact(const std::string& payload, size_t& offset)
{
  KadContact contact;
  contact.id = readBytes(payload, offset, HASH_LENGTH);
  contact.host = readString(payload, offset);
  contact.udpPort = readUInt16(readBytes(payload, offset, 2).data());
  contact.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
  contact.version = readByte(payload, offset);
  contact.udpKey = readUInt32(readBytes(payload, offset, 4).data());
  return contact;
}

bool validNodesDatContact(const KadContact& contact)
{
  if (contact.id.size() != HASH_LENGTH || contact.host.empty() ||
      contact.host == "0.0.0.0" || contact.udpPort == 0 ||
      contact.version <= 1) {
    return false;
  }
  return contact.udpPort != 53 || contact.version > 5;
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

std::string ed2kHashToKadId(const std::string& hash)
{
  validateHashLength(hash);
  std::string id;
  id.reserve(HASH_LENGTH);
  for (size_t i = 0; i < HASH_LENGTH; i += 4) {
    id.push_back(hash[i + 3]);
    id.push_back(hash[i + 2]);
    id.push_back(hash[i + 1]);
    id.push_back(hash[i]);
  }
  return id;
}

std::string kadIdToEd2kHash(const std::string& id)
{
  return ed2kHashToKadId(id);
}

std::string kadIdToObfuscationKeyBytes(const std::string& id)
{
  validateHashLength(id);
  std::string key;
  key.reserve(HASH_LENGTH);
  for (size_t i = 0; i < HASH_LENGTH; i += 4) {
    key.push_back(id[i + 3]);
    key.push_back(id[i + 2]);
    key.push_back(id[i + 1]);
    key.push_back(id[i]);
  }
  return key;
}

KadContact readKadContact(const std::string& data, size_t& offset)
{
  KadContact contact;
  contact.id = readBytes(data, offset, HASH_LENGTH);
  contact.host =
      ipv4FromKadContactValue(readUInt32(readBytes(data, offset, 4).data()));
  contact.udpPort = readUInt16(readBytes(data, offset, 2).data());
  contact.tcpPort = readUInt16(readBytes(data, offset, 2).data());
  contact.version = readByte(data, offset);
  return contact;
}

std::string packKadContact(const KadContact& contact)
{
  validateHashLength(contact.id);
  std::string payload = contact.id;
  payload += packUInt32(ipv4ToKadContactValue(contact.host));
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

std::string createKadCallbackRequestPayload(const std::string& buddyId,
                                            const std::string& fileId,
                                            uint16_t tcpPort)
{
  validateHashLength(buddyId);
  validateHashLength(fileId);
  return buddyId + fileId + packUInt16(tcpPort);
}

bool parseKadCallbackRequestPayload(KadCallbackRequest& request,
                                    const std::string& payload)
{
  if (payload.size() != HASH_LENGTH * 2 + 2) {
    return false;
  }
  size_t offset = 0;
  request.buddyId = readBytes(payload, offset, HASH_LENGTH);
  request.fileId = readBytes(payload, offset, HASH_LENGTH);
  request.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
  return true;
}

std::string createKadObfuscatedDatagram(const std::string& datagram,
                                        const KadObfuscationKey& key,
                                        uint16_t randomKeyPart,
                                        bool receiverKeyUsed,
                                        uint32_t receiverVerifyKey,
                                        uint32_t senderVerifyKey);

bool parseKadObfuscatedDatagram(KadObfuscatedDatagram& parsed,
                                const std::string& datagram,
                                const KadObfuscationKey& key);

std::string createKadObfuscatedDatagram(const std::string& datagram,
                                        const std::string& targetId,
                                        uint16_t randomKeyPart,
                                        uint32_t receiverVerifyKey,
                                        uint32_t senderVerifyKey)
{
  validateHashLength(targetId);
  return createKadObfuscatedDatagram(datagram,
                                     KadObfuscationKey{
                                         kadObfuscationKey(targetId,
                                                           randomKeyPart)},
                                     randomKeyPart, false, receiverVerifyKey,
                                     senderVerifyKey);
}

std::string createKadObfuscatedDatagram(const std::string& datagram,
                                        uint32_t receiverVerifyKey,
                                        uint32_t senderVerifyKey,
                                        uint16_t randomKeyPart)
{
  return createKadObfuscatedDatagram(
      datagram,
      KadObfuscationKey{
          kadVerifyKeyObfuscationKey(receiverVerifyKey, randomKeyPart)},
      randomKeyPart, true, receiverVerifyKey, senderVerifyKey);
}

std::string createKadObfuscatedDatagram(const std::string& datagram,
                                        const KadObfuscationKey& key,
                                        uint16_t randomKeyPart,
                                        bool receiverKeyUsed,
                                        uint32_t receiverVerifyKey,
                                        uint32_t senderVerifyKey)
{
  ARC4Encryptor rc4;
  rc4.init(reinterpret_cast<const unsigned char*>(key.value.data()),
           key.value.size());

  std::string out;
  out.reserve(datagram.size() + 16);
  out.push_back(static_cast<char>(kadObfuscationMarker(receiverKeyUsed)));
  out += packUInt16(randomKeyPart);

  std::string plain;
  plain += packUInt32Be(KAD_UDP_SYNC_CLIENT);
  plain.push_back('\0');
  plain += packUInt32Be(receiverVerifyKey);
  plain += packUInt32Be(senderVerifyKey);
  plain += datagram;

  std::string encrypted(plain.size(), '\0');
  rc4.encrypt(plain.size(), reinterpret_cast<unsigned char*>(&encrypted[0]),
              reinterpret_cast<const unsigned char*>(plain.data()));
  out += encrypted;
  return out;
}

bool parseKadObfuscatedDatagram(KadObfuscatedDatagram& parsed,
                                const std::string& datagram,
                                const std::string& targetId)
{
  if (!possibleKadObfuscatedDatagram(datagram) ||
      targetId.size() != HASH_LENGTH) {
    return false;
  }
  const auto marker = static_cast<uint8_t>(datagram[0]);
  if ((marker & 0x03) != 0) {
    return false;
  }
  const auto randomKeyPart = readUInt16(datagram.data() + 1);
  KadObfuscationKey key{kadObfuscationKey(targetId, randomKeyPart)};
  return parseKadObfuscatedDatagram(parsed, datagram, key);
}

bool parseKadObfuscatedDatagram(KadObfuscatedDatagram& parsed,
                                const std::string& datagram,
                                uint32_t receiverVerifyKey)
{
  if (!possibleKadObfuscatedDatagram(datagram)) {
    return false;
  }
  const auto marker = static_cast<uint8_t>(datagram[0]);
  if ((marker & 0x03) != 2) {
    return false;
  }
  const auto randomKeyPart = readUInt16(datagram.data() + 1);
  KadObfuscationKey key{
      kadVerifyKeyObfuscationKey(receiverVerifyKey, randomKeyPart)};
  return parseKadObfuscatedDatagram(parsed, datagram, key);
}

bool parseKadObfuscatedDatagram(KadObfuscatedDatagram& parsed,
                                const std::string& datagram,
                                const KadObfuscationKey& key)
{
  if (!possibleKadObfuscatedDatagram(datagram)) {
    return false;
  }
  const auto marker = static_cast<uint8_t>(datagram[0]);
  if ((marker & 0x01) != 0) {
    return false;
  }
  std::string decrypted;
  rc4Crypt(decrypted, datagram.substr(3), key.value);
  if (decrypted.size() <= 13 ||
      readUInt32Be(decrypted.data()) != KAD_UDP_SYNC_CLIENT) {
    return false;
  }
  const auto padLen = static_cast<unsigned char>(decrypted[4]);
  if (decrypted.size() <= static_cast<size_t>(13 + padLen)) {
    return false;
  }
  size_t offset = 5 + padLen;
  parsed.receiverVerifyKey = readUInt32Be(decrypted.data() + offset);
  offset += 4;
  parsed.senderVerifyKey = readUInt32Be(decrypted.data() + offset);
  offset += 4;
  parsed.datagram.assign(decrypted.begin() + offset, decrypted.end());
  return true;
}

uint32_t createKadUdpVerifyKey(uint32_t baseKey, const std::string& host)
{
  if (baseKey == 0 || host.empty()) {
    return 0;
  }
  uint64_t data =
      (static_cast<uint64_t>(baseKey) << 32) | ipv4ToEndpointValue(host);
  std::string keyData;
  keyData.reserve(8);
  for (size_t i = 0; i < 8; ++i) {
    keyData.push_back(static_cast<char>(data >> (i * 8)));
  }

  auto md5 = MessageDigest::create("md5");
  std::array<unsigned char, 16> digest;
  message_digest::digest(digest.data(), digest.size(), md5.get(),
                         keyData.data(), keyData.size());
  auto value = readUInt32(reinterpret_cast<const char*>(digest.data())) ^
               readUInt32(reinterpret_cast<const char*>(digest.data()) + 4) ^
               readUInt32(reinterpret_cast<const char*>(digest.data()) + 8) ^
               readUInt32(reinterpret_cast<const char*>(digest.data()) + 12);
  return value % 0xfffffffeU + 1;
}

std::string createKadRoutingStatePayload(const KadRoutingSnapshot& snapshot)
{
  validateHashLength(snapshot.selfId);
  if (snapshot.buckets.size() > std::numeric_limits<uint16_t>::max() ||
      snapshot.routerNodes.size() > std::numeric_limits<uint16_t>::max() ||
      snapshot.routerContacts.size() >
          std::numeric_limits<uint16_t>::max() ||
      snapshot.observedAddresses.size() >
          std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("ED2K Kad routing state is too large.");
  }
  std::string payload;
  payload.append(KAD_ROUTING_STATE_MAGIC, sizeof(KAD_ROUTING_STATE_MAGIC) - 1);
  payload += packUInt32(KAD_ROUTING_STATE_VERSION);
  payload += snapshot.selfId;
  appendInt64(payload, snapshot.lastBootstrap);
  appendInt64(payload, snapshot.lastRefresh);
  appendInt64(payload, snapshot.lastSelfRefresh);
  appendInt64(payload, snapshot.lastFirewalledCheck);
  appendInt64(payload, snapshot.lastSourcePublish);
  appendInt64(payload, snapshot.lastSourceSearch);
  payload += packUInt32(snapshot.sourceSearchCount);
  payload += packUInt32(snapshot.udpVerifyKey);
  appendByte(payload, snapshot.firewalled ? 1 : 0);
  payload += packUInt16(
      static_cast<uint16_t>(snapshot.observedAddresses.size()));
  for (const auto& address : snapshot.observedAddresses) {
    appendString(payload, address);
  }
  payload += packUInt16(static_cast<uint16_t>(snapshot.routerNodes.size()));
  for (const auto& endpoint : snapshot.routerNodes) {
    appendEndpoint(payload, endpoint);
  }
  payload += packUInt16(static_cast<uint16_t>(snapshot.routerContacts.size()));
  for (const auto& contact : snapshot.routerContacts) {
    appendKadContact(payload, contact);
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
    if (version != 1 && (version < 3 || version > KAD_ROUTING_STATE_VERSION)) {
      return false;
    }
    KadRoutingSnapshot parsed;
    parsed.selfId = readBytes(payload, offset, HASH_LENGTH);
    parsed.lastBootstrap = readInt64(payload, offset);
    parsed.lastRefresh = readInt64(payload, offset);
    parsed.lastSelfRefresh = readInt64(payload, offset);
    if (version >= 2) {
      parsed.lastFirewalledCheck = readInt64(payload, offset);
      parsed.lastSourcePublish = readInt64(payload, offset);
      if (version >= 3) {
        parsed.lastSourceSearch = readInt64(payload, offset);
        parsed.sourceSearchCount =
            readUInt32(readBytes(payload, offset, 4).data());
      }
      if (version >= 6) {
        parsed.udpVerifyKey =
            readUInt32(readBytes(payload, offset, 4).data());
      }
      parsed.firewalled = readByte(payload, offset) != 0;
      const auto observedCount =
          readUInt16(readBytes(payload, offset, 2).data());
      parsed.observedAddresses.reserve(observedCount);
      for (uint16_t i = 0; i < observedCount; ++i) {
        parsed.observedAddresses.push_back(readString(payload, offset));
      }
    }
    const auto routerCount = readUInt16(readBytes(payload, offset, 2).data());
    parsed.routerNodes.reserve(routerCount);
    for (uint16_t i = 0; i < routerCount; ++i) {
      parsed.routerNodes.push_back(readStateEndpoint(payload, offset));
    }
    if (version >= 4) {
      const auto routerContactCount =
          readUInt16(readBytes(payload, offset, 2).data());
      parsed.routerContacts.reserve(routerContactCount);
      for (uint16_t i = 0; i < routerContactCount; ++i) {
        KadContact contact;
        contact.id = readBytes(payload, offset, HASH_LENGTH);
        contact.host = readString(payload, offset);
        contact.udpPort = readUInt16(readBytes(payload, offset, 2).data());
        contact.tcpPort = readUInt16(readBytes(payload, offset, 2).data());
        contact.version = readByte(payload, offset);
        if (version >= 5) {
          contact.udpKey = readUInt32(readBytes(payload, offset, 4).data());
        }
        parsed.routerContacts.push_back(contact);
      }
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
    nodes.verified.reserve(count);
    bool anyVerified = false;
    for (uint32_t i = 0; i < count; ++i) {
      auto contact = readKadContact(payload, offset);
      bool verified = true;
      if (hasVerifiedData) {
        readBytes(payload, offset, 8);
        verified = readByte(payload, offset) != 0;
      }
      if (!validNodesDatContact(contact)) {
        continue;
      }
      nodes.contacts.push_back(std::move(contact));
      nodes.verified.push_back(verified);
      anyVerified = anyVerified || verified;
    }
    if (!hasVerifiedData || !anyVerified) {
      std::fill(nodes.verified.begin(), nodes.verified.end(), true);
    }
    return offset == payload.size();
  }
  catch (DlAbortEx&) {
    return false;
  }
}

} // namespace ed2k

} // namespace aria2
