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
#ifndef D_ED2K_PACKET_H
#define D_ED2K_PACKET_H

#include "common.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

namespace ed2k {

struct PacketHeader {
  uint8_t protocol = 0;
  uint32_t size = 0;
  uint8_t opcode = 0;

  size_t payloadSize() const { return size == 0 ? 0 : size - 1; }
};

enum class TagValueType {
  UNKNOWN,
  STRING,
  UINT,
  HASH,
  BLOB,
};

struct Tag {
  uint8_t id = 0;
  std::string name;
  uint8_t rawType = 0;
  TagValueType valueType = TagValueType::UNKNOWN;
  std::string stringValue;
  uint64_t intValue = 0;
  std::string binaryValue;
};

uint8_t readByte(const std::string& data, size_t& offset);
std::string readBytes(const std::string& data, size_t& offset, size_t length);
uint16_t readUInt16(const char* data);
uint32_t readUInt32(const char* data);
uint64_t readUInt64(const char* data);
std::string packUInt16(uint16_t value);
std::string packUInt32(uint32_t value);
std::string packUInt64(uint64_t value);
std::string createPacket(uint8_t protocol, uint8_t opcode,
                         const std::string& payload);
bool readPacketHeader(PacketHeader& header, const char* data, size_t length);
Tag readTag(const std::string& data, size_t& offset);
void skipTag(const std::string& data, size_t& offset);
std::string createUInt32Tag(uint8_t id, uint32_t value);
std::string createUInt64Tag(uint8_t id, uint64_t value);
std::string createStringTag(uint8_t id, const std::string& value);
bool parseTagList(std::vector<Tag>& tags, const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_PACKET_H
