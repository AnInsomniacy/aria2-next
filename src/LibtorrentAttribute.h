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
#ifndef D_LIBTORRENT_ATTRIBUTE_H
#define D_LIBTORRENT_ATTRIBUTE_H

#include "ContextAttribute.h"

#include <memory>
#include <string>
#include <vector>

namespace aria2 {

class DownloadContext;

struct LibtorrentAttribute : public ContextAttribute {
  enum class SourceType { TORRENT_FILE, TORRENT_DATA, MAGNET };

  SourceType sourceType;
  std::string sourceUri;
  std::string torrentData;
  std::vector<std::string> webSeedUris;

  LibtorrentAttribute(SourceType sourceType, std::string sourceUri,
                      std::string torrentData,
                      std::vector<std::string> webSeedUris);
  ~LibtorrentAttribute();

  LibtorrentAttribute(const LibtorrentAttribute&) = delete;
  LibtorrentAttribute& operator=(const LibtorrentAttribute&) = delete;
};

LibtorrentAttribute* getLibtorrentAttrs(DownloadContext* dctx);
LibtorrentAttribute*
getLibtorrentAttrs(const std::shared_ptr<DownloadContext>& dctx);

} // namespace aria2

#endif // D_LIBTORRENT_ATTRIBUTE_H
