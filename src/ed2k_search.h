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
#ifndef D_ED2K_SEARCH_H
#define D_ED2K_SEARCH_H

#include "common.h"
#include "ed2k_kad_search.h"
#include "ed2k_packet.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

namespace ed2k {

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
std::vector<SearchResultEntry> kadSearchEntriesToSearchResults(
    const std::vector<KadSearchEntry>& entries,
    const std::string& sourceNetwork);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_SEARCH_H
