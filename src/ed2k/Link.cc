/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Link.h"

#include "base/Text.h"
#include "ed2k/Hash.h"
#include "ed2k/Packet.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <limits>

namespace aria2::ed2k {

namespace {

constexpr int64_t maxFileSize = 0x4000000000LL;
constexpr std::string_view base32Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// Fields are split verbatim: names may begin or end with spaces.
std::vector<std::string> fieldsOf(std::string_view value, char separator)
{
  std::vector<std::string> fields;
  for (size_t begin = 0;;) {
    const auto end = value.find(separator, begin);
    if (const auto field = value.substr(begin, end - begin); !field.empty()) {
      fields.emplace_back(field);
    }
    if (end == std::string_view::npos) {
      return fields;
    }
    begin = end + 1;
  }
}

std::vector<std::string> splitFields(const std::string& uri)
{
  auto normalized = uri;
  if (text::istartsWith(normalized, "ed2k://%7C")) {
    normalized =
        text::replaceAll(text::replaceAll(normalized, "%7C", "|"), "%7c", "|");
  }
  return fieldsOf(normalized, '|');
}

uint16_t parsePort(const std::string& value)
{
  const auto port = text::toInt(value);
  if (!port || *port <= 0 || *port > std::numeric_limits<uint16_t>::max()) {
    throw ProtocolError(fmt::format("Bad ED2K port: {}", value));
  }
  return static_cast<uint16_t>(*port);
}

int64_t parseSize(const std::string& value)
{
  const auto size = text::toInt(value);
  if (!size || *size <= 0 || *size >= maxFileSize) {
    throw ProtocolError(fmt::format("Bad ED2K file size: {}", value));
  }
  return *size;
}

std::string sanitizeFileName(std::string name)
{
  std::replace(name.begin(), name.end(), '/', '_');
  std::replace(name.begin(), name.end(), '\\', '_');
  return name;
}

std::string parseHash(const std::string& value)
{
  auto hash =
      value.size() == HASH_LENGTH * 2 ? text::fromHex(value) : std::nullopt;
  if (!hash) {
    throw ProtocolError("Bad ED2K hash.");
  }
  return *hash;
}

std::string base32Decode(std::string_view value)
{
  std::string result;
  uint32_t buffer = 0;
  int bits = 0;
  for (const char ch : value) {
    const auto index = base32Alphabet.find(
        static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    if (index == std::string_view::npos) {
      throw ProtocolError("Bad ED2K AICH hash.");
    }
    buffer = (buffer << 5) | static_cast<uint32_t>(index);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      result += static_cast<char>((buffer >> bits) & 0xff);
    }
  }
  return result;
}

std::string base32Encode(std::string_view bytes)
{
  std::string result;
  uint32_t buffer = 0;
  int bits = 0;
  for (const char ch : bytes) {
    buffer = (buffer << 8) | static_cast<unsigned char>(ch);
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      result += base32Alphabet[(buffer >> bits) & 31];
    }
  }
  if (bits > 0) {
    result += base32Alphabet[(buffer << (5 - bits)) & 31];
  }
  return result;
}

std::string parseAichHash(const std::string& value)
{
  auto decoded = base32Decode(value);
  if (decoded.size() != AICH_HASH_LENGTH) {
    throw ProtocolError("Bad ED2K AICH hash.");
  }
  return decoded;
}

Endpoint parseLinkEndpoint(const std::string& value)
{
  const auto fields = fieldsOf(value, ':');
  if (fields.size() < 2 || fields.size() > 4) {
    throw ProtocolError(fmt::format("Bad ED2K endpoint: {}", value));
  }
  Endpoint endpoint;
  endpoint.host = text::percentDecode(fields[0]);
  endpoint.port = parsePort(fields[1]);
  if (fields.size() >= 3) {
    const auto options = text::toInt(fields[2]);
    if (!options || *options < 0 ||
        *options > std::numeric_limits<uint16_t>::max()) {
      throw ProtocolError(fmt::format("Bad ED2K crypt options: {}", fields[2]));
    }
    endpoint.cryptOptions = static_cast<uint16_t>(*options);
  }
  if (fields.size() == 4) {
    endpoint.userHash = parseHash(fields[3]);
  }
  return endpoint;
}

std::string endpointToLinkSource(const Endpoint& endpoint)
{
  auto value =
      text::percentEncode(endpoint.host) + ':' + std::to_string(endpoint.port);
  if (endpoint.cryptOptions != 0 || !endpoint.userHash.empty()) {
    value += ':' + std::to_string(endpoint.cryptOptions);
  }
  if (!endpoint.userHash.empty()) {
    if (endpoint.userHash.size() != HASH_LENGTH) {
      throw ProtocolError("Bad ED2K hash length.");
    }
    value += ':' + text::toHex(endpoint.userHash);
  }
  return value;
}

void parseFileOption(Link& link, const std::string& option)
{
  if (option.starts_with("p=")) {
    const auto hashes = fieldsOf(std::string_view(option).substr(2), ':');
    if (hashes.empty()) {
      throw ProtocolError("Bad ED2K part hash list.");
    }
    for (const auto& hash : hashes) {
      link.pieceHashes.push_back(parseHash(hash));
    }
  }
  else if (option.starts_with("h=")) {
    link.aichHash = parseAichHash(option.substr(2));
  }
  else if (option.starts_with("sources,")) {
    for (const auto& source :
         fieldsOf(std::string_view(option).substr(8), ',')) {
      link.sources.push_back(parseLinkEndpoint(source));
    }
  }
}

Link parseFileLink(const std::vector<std::string>& fields)
{
  if (fields.size() < 6 || fields.front() != "ed2k://" || fields[1] != "file") {
    throw ProtocolError("Malformed ED2K file link.");
  }
  Link link;
  link.type = LinkType::FILE;
  link.name = sanitizeFileName(text::percentDecode(fields[2]));
  link.size = parseSize(fields[3]);
  link.hash = parseHash(fields[4]);
  if (link.name.empty()) {
    throw ProtocolError("ED2K file link has empty file name.");
  }
  for (size_t i = 5; i < fields.size(); ++i) {
    if (fields[i] != "/") {
      parseFileOption(link, fields[i]);
    }
  }
  return link;
}

Link parseServerLink(const std::vector<std::string>& fields)
{
  if (fields.size() != 5 || fields.front() != "ed2k://" ||
      fields[1] != "server" || fields.back() != "/") {
    throw ProtocolError("Malformed ED2K server link.");
  }
  Link link;
  link.type = LinkType::SERVER;
  link.server.host = text::percentDecode(fields[2]);
  link.server.port = parsePort(fields[3]);
  if (link.server.host.empty()) {
    throw ProtocolError("ED2K server link has empty host.");
  }
  return link;
}

Link parseUrlLink(const std::vector<std::string>& fields, LinkType type,
                  const char* name)
{
  if (fields.size() != 4 || fields.front() != "ed2k://" || fields[1] != name ||
      fields.back() != "/") {
    throw ProtocolError(fmt::format("Malformed ED2K {} link.", name));
  }
  Link link;
  link.type = type;
  link.url = text::percentDecode(fields[2]);
  if (link.url.empty()) {
    throw ProtocolError(fmt::format("ED2K {} link has empty URL.", name));
  }
  return link;
}

Link parseSearchLink(const std::vector<std::string>& fields)
{
  if (fields.size() != 4 || fields.front() != "ed2k://" ||
      fields[1] != "search" || fields.back() != "/") {
    throw ProtocolError("Malformed ED2K search link.");
  }
  Link link;
  link.type = LinkType::SEARCH;
  link.name = text::percentDecode(fields[2]);
  if (link.name.empty()) {
    throw ProtocolError("ED2K search link has empty keyword.");
  }
  return link;
}

} // namespace

