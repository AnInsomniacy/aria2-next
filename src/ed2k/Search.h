/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "ed2k/KadSearch.h"
#include "ed2k/Packet.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2::ed2k {

struct SearchQuery {
  std::string keyword;
  std::string fileType;
  std::string extension;
  int64_t minSize = 0;
  int64_t maxSize = 0;
  uint32_t minSourceCount = 0;
  uint32_t minCompleteSourceCount = 0;
};

struct SearchResultEntry {
  std::string hash;
  std::string name;
  int64_t size = 0;
  uint32_t sourceCount = 0;
  uint32_t completeSourceCount = 0;
  std::string fileType;
  std::string extension;
  std::string mediaArtist;
  std::string mediaAlbum;
  std::string mediaTitle;
  std::string mediaLength;
  uint32_t mediaBitrate = 0;
  std::string mediaCodec;
  std::string sourceNetwork;
  std::string ed2kLink;
  std::vector<Endpoint> sources;
  std::vector<Tag> tags;
};

struct SearchResult {
  std::vector<SearchResultEntry> entries;
  bool moreResults = false;
};

bool parseSearchResultPayload(SearchResult& result, const std::string& payload,
                              const std::string& sourceNetwork);
std::string createSearchRequestPayload(const SearchQuery& query,
                                       bool supports64Bit);
std::string pickKadKeyword(const std::string& query);
std::string createKadKeywordTarget(const std::string& query);
std::vector<SearchResultEntry>
kadSearchEntriesToSearchResults(const std::vector<KadSearchEntry>& entries,
                                const std::string& sourceNetwork);

} // namespace aria2::ed2k
