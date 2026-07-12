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
#include "Ed2kShareIndex.h"

#include <algorithm>
#include <limits>

#include "DefaultDiskWriter.h"
#include "DiskWriter.h"
#include "DownloadContext.h"
#include "Ed2kAttribute.h"
#include "FileEntry.h"
#include "PieceStorage.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "ed2k_hash.h"
#include "ed2k_packet.h"

namespace aria2 {

namespace ed2k {

namespace {

bool readDiskRange(const std::string& path, std::string& data, int64_t begin,
                   int64_t end)
{
  if (path.empty() || begin < 0 || end <= begin ||
      end - begin > static_cast<int64_t>(BLOCK_LENGTH)) {
    return false;
  }
  const auto length = static_cast<size_t>(end - begin);
  std::shared_ptr<DiskWriter> writer(new DefaultDiskWriter(path));
  writer->enableReadOnly();
  writer->openExistingFile();
  if (end > writer->size()) {
    return false;
  }
  data.assign(length, '\0');
  const auto read = writer->readData(
      reinterpret_cast<unsigned char*>(&data[0]), length, begin);
  return read == static_cast<ssize_t>(length);
}

class ActiveSharedSource : public SharedSource {
private:
  std::string hash_;
  std::string aichRootHash_;
  std::vector<std::string> pieceHashes_;
  std::string name_;
  std::string path_;
  int64_t size_;
  int64_t pieceLength_;
  DownloadContext* dctx_;
  PieceStorage* pieceStorage_;

  bool verifiedRange(int64_t begin, int64_t end) const
  {
    if (!pieceStorage_ || begin < 0 || end <= begin || end > size_ ||
        end - begin > static_cast<int64_t>(BLOCK_LENGTH)) {
      return false;
    }
    const auto first = static_cast<size_t>(begin / pieceLength_);
    const auto last = static_cast<size_t>((end - 1) / pieceLength_);
    for (size_t index = first; index <= last; ++index) {
      if (!pieceStorage_->hasPiece(index)) {
        return false;
      }
    }
    return true;
  }

public:
  ActiveSharedSource(const Ed2kAttribute& attrs, DownloadContext* dctx,
                     PieceStorage* pieceStorage, std::string path)
      : hash_(attrs.link.hash),
        aichRootHash_(attrs.aichRootHash),
        pieceHashes_(attrs.pieceHashes),
        name_(attrs.link.name),
        path_(std::move(path)),
        size_(attrs.link.size),
        pieceLength_(dctx ? dctx->getPieceLength() : PIECE_LENGTH),
        dctx_(dctx),
        pieceStorage_(pieceStorage)
  {
    if (name_.empty() && dctx && !dctx->getFileEntries().empty()) {
      name_ = dctx->getFileEntries()[0]->getBasename();
    }
  }

