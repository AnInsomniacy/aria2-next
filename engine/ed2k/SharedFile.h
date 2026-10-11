/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aria2::ed2k {

struct PartRange;

// A complete or partial file offered to other clients. readRange() reads the
// file synchronously, so payload builders that read data run on a disk pool
// thread.
class SharedSource {
public:
  virtual ~SharedSource() = default;
  virtual const std::string& hash() const = 0;
  virtual const std::string& aichRootHash() const = 0;
  virtual const std::vector<std::string>& pieceHashes() const = 0;
  virtual const std::string& name() const = 0;
  virtual int64_t size() const = 0;
  virtual bool complete() const = 0;
  virtual std::vector<bool> bitfield() const = 0;
  virtual bool readRange(std::string& data, int64_t begin,
                         int64_t end) const = 0;
};

std::string createSharedFileNameAnswerPayload(const SharedSource& source);
std::string createSharedFileStatusPayload(const SharedSource& source);
bool createSharedFileHashSetPayload(std::string& payload,
                                    const SharedSource& source);
bool createSharedFilePartPayload(std::string& payload,
                                 const SharedSource& source,
                                 const PartRange& range, bool use64BitOffsets);
bool createSharedFileAichAnswerPayload(std::string& payload,
                                       const SharedSource& source,
                                       uint16_t partIndex,
                                       const std::string& rootHash);
bool parsePartRequestPayload(std::vector<PartRange>& ranges,
                             std::string& fileHash, const std::string& payload,
                             bool use64BitOffsets);

} // namespace aria2::ed2k
