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
#include "ed2k_link.h"

#include <limits>

#include "DlAbortEx.h"
#include "base32.h"
#include "ed2k_hash.h"
#include "fmt.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

namespace {

std::vector<std::string> splitFields(const std::string& uri)
{
  auto normalized = uri;
  if (util::startsWith(normalized, "ed2k://%7C") ||
      util::startsWith(normalized, "ed2k://%7c")) {
    normalized = util::replace(normalized, "%7C", "|");
    normalized = util::replace(normalized, "%7c", "|");
  }
  std::vector<std::string> fields;
  std::vector<Scip> parts;
  util::splitIter(normalized.begin(), normalized.end(),
                  std::back_inserter(parts), '|');
  for (const auto& part : parts) {
    fields.emplace_back(part.first, part.second);
  }
  return fields;
}

std::string percentDecode(const std::string& value)
{
  return util::percentDecode(value.begin(), value.end());
}

uint16_t parsePort(const std::string& value)
{
  uint32_t port = 0;
  if (!util::parseUIntNoThrow(port, value) || port == 0 ||
      port > std::numeric_limits<uint16_t>::max()) {
    throw DL_ABORT_EX(fmt("Bad ED2K port: %s", value.c_str()));
  }
  return static_cast<uint16_t>(port);
}

int64_t parseSize(const std::string& value)
{
  int64_t size = 0;
  if (!util::parseLLIntNoThrow(size, value) || size <= 0) {
    throw DL_ABORT_EX(fmt("Bad ED2K file size: %s", value.c_str()));
  }
  return size;
}

std::string parseHash(const std::string& value)
{
  if (value.size() != HASH_LENGTH * 2 || !util::isHexDigit(value)) {
    throw DL_ABORT_EX("Bad ED2K hash.");
  }
  return util::fromHex(value.begin(), value.end());
}

void validateHashLength(const std::string& hash)
{
  if (hash.size() != HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K hash length.");
  }
}

std::string parseAichHash(const std::string& value)
{
  auto normalized = util::toUpper(value);
  auto decoded = base32::decode(normalized.begin(), normalized.end());
  if (decoded.size() != AICH_HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K AICH hash.");
  }
  return normalized;
}

Endpoint parseLinkEndpoint(const std::string& value)
{
  std::vector<Scip> fields;
  util::splitIter(value.begin(), value.end(), std::back_inserter(fields), ':');
  if (fields.size() < 2 || fields.size() > 4) {
    throw DL_ABORT_EX(fmt("Bad ED2K endpoint: %s", value.c_str()));
  }
  std::string host(fields[0].first, fields[0].second);
  std::string port(fields[1].first, fields[1].second);
  if (host.empty() || port.empty()) {
    throw DL_ABORT_EX(fmt("Bad ED2K endpoint: %s", value.c_str()));
  }
  Endpoint endpoint;
  endpoint.host = percentDecode(host);
  endpoint.port = parsePort(port);
  if (fields.size() >= 3) {
    std::string cryptOptions(fields[2].first, fields[2].second);
    uint32_t value = 0;
    if (cryptOptions.empty() || !util::parseUIntNoThrow(value, cryptOptions) ||
        value > std::numeric_limits<uint16_t>::max()) {
      throw DL_ABORT_EX(fmt("Bad ED2K crypt options: %s",
                            cryptOptions.c_str()));
    }
    endpoint.cryptOptions = static_cast<uint16_t>(value);
  }
  if (fields.size() >= 4) {
    endpoint.userHash = parseHash(std::string(fields[3].first,
                                              fields[3].second));
  }
  return endpoint;
}

std::string endpointToLinkSource(const Endpoint& endpoint)
{
  std::string value = util::percentEncode(endpoint.host);
  value += ":";
  value += util::uitos(endpoint.port);
  if (endpoint.cryptOptions != 0 || !endpoint.userHash.empty()) {
    value += ":";
    value += util::uitos(endpoint.cryptOptions);
  }
  if (!endpoint.userHash.empty()) {
    validateHashLength(endpoint.userHash);
    value += ":";
    value += util::toHex(endpoint.userHash);
  }
  return value;
}

void parseFileOption(Link& link, const std::string& option)
{
  if (util::startsWith(option, "p=")) {
    std::vector<Scip> hashes;
    util::splitIter(option.begin() + 2, option.end(), std::back_inserter(hashes),
                    ':');
    for (const auto& hash : hashes) {
      link.pieceHashes.push_back(parseHash(std::string(hash.first, hash.second)));
    }
  }
  else if (util::startsWith(option, "h=")) {
    link.aichHash = parseAichHash(std::string(option.begin() + 2,
                                              option.end()));
  }
  else if (util::startsWith(option, "sources,")) {
    std::vector<Scip> sources;
    util::splitIter(option.begin() + 8, option.end(),
                    std::back_inserter(sources), ',');
    for (const auto& source : sources) {
      link.sources.push_back(
          parseLinkEndpoint(std::string(source.first, source.second)));
    }
  }
}

Link parseFileLink(const std::vector<std::string>& fields)
{
  if (fields.size() < 6 || fields.front() != "ed2k://" || fields[1] != "file") {
    throw DL_ABORT_EX("Malformed ED2K file link.");
  }
  Link link;
  link.type = LinkType::FILE;
  link.name = percentDecode(fields[2]);
  link.size = parseSize(fields[3]);
  link.hash = parseHash(fields[4]);
  if (link.name.empty()) {
    throw DL_ABORT_EX("ED2K file link has empty file name.");
  }
  for (size_t i = 5; i < fields.size(); ++i) {
    if (fields[i] == "/") {
      continue;
    }
    parseFileOption(link, fields[i]);
  }
  return link;
}

Link parseServerLink(const std::vector<std::string>& fields)
{
  if (fields.size() != 5 || fields.front() != "ed2k://" ||
      fields[1] != "server" || fields.back() != "/") {
    throw DL_ABORT_EX("Malformed ED2K server link.");
  }
  Link link;
  link.type = LinkType::SERVER;
  link.server.host = percentDecode(fields[2]);
  link.server.port = parsePort(fields[3]);
  if (link.server.host.empty()) {
    throw DL_ABORT_EX("ED2K server link has empty host.");
  }
  return link;
}

Link parseUrlLink(const std::vector<std::string>& fields, LinkType type,
                  const char* name)
{
  if (fields.size() != 4 || fields.front() != "ed2k://" ||
      fields[1] != name || fields.back() != "/") {
    throw DL_ABORT_EX(fmt("Malformed ED2K %s link.", name));
  }
  Link link;
  link.type = type;
  link.url = percentDecode(fields[2]);
  if (link.url.empty()) {
    throw DL_ABORT_EX(fmt("ED2K %s link has empty URL.", name));
  }
  return link;
}

} // namespace

