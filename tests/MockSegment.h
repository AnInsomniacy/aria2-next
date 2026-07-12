#ifndef D_MOCK_SEGMENT_H
#define D_MOCK_SEGMENT_H

#include "Segment.h"
#include "Piece.h"

namespace aria2 {

class MockSegment : public Segment {
public:
  virtual bool complete() const override { return false; }

  virtual size_t getIndex() const override { return 0; }

  virtual int64_t getPosition() const override { return 0; }

  virtual int64_t getPositionToWrite() const override { return 0; }

  virtual int64_t getLength() const override { return 0; }

  virtual int64_t getSegmentLength() const override { return 0; }

  virtual int64_t getWrittenLength() const override { return 0; }

  virtual void updateWrittenLength(int64_t bytes) override {}

  // `begin' is a offset inside this segment.
  virtual bool updateHash(int64_t begin, const unsigned char* data,
                          size_t dataLength) override
  {
    return false;
  }

  virtual bool isHashCalculated() const override { return false; }

  virtual std::string getDigest() override { return ""; }

  virtual void clear(WrDiskCache* diskCache) override {}

  virtual std::shared_ptr<Piece> getPiece() const override
  {
    return std::shared_ptr<Piece>(new Piece());
  }
};

} // namespace aria2

#endif // D_MOCK_SEGMENT_H
