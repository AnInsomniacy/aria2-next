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
#include "ed2k_packet.h"

#include "DlAbortEx.h"
#include "ed2k_hash.h"

namespace aria2 {

namespace ed2k {

uint8_t readByte(const std::string& data, size_t& offset)
{
  if (offset >= data.size()) {
    throw DL_ABORT_EX("Truncated ED2K data.");
  }
  return static_cast<unsigned char>(data[offset++]);
}

std::string readBytes(const std::string& data, size_t& offset, size_t length)
{
  if (length > data.size() || offset > data.size() - length) {
    throw DL_ABORT_EX("Truncated ED2K data.");
  }
  auto result = data.substr(offset, length);
  offset += length;
  return result;
}

uint16_t readUInt16(const char* data)
{
  auto bytes = reinterpret_cast<const unsigned char*>(data);
  return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t readUInt32(const char* data)
{
  auto bytes = reinterpret_cast<const unsigned char*>(data);
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

uint64_t readUInt64(const char* data)
{
  auto bytes = reinterpret_cast<const unsigned char*>(data);
  uint64_t value = 0;
  for (size_t i = 0; i < 8; ++i) {
    value |= static_cast<uint64_t>(bytes[i]) << (i * 8);
  }
  return value;
}

std::string packUInt16(uint16_t value)
{
  std::string out(2, '\0');
  out[0] = static_cast<char>(value);
  out[1] = static_cast<char>(value >> 8);
  return out;
}

std::string packUInt32(uint32_t value)
{
  std::string out(4, '\0');
  for (size_t i = 0; i < 4; ++i) {
    out[i] = static_cast<char>(value >> (i * 8));
  }
  return out;
}

std::string packUInt64(uint64_t value)
{
  std::string out(8, '\0');
  for (size_t i = 0; i < 8; ++i) {
    out[i] = static_cast<char>(value >> (i * 8));
  }
  return out;
}

std::string createPacket(uint8_t protocol, uint8_t opcode,
                         const std::string& payload)
{
  std::string packet;
  packet.reserve(6 + payload.size());
  packet.push_back(static_cast<char>(protocol));
  packet += packUInt32(static_cast<uint32_t>(payload.size() + 1));
  packet.push_back(static_cast<char>(opcode));
  packet += payload;
  return packet;
}

bool readPacketHeader(PacketHeader& header, const char* data, size_t length)
{
  if (length < 6) {
    return false;
  }
  header.protocol = static_cast<unsigned char>(data[0]);
  header.size = readUInt32(data + 1);
  header.opcode = static_cast<unsigned char>(data[5]);
  return header.size > 0;
}

Tag readTag(const std::string& data, size_t& offset)
{
  const uint8_t tagTypeWithName = readByte(data, offset);
  const uint8_t tagType = tagTypeWithName & 0x7f;
  Tag tag;
  tag.rawType = tagType;
  if (tagTypeWithName & 0x80) {
    tag.id = readByte(data, offset);
  }
  else {
    auto nameSize = readUInt16(readBytes(data, offset, 2).data());
    tag.name = readBytes(data, offset, nameSize);
  }

  switch (tagType) {
  case 0x01:
    tag.valueType = TagValueType::HASH;
    tag.binaryValue = readBytes(data, offset, HASH_LENGTH);
    break;
  case 0x02: {
    auto size = readUInt16(readBytes(data, offset, 2).data());
    tag.valueType = TagValueType::STRING;
    tag.stringValue = readBytes(data, offset, size);
    break;
  }
  case 0x03:
    tag.valueType = TagValueType::UINT;
    tag.intValue = readUInt32(readBytes(data, offset, 4).data());
    break;
  case 0x07: {
    auto size = readUInt32(readBytes(data, offset, 4).data());
    tag.valueType = TagValueType::BLOB;
    tag.binaryValue = readBytes(data, offset, size);
    break;
  }
  case 0x08:
    tag.valueType = TagValueType::UINT;
    tag.intValue = readUInt16(readBytes(data, offset, 2).data());
    break;
  case 0x09:
    tag.valueType = TagValueType::UINT;
    tag.intValue = readByte(data, offset);
    break;
  case 0x0b:
    tag.valueType = TagValueType::UINT;
    tag.intValue = readUInt64(readBytes(data, offset, 8).data());
    break;
  default:
    if (tagType >= 0x11 && tagType <= 0x20) {
      tag.valueType = TagValueType::STRING;
      tag.stringValue = readBytes(data, offset, tagType - 0x11 + 1);
    }
    break;
  }
  return tag;
}

void skipTag(const std::string& data, size_t& offset)
{
  readTag(data, offset);
}

std::string createTagHeader(uint8_t type, uint8_t id)
{
  std::string payload;
  payload.push_back(static_cast<char>(type | 0x80));
  payload.push_back(static_cast<char>(id));
  return payload;
}

std::string createUInt32Tag(uint8_t id, uint32_t value)
{
  return createTagHeader(0x03, id) + packUInt32(value);
}

std::string createUInt64Tag(uint8_t id, uint64_t value)
{
  return createTagHeader(0x0b, id) + packUInt64(value);
}

std::string createStringTag(uint8_t id, const std::string& value)
{
  if (!value.empty() && value.size() <= 16) {
    return createTagHeader(static_cast<uint8_t>(0x11 + value.size() - 1), id) +
           value;
  }
  return createTagHeader(0x02, id) +
         packUInt16(static_cast<uint16_t>(value.size())) + value;
}

bool parseTagList(std::vector<Tag>& tags, const std::string& payload)
{
  if (payload.size() < 4) {
    return false;
  }
  size_t offset = 0;
  const auto count = readUInt32(readBytes(payload, offset, 4).data());
  if (count > 10000) {
    return false;
  }
  tags.clear();
  tags.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    tags.push_back(readTag(payload, offset));
  }
  return offset == payload.size();
}

} // namespace ed2k

} // namespace aria2