bool isLink(std::string_view uri) { return text::istartsWith(uri, "ed2k://"); }

Endpoint parseEndpoint(const std::string& value)
{
  const auto colon = value.rfind(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 == value.size()) {
    throw ProtocolError(fmt::format("Bad ED2K endpoint: {}", value));
  }
  Endpoint endpoint;
  endpoint.host = text::percentDecode(value.substr(0, colon));
  endpoint.port = parsePort(value.substr(colon + 1));
  return endpoint;
}

Link parseLink(const std::string& uri)
{
  const auto fields = splitFields(uri);
  if (fields.size() < 2 || fields.front() != "ed2k://") {
    throw ProtocolError("Malformed ED2K link.");
  }
  if (fields[1] == "file") {
    return parseFileLink(fields);
  }
  if (fields[1] == "server") {
    return parseServerLink(fields);
  }
  if (fields[1] == "serverlist") {
    return parseUrlLink(fields, LinkType::SERVER_LIST, "serverlist");
  }
  if (fields[1] == "nodeslist") {
    return parseUrlLink(fields, LinkType::NODES_LIST, "nodeslist");
  }
  if (fields[1] == "search") {
    return parseSearchLink(fields);
  }
  throw ProtocolError(fmt::format("Unsupported ED2K link type: {}", fields[1]));
}

std::string toFileLink(const Link& link)
{
  if (link.type != LinkType::FILE) {
    throw ProtocolError("Only ED2K file links can be serialized.");
  }
  auto uri = "ed2k://|file|" + text::percentEncode(link.name) + '|' +
             std::to_string(link.size) + '|' + text::toHex(link.hash) + '|';
  if (!link.pieceHashes.empty()) {
    uri += "p=";
    for (size_t i = 0; i < link.pieceHashes.size(); ++i) {
      uri += (i ? ":" : "") + text::toHex(link.pieceHashes[i]);
    }
    uri += '|';
  }
  if (!link.aichHash.empty()) {
    if (link.aichHash.size() != AICH_HASH_LENGTH) {
      throw ProtocolError("Bad ED2K AICH hash length.");
    }
    uri += "h=" + base32Encode(link.aichHash) + '|';
  }
  if (!link.sources.empty()) {
    uri += "sources,";
    for (size_t i = 0; i < link.sources.size(); ++i) {
      uri += (i ? "," : "") + endpointToLinkSource(link.sources[i]);
    }
    uri += '|';
  }
  return uri + '/';
}

} // namespace aria2::ed2k
