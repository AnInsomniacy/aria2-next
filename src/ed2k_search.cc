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
#include "ed2k_search.h"

#include <algorithm>
#include <limits>

#include "DlAbortEx.h"
#include "ed2k_endpoint.h"
#include "ed2k_hash.h"
#include "ed2k_link.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

namespace {

void appendSearchAnd(std::string& payload)
{
  payload.push_back('\0');
  payload.push_back('\0');
}

void appendSearchString(std::string& payload, const std::string& value)
{
  if (value.empty()) {
    throw DL_ABORT_EX("ED2K search keyword is empty.");
  }
  if (value.size() > 450) {
    throw DL_ABORT_EX("ED2K search keyword is too long.");
  }
  payload.push_back('\x01');
  payload += packUInt16(static_cast<uint16_t>(value.size()));
  payload += value;
}

void appendSearchTaggedString(std::string& payload, uint8_t tag,
                              const std::string& value)
{
  if (value.size() > 20) {
    throw DL_ABORT_EX("ED2K search tag value is too long.");
  }
  payload.push_back('\x02');
  payload += packUInt16(static_cast<uint16_t>(value.size()));
  payload += value;
  payload += packUInt16(1);
  payload.push_back(static_cast<char>(tag));
}

void appendSearchTaggedNumber(std::string& payload, uint8_t tag,
                              uint8_t searchOperator, uint64_t value,
                              bool supports64Bit)
{
  if (value > std::numeric_limits<uint32_t>::max() && supports64Bit) {
    payload.push_back('\x08');
    payload += packUInt64(value);
  }
  else {
    payload.push_back('\x03');
    payload += packUInt32(static_cast<uint32_t>(
        std::min<uint64_t>(value, std::numeric_limits<uint32_t>::max())));
  }
  payload.push_back(static_cast<char>(searchOperator));
  payload += packUInt16(1);
  payload.push_back(static_cast<char>(tag));
}

void applySearchTag(SearchResultEntry& entry, const Tag& tag, uint64_t& sizeLow,
                    uint64_t& sizeHigh)
{
  if (tag.valueType == TagValueType::STRING) {
    if (tag.id == 0x01) {
      entry.name = tag.stringValue;
    }
    else if (tag.id == 0x03) {
      entry.fileType = tag.stringValue;
    }
    else if (tag.id == 0x04) {
      entry.extension = tag.stringValue;
    }
    else if (tag.id == 0xd0 || tag.name == "Artist") {
      entry.mediaArtist = tag.stringValue;
    }
    else if (tag.id == 0xd1 || tag.name == "Album") {
      entry.mediaAlbum = tag.stringValue;
    }
    else if (tag.id == 0xd2 || tag.name == "Title") {
      entry.mediaTitle = tag.stringValue;
    }
    else if (tag.id == 0xd3 || tag.name == "length") {
      entry.mediaLength = tag.stringValue;
    }
    else if (tag.id == 0xd5 || tag.name == "codec") {
      entry.mediaCodec = tag.stringValue;
    }
  }
  else if (tag.valueType == TagValueType::UINT) {
    if (tag.id == 0x02) {
      sizeLow = tag.intValue;
    }
    else if (tag.id == 0x3a) {
      sizeHigh = tag.intValue;
    }
    else if (tag.id == 0x15) {
      entry.sourceCount = static_cast<uint32_t>(tag.intValue);
    }
    else if (tag.id == 0x30) {
      entry.completeSourceCount = static_cast<uint32_t>(tag.intValue);
    }
    else if (tag.id == 0xd3) {
      entry.mediaLength = util::uitos(tag.intValue);
    }
    else if (tag.id == 0xd4 || tag.name == "bitrate") {
      entry.mediaBitrate = static_cast<uint32_t>(tag.intValue);
    }
  }
}

bool finishSearchResultEntry(SearchResultEntry& entry, uint64_t sizeLow,
                             uint64_t sizeHigh,
                             const std::string& sourceNetwork)
{
  const uint64_t size = sizeLow | (sizeHigh << 32);
  if (size > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    return false;
  }
  entry.size = static_cast<int64_t>(size);
  entry.sourceNetwork = sourceNetwork;
  Link link;
  link.type = LinkType::FILE;
  link.name = entry.name;
  link.size = entry.size;
  link.hash = entry.hash;
  link.sources = entry.sources;
  entry.ed2kLink = toFileLink(link);
  return true;
}

} // namespace

bool parseSearchResultPayload(SearchResult& result, const std::string& payload,
                              const std::string& sourceNetwork)
{
  if (payload.size() < 4) {
    return false;
  }
  size_t offset = 0;
  const auto count = readUInt32(readBytes(payload, offset, 4).data());
  if (count > 10000) {
    return false;
  }
  result.entries.clear();
  result.entries.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (payload.size() - offset < HASH_LENGTH + 6) {
      return false;
    }
    SearchResultEntry entry;
    entry.hash = readBytes(payload, offset, HASH_LENGTH);
    const auto clientId =
        readUInt32(readBytes(payload, offset, 4).data());
    const auto clientPort =
        readUInt16(readBytes(payload, offset, 2).data());
    if (clientId > 0x00ffffffu && clientPort != 0) {
      Endpoint source;
      source.host = ipv4FromEndpoint(clientId);
      source.port = clientPort;
      entry.sources.push_back(source);
    }
    std::vector<Tag> tags;
    const auto tagCount = readUInt32(readBytes(payload, offset, 4).data());
    std::string tagPayload;
    tagPayload += packUInt32(tagCount);
    for (uint32_t tagIndex = 0; tagIndex < tagCount; ++tagIndex) {
      const auto before = offset;
      readTag(payload, offset);
      tagPayload.append(payload.begin() + before, payload.begin() + offset);
    }
    if (!parseTagList(tags, tagPayload)) {
      return false;
    }
    entry.tags = tags;
    uint64_t sizeLow = 0;
    uint64_t sizeHigh = 0;
    for (const auto& tag : tags) {
      applySearchTag(entry, tag, sizeLow, sizeHigh);
    }
    if (!finishSearchResultEntry(entry, sizeLow, sizeHigh, sourceNetwork)) {
      return false;
    }
    result.entries.push_back(std::move(entry));
  }
  result.moreResults = false;
  if (payload.size() - offset == 1) {
    result.moreResults = readByte(payload, offset) != 0;
  }
  return offset == payload.size();
}

