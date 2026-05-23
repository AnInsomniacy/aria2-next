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
#include "LibtorrentAttribute.h"

#include "DownloadContext.h"

#include <utility>

namespace aria2 {

LibtorrentAttribute::LibtorrentAttribute(SourceType sourceType,
                                         std::string sourceUri,
                                         std::string torrentData,
                                         std::vector<std::string> webSeedUris)
    : sourceType(sourceType),
      sourceUri(std::move(sourceUri)),
      torrentData(std::move(torrentData)),
      webSeedUris(std::move(webSeedUris))
{
}

LibtorrentAttribute::~LibtorrentAttribute() = default;

LibtorrentAttribute* getLibtorrentAttrs(DownloadContext* dctx)
{
  return static_cast<LibtorrentAttribute*>(
      dctx->getAttribute(CTX_ATTR_LIBTORRENT).get());
}

LibtorrentAttribute*
getLibtorrentAttrs(const std::shared_ptr<DownloadContext>& dctx)
{
  return getLibtorrentAttrs(dctx.get());
}

} // namespace aria2
