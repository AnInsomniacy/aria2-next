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
  const auto compressedLength =
      readUInt32(payload.data() + (use64BitOffsets ? 24 : 20));
  if (compressedLength != payload.size() - metaLength ||
      begin > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    return false;
  }
  header.begin = static_cast<int64_t>(begin);
  header.compressedLength = compressedLength;
  compressedData.assign(payload.begin() + metaLength, payload.end());
  return true;
}

bool inflateCompressedPartData(std::string& inflatedData,
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

  if (!ok || produced != maxInflatedLength) {
    inflatedData.clear();
    return false;
  }
  inflatedData.resize(produced);
  return true;
}

} // namespace ed2k

} // namespace aria2
