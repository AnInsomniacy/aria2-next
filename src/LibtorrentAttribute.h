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

  struct Peer {
    std::string peerId;
    std::string ip;
    int port = 0;
    std::string bitfield;
    int downloadSpeed = 0;
    int uploadSpeed = 0;
    bool amChoking = false;
    bool peerChoking = false;
    bool seeder = false;
  };

  struct Status {
    bool hasStatus = false;
    bool complete = false;
    bool seeding = false;
    bool sharing = false;
    bool checking = false;
    bool hasMetadata = false;
    int64_t totalLength = 0;
    int64_t completedLength = 0;
    int64_t uploadedLength = 0;
    int downloadSpeed = 0;
    int uploadSpeed = 0;
    int connections = 0;
    int seeders = 0;
    std::string bitfield;
    std::string infoHash;
    std::string name;
  };

  SourceType sourceType;
  std::string sourceUri;
  std::string torrentData;
  std::vector<std::string> webSeedUris;
  std::vector<std::string> trackerUris;
  std::vector<int> trackerTiers;
  std::string selectedFiles;
  std::vector<int> filePriorities;
  bool filePrioritiesApplied = false;
  Status status;
  Status resumeStatus;
  std::vector<Peer> peers;
  std::string resumeData;
  std::string controlFilePath;
  std::string infoHash;
  bool pauseAfterMetadata = false;
  bool metadataPauseApplied = false;

  LibtorrentAttribute(SourceType sourceType, std::string sourceUri,
                      std::string torrentData,
                      std::vector<std::string> webSeedUris,
                      std::string controlFilePath, std::string infoHash = "");
  ~LibtorrentAttribute();

  LibtorrentAttribute(const LibtorrentAttribute&) = delete;
  LibtorrentAttribute& operator=(const LibtorrentAttribute&) = delete;

  bool hasResumeData() const;
  const std::string& getResumeData() const;
  void setResumeData(std::string data);
  std::string takeResumeData();
};

LibtorrentAttribute* getLibtorrentAttrs(DownloadContext* dctx);
LibtorrentAttribute*
getLibtorrentAttrs(const std::shared_ptr<DownloadContext>& dctx);

} // namespace aria2

#endif // D_LIBTORRENT_ATTRIBUTE_H