Endpoint parseEndpoint(const std::string& value)
{
  auto p = util::divide(value.begin(), value.end(), ':');
  std::string host(p.first.first, p.first.second);
  std::string port(p.second.first, p.second.second);
  if (host.empty() || port.empty()) {
    throw DL_ABORT_EX(fmt("Bad ED2K endpoint: %s", value.c_str()));
  }
  Endpoint endpoint;
  endpoint.host = percentDecode(host);
  endpoint.port = parsePort(port);
  return endpoint;
}

Link parseLink(const std::string& uri)
{
  auto fields = splitFields(uri);
  if (fields.size() < 2 || fields.front() != "ed2k://") {
    throw DL_ABORT_EX("Malformed ED2K link.");
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
  throw DL_ABORT_EX(fmt("Unsupported ED2K link type: %s", fields[1].c_str()));
}

std::string toFileLink(const Link& link)
{
  if (link.type != LinkType::FILE) {
    throw DL_ABORT_EX("Only ED2K file links can be serialized.");
  }
  std::string uri = "ed2k://|file|";
  uri += util::percentEncode(link.name);
  uri += "|";
  uri += util::uitos(link.size);
  uri += "|";
  uri += util::toHex(link.hash);
  uri += "|";
  if (!link.pieceHashes.empty()) {
    uri += "p=";
    for (size_t i = 0; i < link.pieceHashes.size(); ++i) {
      if (i != 0) {
        uri += ":";
      }
      uri += util::toHex(link.pieceHashes[i]);
    }
    uri += "|";
  }
  if (!link.aichHash.empty()) {
    uri += "h=";
    uri += link.aichHash;
    uri += "|";
  }
  if (!link.sources.empty()) {
    uri += "sources,";
    for (size_t i = 0; i < link.sources.size(); ++i) {
      if (i != 0) {
        uri += ",";
      }
      uri += endpointToLinkSource(link.sources[i]);
    }
    uri += "|";
  }
  uri += "/";
  return uri;
}

} // namespace ed2k

} // namespace aria2
