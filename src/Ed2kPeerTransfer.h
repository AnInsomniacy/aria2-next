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
#ifndef D_ED2K_PEER_TRANSFER_H
#define D_ED2K_PEER_TRANSFER_H

#include "common.h"
#include "Command.h"

#include <memory>
#include <string>

namespace aria2 {

class DownloadContext;
class Piece;
class PieceStorage;
class SegmentMan;
class Segment;

namespace ed2k {

class PeerTransfer {
private:
  DownloadContext* dctx_;
  PieceStorage* pieceStorage_;
  SegmentMan* segmentMan_;
  cuid_t cuid_;

  std::string readPiece(size_t index) const;
  bool verifyPiece(size_t index) const;
  bool applyAichRecovery(const std::shared_ptr<Piece>& piece,
                         const std::string& data) const;

public:
  PeerTransfer(DownloadContext* dctx, PieceStorage* pieceStorage,
               SegmentMan* segmentMan, cuid_t cuid);

  int64_t expectedPartLength(int64_t begin) const;
  std::shared_ptr<Segment> writePartData(int64_t begin,
                                         const std::string& data);
  bool completeVerifiedSegment(size_t index);
};

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_PEER_TRANSFER_H
