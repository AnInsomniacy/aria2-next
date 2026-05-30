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
#ifndef D_ED2K_COMPRESSION_H
#define D_ED2K_COMPRESSION_H

#include "common.h"

#include <cstdint>
#include <string>
#include <zlib.h>

namespace aria2 {

namespace ed2k {

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
bool inflatePackedPacketPayload(std::string& inflatedData,
                                const std::string& compressedData,
                                size_t maxInflatedLength);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_COMPRESSION_H