std::string pickKadKeyword(const std::string& query)
{
  auto value = util::toLower(query);
  std::string best;
  std::string current;
  for (const auto c : value) {
    if (util::isAlpha(c) || util::isDigit(c)) {
      current.push_back(c);
      continue;
    }
    if (current.size() >= 3 && current.size() > best.size()) {
      best = current;
    }
    current.clear();
  }
  if (current.size() >= 3 && current.size() > best.size()) {
    best = current;
  }
  return best;
}

std::string createKadKeywordTarget(const std::string& query)
{
  auto keyword = pickKadKeyword(query);
  if (keyword.empty()) {
    throw DL_ABORT_EX("ED2K Kad search keyword is empty.");
  }
  return md4Digest(keyword);
}

std::string createSearchRequestPayload(const SearchQuery& query,
                                       bool supports64Bit)
{
  struct SearchTerm {
    enum class Type { KEYWORD, STRING_TAG, NUMBER_TAG };
    Type type = Type::KEYWORD;
    std::string text;
    uint8_t tag = 0;
    uint8_t searchOperator = 0;
    uint64_t number = 0;
  };

  std::vector<SearchTerm> terms;
  SearchTerm keyword;
  keyword.type = SearchTerm::Type::KEYWORD;
  keyword.text = query.keyword;
  terms.push_back(keyword);
  if (!query.fileType.empty()) {
    SearchTerm term;
    term.type = SearchTerm::Type::STRING_TAG;
    term.tag = 0x03;
    term.text = query.fileType;
    terms.push_back(term);
  }
  if (query.minSize > 0) {
    SearchTerm term;
    term.type = SearchTerm::Type::NUMBER_TAG;
    term.tag = 0x02;
    term.searchOperator = 0x01;
    term.number = static_cast<uint64_t>(query.minSize);
    terms.push_back(term);
  }
  if (query.maxSize > 0) {
    SearchTerm term;
    term.type = SearchTerm::Type::NUMBER_TAG;
    term.tag = 0x02;
    term.searchOperator = 0x02;
    term.number = static_cast<uint64_t>(query.maxSize);
    terms.push_back(term);
  }
  if (query.minSourceCount > 0) {
    SearchTerm term;
    term.type = SearchTerm::Type::NUMBER_TAG;
    term.tag = 0x15;
    term.searchOperator = 0x01;
    term.number = query.minSourceCount;
    terms.push_back(term);
  }
  if (query.minCompleteSourceCount > 0) {
    SearchTerm term;
    term.type = SearchTerm::Type::NUMBER_TAG;
    term.tag = 0x30;
    term.searchOperator = 0x01;
    term.number = query.minCompleteSourceCount;
    terms.push_back(term);
  }
  if (!query.extension.empty()) {
    SearchTerm term;
    term.type = SearchTerm::Type::STRING_TAG;
    term.tag = 0x04;
    term.text = query.extension;
    terms.push_back(term);
  }
  if (terms.size() > 30) {
    throw DL_ABORT_EX("Too many ED2K search terms.");
  }

  std::string payload;
  for (size_t i = 0; i < terms.size(); ++i) {
    if (i + 1 < terms.size()) {
      appendSearchAnd(payload);
    }
    const auto& term = terms[i];
    switch (term.type) {
    case SearchTerm::Type::KEYWORD:
      appendSearchString(payload, term.text);
      break;
    case SearchTerm::Type::STRING_TAG:
      appendSearchTaggedString(payload, term.tag, term.text);
      break;
    case SearchTerm::Type::NUMBER_TAG:
      appendSearchTaggedNumber(payload, term.tag, term.searchOperator,
                               term.number, supports64Bit);
      break;
    }
  }
  return payload;
}

std::vector<SearchResultEntry> kadSearchEntriesToSearchResults(
    const std::vector<KadSearchEntry>& entries,
    const std::string& sourceNetwork)
{
  std::vector<SearchResultEntry> results;
  results.reserve(entries.size());
  for (const auto& item : entries) {
    if (item.id.size() != HASH_LENGTH) {
      continue;
    }
    SearchResultEntry entry;
    entry.hash = item.id;
    entry.tags = item.tags;
    uint64_t sizeLow = 0;
    uint64_t sizeHigh = 0;
    for (const auto& tag : item.tags) {
      applySearchTag(entry, tag, sizeLow, sizeHigh);
    }
    if (!entry.name.empty() &&
        finishSearchResultEntry(entry, sizeLow, sizeHigh, sourceNetwork)) {
      results.push_back(std::move(entry));
    }
  }
  return results;
}

} // namespace ed2k

} // namespace aria2
