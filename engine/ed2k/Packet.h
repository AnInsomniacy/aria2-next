/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace aria2::ed2k {

// Malformed or unsupported protocol data. The connection or datagram that
// carried it is dropped.
class ProtocolError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

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
std::string createDatagram(uint8_t protocol, uint8_t opcode,
                           const std::string& payload);
bool readPacketHeader(PacketHeader& header, const char* data, size_t length);
bool readDatagramHeader(PacketHeader& header, const char* data, size_t length);
Tag readTag(const std::string& data, size_t& offset);
void skipTag(const std::string& data, size_t& offset);
std::string createUInt32Tag(uint8_t id, uint32_t value);
std::string createUInt64Tag(uint8_t id, uint64_t value);
std::string createStringTag(uint8_t id, const std::string& value);
bool parseTagList(std::vector<Tag>& tags, const std::string& payload);

} // namespace aria2::ed2k
