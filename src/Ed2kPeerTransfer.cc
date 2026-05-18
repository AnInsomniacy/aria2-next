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
#include "Ed2kPeerTransfer.h"

#include <algorithm>
#include <vector>

#include "DiskAdaptor.h"
#include "DlRetryEx.h"
#include "DownloadContext.h"
#include "Ed2kAttribute.h"
#include "Piece.h"
#include "PieceStorage.h"
#include "Segment.h"
#include "SegmentMan.h"
#include "ed2k_hash.h"

namespace aria2 {

namespace ed2k {

PeerTransfer::PeerTransfer(DownloadContext* dctx, PieceStorage* pieceStorage,
                           SegmentMan* segmentMan, cuid_t cuid)
    : dctx_(dctx),
      pieceStorage_(pieceStorage),
      segmentMan_(segmentMan),
      cuid_(cuid)
{
}

int64_t PeerTransfer::expectedPartLength(int64_t begin) const
{
  std::vector<std::shared_ptr<Segment>> segments;
  segmentMan_->getInFlightSegment(segments, cuid_);
  for (const auto& segment : segments) {
    if (segment->getPositionToWrite() == begin) {
      return std::min(begin + static_cast<int64_t>(BLOCK_LENGTH),
                      segment->getPosition() + segment->getLength()) -
             begin;
    }
  }
  return 0;
}

std::shared_ptr<Segment>
PeerTransfer::writePartData(int64_t begin, const std::string& data)
{
  if (begin < 0 || data.empty()) {
    throw DL_RETRY_EX("Bad ED2K part range.");
  }

  std::vector<std::shared_ptr<Segment>> segments;
  segmentMan_->getInFlightSegment(segments, cuid_);
  for (auto& segment : segments) {
    if (segment->getPositionToWrite() != begin) {
      continue;
    }
    pieceStorage_->getDiskAdaptor()->writeData(
        reinterpret_cast<const unsigned char*>(data.data()), data.size(),
        begin);
    dctx_->updateDownload(data.size());
    segment->updateWrittenLength(data.size());
    if (!segment->complete()) {
      return nullptr;
    }
    if (!verifyPiece(segment->getIndex())) {
      segment->clear(pieceStorage_->getWrDiskCache());
      segmentMan_->cancelSegment(cuid_, segment);
      throw DL_RETRY_EX("Bad ED2K piece hash.");
    }
    return segment;
  }

  throw DL_RETRY_EX("Unexpected ED2K part range.");
}

bool PeerTransfer::completeVerifiedSegment(size_t index)
{
  std::vector<std::shared_ptr<Segment>> segments;
  segmentMan_->getInFlightSegment(segments, cuid_);
  for (auto& segment : segments) {
    if (segment->getIndex() == index) {
      return segmentMan_->completeSegment(cuid_, segment);
    }
  }
  return false;
}

std::string PeerTransfer::readPiece(size_t index) const
{
  auto piece = pieceStorage_->getPiece(index);
  if (!piece) {
    return std::string();
  }
  std::string data(piece->getLength(), '\0');
  auto nread = pieceStorage_->getDiskAdaptor()->readData(
      reinterpret_cast<unsigned char*>(&data[0]), data.size(),
      static_cast<int64_t>(index) * dctx_->getPieceLength());
  if (nread != static_cast<ssize_t>(data.size())) {
    return std::string();
  }
  return data;
}

bool PeerTransfer::verifyPiece(size_t index) const
{
  auto attrs = getEd2kAttrs(dctx_);
  if (attrs->pieceHashes.empty() && dctx_->getNumPieces() == 1) {
    attrs->pieceHashes.push_back(attrs->link.hash);
  }
  if (index >= attrs->pieceHashes.size()) {
    return false;
  }
  auto data = readPiece(index);
  return !data.empty() && md4Digest(data) == attrs->pieceHashes[index];
}

} // namespace ed2k

} // namespace aria2
