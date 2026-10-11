/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aria2::ed2k {

constexpr uint16_t SOURCE_CRYPT_SUPPORT = 0x01;
constexpr uint16_t SOURCE_CRYPT_REQUEST = 0x02;
constexpr uint16_t SOURCE_CRYPT_REQUIRE = 0x04;
constexpr uint16_t SOURCE_CRYPT_DIRECT_CALLBACK = 0x08;
constexpr uint16_t SOURCE_CRYPT_HAS_USER_HASH = 0x80;

enum class LinkType {
  FILE,
  SERVER,
  SERVER_LIST,
  NODES_LIST,
  SEARCH,
};

struct Endpoint {
  std::string host;
  uint16_t port = 0;
  uint16_t cryptOptions = 0;
  std::string userHash;
};

struct FoundSource {
  Endpoint endpoint;
  uint32_t clientId = 0;
  bool lowId = false;
};

struct Link {
  LinkType type = LinkType::FILE;
  std::string name;
  int64_t size = 0;
  std::string hash;
  std::vector<std::string> pieceHashes;
  std::string aichHash;
  std::vector<Endpoint> sources;
  Endpoint server;
  std::string url;
};

bool isLink(std::string_view uri);
Link parseLink(const std::string& uri);
Endpoint parseEndpoint(const std::string& value);
std::string toFileLink(const Link& link);

} // namespace aria2::ed2k
