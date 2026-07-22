#ifndef D_MOCK_PIECE_STORAGE_H
#define D_MOCK_PIECE_STORAGE_H

#include "PieceStorage.h"

#include <algorithm>
#include <deque>

#include "BitfieldMan.h"
#include "FatalException.h"
#include "Piece.h"
#include "DiskAdaptor.h"

namespace aria2 {

class MockPieceStorage : public PieceStorage {
private:
  int64_t totalLength;
  int64_t filteredTotalLength;
  int64_t completedLength;
  int64_t filteredCompletedLength;
  BitfieldMan* bitfieldMan;
  bool selectiveDownloadingMode;
  bool endGame;
  std::shared_ptr<DiskAdaptor> diskAdaptor;
  std::deque<int32_t> pieceLengthList;
  std::deque<std::shared_ptr<Piece>> inFlightPieces;
  bool downloadFinished_;
  bool allDownloadFinished_;

public:
  MockPieceStorage()
      : totalLength(0),
        filteredTotalLength(0),
        completedLength(0),
        filteredCompletedLength(0),
        bitfieldMan(0),
        selectiveDownloadingMode(false),
        endGame(false),
        downloadFinished_(false),
        allDownloadFinished_(false)
  {
  }

  virtual ~MockPieceStorage() {}

#ifdef ENABLE_BITTORRENT

  virtual bool hasMissingPiece(const std::shared_ptr<Peer>& peer) override
  {
    return false;
  }

  virtual void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                               size_t minMissingBlocks,
                               const std::shared_ptr<Peer>& peer,
                               cuid_t cuid) override
  {
  }

  virtual void getMissingPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                               size_t minMissingBlocks,
                               const std::shared_ptr<Peer>& peer,
                               const std::vector<size_t>& excludedIndexes,
                               cuid_t cuid) override
  {
  }

  virtual void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                                   size_t minMissingBlocks,
                                   const std::shared_ptr<Peer>& peer,
                                   cuid_t cuid) override
  {
  }

  virtual void getMissingFastPiece(std::vector<std::shared_ptr<Piece>>& pieces,
                                   size_t minMissingBlocks,
                                   const std::shared_ptr<Peer>& peer,
                                   const std::vector<size_t>& excludedIndexes,
                                   cuid_t cuid) override
  {
  }

  virtual std::shared_ptr<Piece>
  getMissingPiece(const std::shared_ptr<Peer>& peer, cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  virtual std::shared_ptr<Piece>
  getMissingPiece(const std::shared_ptr<Peer>& peer,
                  const std::vector<size_t>& excludedIndexes,
                  cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

#endif // ENABLE_BITTORRENT

  virtual bool hasMissingUnusedPiece() override { return false; }

  virtual std::shared_ptr<Piece>
  getMissingPiece(size_t minSplitSize, const unsigned char* ignoreBitfield,
                  size_t length, cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  virtual std::shared_ptr<Piece> getMissingPiece(size_t index,
                                                 cuid_t cuid) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  virtual bool isPieceUsed(size_t index) override { return false; }

  virtual void markPieceMissing(size_t index) override {}

  virtual void markPiecesDone(int64_t) override {}

  virtual std::shared_ptr<Piece> getPiece(size_t index) override
  {
    return std::shared_ptr<Piece>(new Piece());
  }

  virtual void completePiece(const std::shared_ptr<Piece>& piece) override
  {
  }

  virtual void cancelPiece(const std::shared_ptr<Piece>& piece,
                           cuid_t cuid) override
  {
  }

  virtual bool hasPiece(size_t index) override { return false; }

  virtual int64_t getTotalLength() override { return totalLength; }

  void setTotalLength(int64_t totalLength) { this->totalLength = totalLength; }

  virtual int64_t getFilteredTotalLength() override
  {
    return filteredTotalLength;
  }

  void setFilteredTotalLength(int64_t totalLength)
  {
    this->filteredTotalLength = totalLength;
  }

  virtual int64_t getCompletedLength() override
  {
    return completedLength;
  }

  void setCompletedLength(int64_t completedLength)
  {
    this->completedLength = completedLength;
  }

  virtual int64_t getFilteredCompletedLength() override
  {
    return filteredCompletedLength;
  }

  void setFilteredCompletedLength(int64_t completedLength)
  {
    this->filteredCompletedLength = completedLength;
  }

  virtual void setupFileFilter() override {}

  virtual void clearFileFilter() override {}

  virtual bool downloadFinished() override { return downloadFinished_; }

  void setDownloadFinished(bool f) { downloadFinished_ = f; }

  virtual bool allDownloadFinished() override
  {
    return allDownloadFinished_;
  }

  void setAllDownloadFinished(bool f) { allDownloadFinished_ = f; }

  virtual void initStorage() override {}

  virtual const unsigned char* getBitfield() override
  {
    return bitfieldMan->getBitfield();
  }

  virtual void setBitfield(const unsigned char* bitfield,
                           size_t bitfieldLength) override
  {
    bitfieldMan->setBitfield(bitfield, bitfieldLength);
  }

  virtual size_t getBitfieldLength() override
  {
    return bitfieldMan->getBitfieldLength();
  }

  void setBitfield(BitfieldMan* bitfieldMan)
  {
    this->bitfieldMan = bitfieldMan;
  }

  virtual bool isSelectiveDownloadingMode() override
  {
    return selectiveDownloadingMode;
  }

  void setSelectiveDownloadingMode(bool flag)
  {
    this->selectiveDownloadingMode = flag;
  }

  virtual bool isEndGame() override { return endGame; }

  virtual void setEndGamePieceNum(size_t num) override {}

  virtual void enterEndGame() override { this->endGame = true; }

  virtual std::shared_ptr<DiskAdaptor> getDiskAdaptor() override
  {
    return diskAdaptor;
  }

  virtual WrDiskCache* getWrDiskCache() override { return 0; }

  virtual void flushWrDiskCacheEntry(bool releaseEntries) override {}

  void setDiskAdaptor(const std::shared_ptr<DiskAdaptor>& adaptor)
  {
    this->diskAdaptor = adaptor;
  }

  virtual int32_t getPieceLength(size_t index) override
  {
    return pieceLengthList.at(index);
  }

  void addPieceLengthList(int32_t length) { pieceLengthList.push_back(length); }

  virtual void advertisePiece(cuid_t cuid, size_t index,
                              Timer registeredTime) override
  {
  }

  virtual uint64_t
  getAdvertisedPieceIndexes(std::vector<size_t>& indexes, cuid_t myCuid,
                            uint64_t lastHaveIndex) override
  {
    throw FATAL_EXCEPTION("Not Implemented!");
  }

  virtual void removeAdvertisedPiece(const Timer& expiry) override {}

  virtual void markAllPiecesDone() override {}

  virtual void addInFlightPiece(
      const std::vector<std::shared_ptr<Piece>>& pieces) override
  {
    std::copy(pieces.begin(), pieces.end(), std::back_inserter(inFlightPieces));
  }

  virtual size_t countInFlightPiece() override
  {
    return inFlightPieces.size();
  }

  virtual void
  getInFlightPieces(std::vector<std::shared_ptr<Piece>>& pieces) override
  {
    pieces.insert(pieces.end(), inFlightPieces.begin(), inFlightPieces.end());
  }

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

  virtual void onDownloadIncomplete() override {}
};

} // namespace aria2

#endif // D_MOCK_PIECE_STORAGE_H
