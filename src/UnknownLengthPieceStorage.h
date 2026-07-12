/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_UNKNOWN_LENGTH_PIECE_STORAGE_H
#define D_UNKNOWN_LENGTH_PIECE_STORAGE_H

#include "FatalException.h"
#include "PieceStorage.h"

namespace aria2 {

class Option;
class DownloadContext;
class DiskWriterFactory;
class DirectDiskAdaptor;
class BitfieldMan;

class UnknownLengthPieceStorage : public PieceStorage {
private:
  std::shared_ptr<DownloadContext> downloadContext_;

  std::shared_ptr<DirectDiskAdaptor> diskAdaptor_;

  std::shared_ptr<DiskWriterFactory> diskWriterFactory_;

  int64_t totalLength_;

  std::unique_ptr<BitfieldMan> bitfield_;

  bool downloadFinished_;

  std::shared_ptr<Piece> piece_;

  void createBitfield();

public:
  UnknownLengthPieceStorage(
      const std::shared_ptr<DownloadContext>& downloadContext);

  virtual ~UnknownLengthPieceStorage();

#ifdef ENABLE_BITTORRENT

  /**
   * Returns true if the peer has a piece that localhost doesn't have.
   * Otherwise returns false.
   */
  virtual bool
  hasMissingPiece(const std::shared_ptr<Peer>& peer) override;

  virtual void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                               size_t minMissingBlocks,
                               const std::shared_ptr<Peer>& peer,
                               cuid_t cuid) override;

  virtual void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                               size_t minMissingBlocks,
                               const std::shared_ptr<Peer>& peer,
                               const std::vector<size_t>& excludedIndexes,
                               cuid_t cuid) override;

  virtual void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                                   size_t minMissingBlocks,
                                   const std::shared_ptr<Peer>& peer,
                                   cuid_t cuid) override;

  virtual void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                                   size_t minMissingBlocks,
                                   const std::shared_ptr<Peer>& peer,
                                   const std::vector<size_t>& excludedIndexes,
                                   cuid_t cuid) override;

  virtual std::shared_ptr<Piece>
  getMissingPiece(const std::shared_ptr<Peer>& peer,
                  cuid_t cuid) override;

  virtual std::shared_ptr<Piece>
  getMissingPiece(const std::shared_ptr<Peer>& peer,
                  const std::vector<size_t>& excludedIndexes,
                  cuid_t cuid) override;
#endif // ENABLE_BITTORRENT

  virtual bool hasMissingUnusedPiece() override;

  /**
   * Returns a missing piece if available. Otherwise returns 0;
   */
  virtual std::shared_ptr<Piece>
  getMissingPiece(size_t minSplitSize, const unsigned char* ignoreBitfield,
                  size_t length, cuid_t cuid) override;

  /**
   * Returns a missing piece whose index is index.
   * If a piece whose index is index is already acquired or currently used,
   * then returns 0.
   * Also returns 0 if any of missing piece is not available.
   */
  virtual std::shared_ptr<Piece> getMissingPiece(size_t index,
                                                 cuid_t cuid) override;

  /**
   * Returns the piece denoted by index.
   * No status of the piece is changed in this method.
   */
  virtual std::shared_ptr<Piece> getPiece(size_t index) override;

  /**
   * Tells that the download of the specified piece completes.
   */
  virtual void
  completePiece(const std::shared_ptr<Piece>& piece) override;

  /**
   * Tells that the download of the specified piece is canceled.
   */
  virtual void cancelPiece(const std::shared_ptr<Piece>& piece,
                           cuid_t cuid) override;

  /**
   * Returns true if the specified piece is already downloaded.
   * Otherwise returns false.
   */
  virtual bool hasPiece(size_t index) override;

  virtual bool isPieceUsed(size_t index) override;

  virtual int64_t getTotalLength() override { return totalLength_; }

  virtual int64_t getFilteredTotalLength() override
  {
    return totalLength_;
  }

  virtual int64_t getCompletedLength() override;

  virtual int64_t getFilteredCompletedLength() override
  {
    return getCompletedLength();
  }

  virtual void setupFileFilter() override {}

  virtual void clearFileFilter() override {}

  /**
   * Returns true if download has completed.
   * If file filter is enabled, then returns true if those files have
   * downloaded.
   */
  virtual bool downloadFinished() override { return downloadFinished_; }

  /**
   * Returns true if all files have downloaded.
   * The file filter is ignored.
   */
  virtual bool allDownloadFinished() override
  {
    return downloadFinished();
  }

  /**
   * Initializes DiskAdaptor.
   * TODO add better documentation here.
   */
  virtual void initStorage() override;

  virtual const unsigned char* getBitfield() override;

  virtual void setBitfield(const unsigned char* bitfield,
                           size_t bitfieldLength) override
  {
  }

  virtual size_t getBitfieldLength() override;

  virtual bool isSelectiveDownloadingMode() override { return false; }

  virtual bool isEndGame() override { return false; }

  virtual void enterEndGame() override {}

  virtual void setEndGamePieceNum(size_t num) override {}

  virtual std::shared_ptr<DiskAdaptor> getDiskAdaptor() override;

  virtual WrDiskCache* getWrDiskCache() override { return nullptr; }

  virtual void flushWrDiskCacheEntry(bool releaseEntries) override {}

  virtual int32_t getPieceLength(size_t index) override;

  virtual void advertisePiece(cuid_t cuid, size_t index,
                              Timer registeredTime) override
  {
  }

  /**
   * indexes is filled with piece index which is not advertised by the
   * caller command and newer than lastHaveIndex.
   */
  virtual uint64_t
  getAdvertisedPieceIndexes(std::vector<size_t>& indexes, cuid_t myCuid,
                            uint64_t lastHaveIndex) override
  {
    throw FATAL_EXCEPTION("Not Implemented!");
  }

  virtual void removeAdvertisedPiece(const Timer& expiry) override {}

  /**
   * Sets all bits in bitfield to 1.
   */
  virtual void markAllPiecesDone() override;

  virtual void markPiecesDone(int64_t length) override;

  virtual void markPieceMissing(size_t index) override;

  /**
   * Do nothing because loading in-flight piece is not supported for this
   * class.
   */
  virtual void addInFlightPiece(
      const std::vector<std::shared_ptr<Piece>>& pieces) override
  {
  }

  virtual size_t countInFlightPiece() override { return 0; }

  virtual void
  getInFlightPieces(std::vector<std::shared_ptr<Piece>>& pieces) override;

  virtual void addPieceStats(size_t index) override {}

  virtual void addPieceStats(const unsigned char* bitfield,
                             size_t bitfieldLength) override
  {
  }

  virtual void subtractPieceStats(const unsigned char* bitfield,
                                  size_t bitfieldLength) override
  {
  }

  virtual void updatePieceStats(const unsigned char* newBitfield,
                                size_t newBitfieldLength,
                                const unsigned char* oldBitfield) override
  {
  }

  virtual size_t getNextUsedIndex(size_t index) override { return 0; }

  void setDiskWriterFactory(
      const std::shared_ptr<DiskWriterFactory>& diskWriterFactory);

  virtual void onDownloadIncomplete() override {}
};

} // namespace aria2

#endif // D_UNKNOWN_LENGTH_PIECE_STORAGE_H