  const std::string& hash() const override { return hash_; }
  const std::string& aichRootHash() const override
  {
    return aichRootHash_;
  }
  const std::vector<std::string>& pieceHashes() const override
  {
    return pieceHashes_;
  }
  const std::string& name() const override { return name_; }
  int64_t size() const override { return size_; }
  bool complete() const override
  {
    return pieceStorage_ && pieceStorage_->downloadFinished();
  }
  std::vector<bool> bitfield() const override
  {
    std::vector<bool> bits;
    if (!pieceStorage_ || pieceLength_ <= 0 || size_ <= 0) {
      return bits;
    }
    bits.resize(static_cast<size_t>((size_ + pieceLength_ - 1) / pieceLength_));
    for (size_t index = 0; index < bits.size(); ++index) {
      bits[index] = pieceStorage_->hasPiece(index);
    }
    return bits;
  }
  bool readRange(std::string& data, int64_t begin,
                 int64_t end) const override
  {
    return verifiedRange(begin, end) && readDiskRange(path_, data, begin, end);
  }
  void recordUpload(size_t bytes) override
  {
    if (!dctx_ || bytes == 0) {
      return;
    }
    dctx_->updateUploadSpeed(bytes);
    dctx_->updateUploadLength(bytes);
  }
};

bool isSingleFileEd2k(DownloadContext* dctx, const Ed2kAttribute* attrs)
{
  return dctx && attrs && dctx->hasAttribute(CTX_ATTR_ED2K) &&
         dctx->getFileEntries().size() == 1 &&
         attrs->link.type == LinkType::FILE &&
         attrs->link.hash.size() == HASH_LENGTH && attrs->link.size > 0;
}

std::string firstFilePath(DownloadContext* dctx)
{
  if (!dctx || dctx->getFileEntries().empty() || !dctx->getFileEntries()[0]) {
    return "";
  }
  return dctx->getFileEntries()[0]->getPath();
}

std::unique_ptr<SharedSource> sourceFromGroup(RequestGroup* group)
{
  if (!group || group->isHaltRequested()) {
    return nullptr;
  }
  auto dctx = group->getDownloadContext().get();
  auto attrs = getEd2kAttrs(dctx);
  if (!isSingleFileEd2k(dctx, attrs) || !group->getPieceStorage()) {
    return nullptr;
  }
  return createActiveSharedSource(dctx, group->getPieceStorage().get(),
                                  firstFilePath(dctx));
}

} // namespace

std::unique_ptr<SharedSource>
createActiveSharedSource(DownloadContext* dctx, PieceStorage* pieceStorage,
                         const std::string& path)
{
  auto attrs = getEd2kAttrs(dctx);
  if (!isSingleFileEd2k(dctx, attrs) || !pieceStorage || path.empty()) {
    return nullptr;
  }
  return make_unique<ActiveSharedSource>(*attrs, dctx, pieceStorage, path);
}

std::unique_ptr<SharedSource>
findSharedSource(RequestGroupMan* rgman, const std::string& hash)
{
  if (!rgman || hash.size() != HASH_LENGTH) {
    return nullptr;
  }
  for (const auto& group : rgman->getRequestGroups()) {
    auto source = sourceFromGroup(group.get());
    if (source && source->hash() == hash) {
      return source;
    }
  }
  return nullptr;
}

std::vector<std::shared_ptr<SharedSource>>
listSharedSources(RequestGroupMan* rgman)
{
  std::vector<std::shared_ptr<SharedSource>> sources;
  if (!rgman) {
    return sources;
  }
  for (const auto& group : rgman->getRequestGroups()) {
    auto source = sourceFromGroup(group.get());
    if (source) {
      sources.push_back(std::shared_ptr<SharedSource>(std::move(source)));
    }
  }
  return sources;
}

bool hasSharedSources(RequestGroupMan* rgman)
{
  if (!rgman) {
    return false;
  }
  for (const auto& group : rgman->getRequestGroups()) {
    if (sourceFromGroup(group.get())) {
      return true;
    }
  }
  return false;
}

bool createOfferFilesPayload(
    std::string& payload,
    const std::vector<std::shared_ptr<SharedSource>>& sources,
    bool supportsLargeFiles, size_t limit, uint32_t clientId,
    uint16_t clientPort)
{
  payload.clear();
  payload += packUInt32(0);
  uint32_t count = 0;
  for (const auto& source : sources) {
    if (!source || source->hash().size() != HASH_LENGTH ||
        source->name().empty() || source->size() <= 0) {
      continue;
    }
    if (count >= limit) {
      break;
    }
    if (source->size() > std::numeric_limits<uint32_t>::max() &&
        !supportsLargeFiles) {
      continue;
    }
    payload += source->hash();
    payload += packUInt32(clientId);
    payload += packUInt16(clientPort);

    std::string tags;
    uint32_t tagCount = 2;
    tags += createStringTag(0x01, source->name());
    if (source->size() > std::numeric_limits<uint32_t>::max()) {
      tags += createUInt32Tag(0x02, static_cast<uint32_t>(source->size()));
      tags += createUInt32Tag(
          0x3a, static_cast<uint32_t>(
                    static_cast<uint64_t>(source->size()) >> 32));
      ++tagCount;
    }
    else {
      tags += createUInt32Tag(0x02, static_cast<uint32_t>(source->size()));
    }
    payload += packUInt32(tagCount);
    payload += tags;
    ++count;
  }
  payload.replace(0, 4, packUInt32(count));
  return count != 0;
}

} // namespace ed2k

} // namespace aria2
