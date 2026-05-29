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
#include "TorrentMetadataFollow.h"

#include "AbstractSingleDiskAdaptor.h"
#include "ByteArrayDiskWriter.h"
#include "DiskAdaptor.h"
#include "DownloadContext.h"
#include "FileEntry.h"
#include "Option.h"
#include "PieceStorage.h"
#include "RequestGroup.h"
#include "download_helper.h"
#include "prefs.h"
#include "util.h"

namespace aria2 {
namespace {

constexpr const char TORRENT_CONTENT_TYPE[] = "application/x-bittorrent";

std::string normalizeContentType(std::string contentType)
{
  auto sep = contentType.find(';');
  if (sep != std::string::npos) {
    contentType.erase(sep);
  }
  contentType = util::strip(contentType);
  util::lowercase(contentType);
  return contentType;
}

std::string readTorrentMetadata(RequestGroup* group)
{
  auto adaptor = group->getPieceStorage()->getDiskAdaptor();
  if (group->inMemoryDownload()) {
    auto single = static_cast<AbstractSingleDiskAdaptor*>(adaptor.get());
    auto writer =
        static_cast<ByteArrayDiskWriter*>(single->getDiskWriter().get());
    return writer->getString();
  }

  adaptor->openExistingFile();
  try {
    auto data = util::toString(adaptor);
    adaptor->closeFile();
    return data;
  }
  catch (...) {
    adaptor->closeFile();
    throw;
  }
}

} // namespace

bool isTorrentMetadataResponse(const std::string& path,
                               const std::string& contentType)
{
  return util::iendsWith(path, ".torrent") ||
         normalizeContentType(contentType) == TORRENT_CONTENT_TYPE;
}

bool createTorrentMetadataFollowGroups(
    std::vector<std::shared_ptr<RequestGroup>>& groups, RequestGroup* group)
{
#ifdef ENABLE_BITTORRENT
  if (!group || group->getDownloadContext()->getFileEntries().size() != 1 ||
      group->getOption()->get(PREF_TORRENT_METADATA) == "save") {
    return false;
  }

  const auto& entry = group->getDownloadContext()->getFirstFileEntry();
  if (!isTorrentMetadataResponse(entry->getPath(), entry->getContentType())) {
    return false;
  }

  createRequestGroupForBitTorrent(
      groups, group->getOption(), {}, group->inMemoryDownload()
                                      ? std::string()
                                      : group->getFirstFilePath(),
      readTorrentMetadata(group));
  group->followedBy(std::begin(groups), std::end(groups));
  for (auto& rg : groups) {
    rg->following(group->getGID());
  }
  auto mi = createMetadataInfoFromFirstFileEntry(group->getGroupId(),
                                                group->getDownloadContext());
  if (mi) {
    setMetadataInfo(std::begin(groups), std::end(groups), mi);
  }
  return !groups.empty();
#else  // !ENABLE_BITTORRENT
  return false;
#endif // !ENABLE_BITTORRENT
}

} // namespace aria2
