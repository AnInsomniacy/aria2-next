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
#ifndef D_TORRENT_METADATA_FOLLOW_H
#define D_TORRENT_METADATA_FOLLOW_H

#include <memory>
#include <string>
#include <vector>

#include "common.h"

namespace aria2 {

class RequestGroup;

bool isTorrentMetadataResponse(const std::string& path,
                               const std::string& contentType);

bool createTorrentMetadataFollowGroups(
    std::vector<std::shared_ptr<RequestGroup>>& groups, RequestGroup* group);

} // namespace aria2

#endif // D_TORRENT_METADATA_FOLLOW_H
