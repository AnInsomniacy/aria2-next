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
#ifndef D_ED2K_KAD_SEARCH_H
#define D_ED2K_KAD_SEARCH_H

#include "common.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

namespace ed2k {

struct KadSearchEntry {
  std::string id;
  std::vector<Tag> tags;
};

struct KadSearchSourcesRequest {
  std::string targetId;
  uint16_t startPosition = 0;
  uint64_t size = 0;
};

struct KadSearchResult {
  std::string sourceId;
  std::string targetId;
  std::vector<KadSearchEntry> entries;
};

struct KadPublishSourceRequest {
  std::string fileId;
  KadSearchEntry source;
};

KadSearchEntry readKadSearchEntry(const std::string& data, size_t& offset);
std::string packKadSearchEntry(const KadSearchEntry& entry);
std::string createKadSearchSourcesRequestPayload(const std::string& targetId,
                                                 uint16_t startPosition,
                                                 uint64_t size);
bool parseKadSearchSourcesRequestPayload(KadSearchSourcesRequest& request,
                                         const std::string& payload);
std::string createKadSearchKeysRequestPayload(const std::string& targetId,
                                              uint16_t startPosition);
bool parseKadSearchResultPayload(KadSearchResult& result,
                                 const std::string& payload);
std::string createKadSearchResultPayload(const std::string& sourceId,
                                         const std::string& targetId,
                                         const std::vector<KadSearchEntry>&
                                             entries);
bool extractKadSourceEndpoint(Endpoint& endpoint,
                              const KadSearchEntry& entry);
std::vector<Endpoint> extractKadSourceEndpoints(const KadSearchResult& result);
std::string createKadPublishSourceRequestPayload(const std::string& fileId,
                                                 const Endpoint& source,
                                                 const std::string& sourceId,
                                                 uint64_t size = 0);
bool parseKadPublishSourceRequestPayload(KadPublishSourceRequest& request,
                                         const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_KAD_SEARCH_H
