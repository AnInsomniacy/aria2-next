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
#include "ed2k_kad_search.h"

#include <limits>

#include "DlAbortEx.h"
#include "ed2k_endpoint.h"
#include "ed2k_hash.h"

namespace aria2 {

namespace ed2k {

namespace {

void validateHashLength(const std::string& hash)
{
  if (hash.size() != HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K hash length.");
  }
}

} // namespace

KadSearchEntry readKadSearchEntry(const std::string& data, size_t& offset)
{
  KadSearchEntry entry;
  entry.id = readBytes(data, offset, HASH_LENGTH);
  const auto tagCount = readByte(data, offset);
  std::string tagPayload = packUInt32(tagCount);
  for (uint8_t i = 0; i < tagCount; ++i) {
    const auto before = offset;
    readTag(data, offset);
    tagPayload.append(data.begin() + before, data.begin() + offset);
  }
  if (!parseTagList(entry.tags, tagPayload)) {
    throw DL_ABORT_EX("Bad Kad search tags.");
  }
  return entry;
}

std::string packKadSearchEntry(const KadSearchEntry& entry)
{
  validateHashLength(entry.id);
  if (entry.tags.size() > std::numeric_limits<uint8_t>::max()) {
    throw DL_ABORT_EX("Too many Kad search tags.");
  }
  std::string payload = entry.id;
  payload.push_back(static_cast<char>(entry.tags.size()));
  for (const auto& tag : entry.tags) {
    if (tag.valueType == TagValueType::STRING) {
      payload += createStringTag(tag.id, tag.stringValue);
    }
    else if (tag.valueType == TagValueType::UINT) {
      if (tag.intValue > std::numeric_limits<uint32_t>::max()) {
        payload += createUInt64Tag(tag.id, tag.intValue);
      }
      else {
        payload += createUInt32Tag(tag.id, static_cast<uint32_t>(tag.intValue));
      }
    }
    else {
      throw DL_ABORT_EX("Unsupported Kad search tag.");
    }
  }
  return payload;
}

std::string createKadSearchSourcesRequestPayload(const std::string& targetId,
                                                 uint16_t startPosition,
                                                 uint64_t size)
{
  validateHashLength(targetId);
  return targetId + packUInt16(startPosition) + packUInt64(size);
}

bool parseKadSearchSourcesRequestPayload(KadSearchSourcesRequest& request,
                                         const std::string& payload)
{
  if (payload.size() != HASH_LENGTH + 10) {
    return false;
  }
  size_t offset = 0;
  request.targetId = readBytes(payload, offset, HASH_LENGTH);
  request.startPosition = readUInt16(readBytes(payload, offset, 2).data());
  request.size = readUInt64(readBytes(payload, offset, 8).data());
  return true;
}

std::string createKadSearchKeysRequestPayload(const std::string& targetId,
                                              uint16_t startPosition)
{
  validateHashLength(targetId);
  return targetId + packUInt16(startPosition);
}

bool parseKadSearchResultPayload(KadSearchResult& result,
                                 const std::string& payload)
{
  if (payload.size() < HASH_LENGTH * 2 + 2) {
    return false;
  }
  try {
    size_t offset = 0;
    result.sourceId = readBytes(payload, offset, HASH_LENGTH);
    result.targetId = readBytes(payload, offset, HASH_LENGTH);
    const auto count = readUInt16(readBytes(payload, offset, 2).data());
    result.entries.clear();
    result.entries.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
      result.entries.push_back(readKadSearchEntry(payload, offset));
    }
    return offset == payload.size();
  }
  catch (DlAbortEx&) {
    return false;
  }
}

std::string createKadSearchResultPayload(
    const std::string& sourceId, const std::string& targetId,
    const std::vector<KadSearchEntry>& entries)
{
  validateHashLength(sourceId);
  validateHashLength(targetId);
  if (entries.size() > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX("Too many Kad search results.");
  }
  std::string payload = sourceId;
  payload += targetId;
  payload += packUInt16(static_cast<uint16_t>(entries.size()));
  for (const auto& entry : entries) {
    payload += packKadSearchEntry(entry);
  }
  return payload;
}

bool extractKadSourceEndpoint(Endpoint& endpoint, const KadSearchEntry& entry)
{
  uint32_t ip = 0;
  uint16_t port = 0;
  uint64_t sourceType = 0;
  bool hasIp = false;
  bool hasPort = false;
  for (const auto& tag : entry.tags) {
    if (tag.valueType != TagValueType::UINT) {
      continue;
    }
    if (tag.id == 0xfe) {
      ip = static_cast<uint32_t>(tag.intValue);
      hasIp = true;
    }
    else if (tag.id == 0xfd) {
      port = static_cast<uint16_t>(tag.intValue);
      hasPort = true;
    }
    else if (tag.id == 0xff) {
      sourceType = tag.intValue;
    }
  }
  if (!hasIp || !hasPort || port == 0 ||
      (sourceType != 0 && sourceType != 1 && sourceType != 4)) {
    return false;
  }
  endpoint.host = ipv4FromEndpoint(ip);
  endpoint.port = port;
  return true;
}

std::vector<Endpoint> extractKadSourceEndpoints(const KadSearchResult& result)
{
  std::vector<Endpoint> endpoints;
  for (const auto& entry : result.entries) {
    Endpoint endpoint;
    if (extractKadSourceEndpoint(endpoint, entry)) {
      endpoints.push_back(std::move(endpoint));
    }
  }
  return endpoints;
}

std::string createKadPublishSourceRequestPayload(const std::string& fileId,
                                                 const Endpoint& source,
                                                 const std::string& sourceId,
                                                 uint64_t size)
{
  validateHashLength(fileId);
  validateHashLength(sourceId);
  KadSearchEntry entry;
  entry.id = sourceId;

  Tag sourceType;
  sourceType.id = 0xff;
  sourceType.valueType = TagValueType::UINT;
  sourceType.intValue = 1;
  entry.tags.push_back(sourceType);

  Tag sourceIp;
  sourceIp.id = 0xfe;
  sourceIp.valueType = TagValueType::UINT;
  sourceIp.intValue = ipv4ToEndpointValue(source.host);
  entry.tags.push_back(sourceIp);

  Tag sourcePort;
  sourcePort.id = 0xfd;
  sourcePort.valueType = TagValueType::UINT;
  sourcePort.intValue = source.port;
  entry.tags.push_back(sourcePort);

  if (size != 0) {
    Tag fileSize;
    fileSize.id = 0xd3;
    fileSize.valueType = TagValueType::UINT;
    fileSize.intValue = size;
    entry.tags.push_back(fileSize);
  }

  return fileId + packKadSearchEntry(entry);
}

bool parseKadPublishSourceRequestPayload(KadPublishSourceRequest& request,
                                         const std::string& payload)
{
  if (payload.size() < HASH_LENGTH * 2 + 1) {
    return false;
  }
  try {
    size_t offset = 0;
    request.fileId = readBytes(payload, offset, HASH_LENGTH);
    request.source = readKadSearchEntry(payload, offset);
    return offset == payload.size();
  }
  catch (DlAbortEx&) {
    return false;
  }
}

} // namespace ed2k

} // namespace aria2
