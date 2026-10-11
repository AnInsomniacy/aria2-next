/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>
#include <zlib.h>

namespace aria2::ed2k {

struct CompressedPartHeader {
  int64_t begin = 0;
  uint32_t totalCompressedLength = 0;
};

class CompressedPartInflater {
public:
  CompressedPartInflater();
  ~CompressedPartInflater();

  void reset();
  bool inflateChunk(std::string& data, const std::string& compressedData,
                    int64_t blockBegin, size_t maxOutput);
  bool active() const;
  int64_t blockBegin() const;
  int64_t inflatedLength() const;

private:
  z_stream stream_;
  bool streamInitialized_;
  int64_t blockBegin_;
  int64_t inflatedLength_;
};

bool parseCompressedPartPayload(CompressedPartHeader& header,
                                std::string& compressedData,
                                const std::string& payload,
                                const std::string& expectedFileHash,
                                bool use64BitOffsets);
bool inflateCompressedPartData(std::string& inflatedData,
                               const std::string& compressedData,
                               size_t maxInflatedLength);
// Deflates a block for OP_COMPRESSEDPART; false when it would not shrink.
bool deflateBlock(std::string& out, const std::string& data);
bool inflatePackedPacketPayload(std::string& inflatedData,
                                const std::string& compressedData,
                                size_t maxInflatedLength);

} // namespace aria2::ed2k
