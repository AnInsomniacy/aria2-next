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
#include "ed2k_compression.h"

#include <cstring>
#include <limits>
#include <zlib.h>

#include "DlAbortEx.h"
#include "ed2k_hash.h"
#include "ed2k_packet.h"

namespace aria2 {

namespace ed2k {

namespace {

void validateHashLength(const std::string& hash)
{
  if (hash.size() != HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K hash length.");
  }
}

} // namespace

CompressedPartInflater::CompressedPartInflater()
    : stream_(),
      streamInitialized_(false),
      blockBegin_(0),
      inflatedLength_(0)
{
  std::memset(&stream_, 0, sizeof(stream_));
}

CompressedPartInflater::~CompressedPartInflater()
{
  reset();
}

void CompressedPartInflater::reset()
{
  if (streamInitialized_) {
    inflateEnd(&stream_);
    std::memset(&stream_, 0, sizeof(stream_));
    streamInitialized_ = false;
  }
  blockBegin_ = 0;
  inflatedLength_ = 0;
}

bool CompressedPartInflater::inflateChunk(std::string& data,
                                          const std::string& compressedData,
                                          int64_t blockBegin, size_t maxOutput)
{
  data.clear();
  if (compressedData.empty() || maxOutput == 0) {
    return false;
  }
  if (!streamInitialized_ || blockBegin_ != blockBegin) {
    reset();
    if (inflateInit(&stream_) != Z_OK) {
      return false;
    }
    streamInitialized_ = true;
    blockBegin_ = blockBegin;
    inflatedLength_ = 0;
  }

  data.assign(maxOutput, '\0');
  const auto oldTotalOut = stream_.total_out;
  stream_.avail_in = compressedData.size();
  stream_.next_in = reinterpret_cast<unsigned char*>(
      const_cast<char*>(compressedData.data()));
  stream_.avail_out = data.size();
  stream_.next_out = reinterpret_cast<unsigned char*>(&data[0]);

  const auto rc = inflate(&stream_, Z_SYNC_FLUSH);
  const auto produced = static_cast<size_t>(stream_.total_out - oldTotalOut);
  if ((rc != Z_OK && rc != Z_STREAM_END) || stream_.avail_in != 0 ||
      produced > maxOutput) {
    reset();
    data.clear();
    return false;
  }
  inflatedLength_ += static_cast<int64_t>(produced);
  data.resize(produced);
  if (rc == Z_STREAM_END) {
    reset();
  }
  return true;
}

bool CompressedPartInflater::active() const
{
  return streamInitialized_;
}

int64_t CompressedPartInflater::blockBegin() const
{
  return blockBegin_;
}

int64_t CompressedPartInflater::inflatedLength() const
{
  return inflatedLength_;
}

bool parseCompressedPartPayload(CompressedPartHeader& header,
                                std::string& compressedData,
                                const std::string& payload,
                                const std::string& expectedFileHash,
                                bool use64BitOffsets)
{
  validateHashLength(expectedFileHash);
  const size_t metaLength = use64BitOffsets ? 28 : 24;
  if (payload.size() < metaLength ||
      payload.substr(0, HASH_LENGTH) != expectedFileHash) {
    return false;
  }
  const uint64_t begin = use64BitOffsets ? readUInt64(payload.data() + 16)
                                         : readUInt32(payload.data() + 16);
  const auto totalCompressedLength =
      readUInt32(payload.data() + (use64BitOffsets ? 24 : 20));
  const auto chunkLength = payload.size() - metaLength;
  if (totalCompressedLength == 0 || chunkLength == 0 ||
      chunkLength > totalCompressedLength ||
      begin > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    return false;
  }
  header.begin = static_cast<int64_t>(begin);
  header.totalCompressedLength = totalCompressedLength;
  compressedData.assign(payload.begin() + metaLength, payload.end());
  return true;
}

bool inflatePackedPacketPayload(std::string& inflatedData,
                                const std::string& compressedData,
                                size_t maxInflatedLength)
{
  z_stream strm;
  std::memset(&strm, 0, sizeof(strm));
  if (inflateInit(&strm) != Z_OK) {
    return false;
  }

  inflatedData.assign(maxInflatedLength, '\0');
  strm.avail_in = compressedData.size();
  strm.next_in = reinterpret_cast<unsigned char*>(
      const_cast<char*>(compressedData.data()));
  strm.avail_out = inflatedData.size();
  strm.next_out = reinterpret_cast<unsigned char*>(&inflatedData[0]);

  const int rc = inflate(&strm, Z_FINISH);
  const bool ok = rc == Z_STREAM_END && strm.avail_in == 0;
  const size_t produced = inflatedData.size() - strm.avail_out;
  inflateEnd(&strm);

  if (!ok || produced == 0) {
    inflatedData.clear();
    return false;
  }
  inflatedData.resize(produced);
  return true;
}

} // namespace ed2k

} // namespace aria2
