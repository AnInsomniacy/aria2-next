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
#ifndef D_ED2K_UPLOAD_QUEUE_H
#define D_ED2K_UPLOAD_QUEUE_H

#include "common.h"
#include "ed2k_link.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

class RequestGroupMan;
class Option;

namespace ed2k {

struct UploadPeer {
  Endpoint endpoint;
  std::string userHash;
  std::string fileHash;
  uint16_t rank = 0;
  bool uploading = false;
  int64_t waitStartTime = 0;
  int64_t uploadStartTime = 0;
  uint64_t sessionUploaded = 0;
};

struct PeerCreditState {
  std::string userHash;
  uint64_t uploaded = 0;
  uint64_t downloaded = 0;
};

class PeerCreditStore {
private:
  std::vector<PeerCreditState> credits_;

  PeerCreditState* getOrCreate(const std::string& userHash);

public:
  void addUploaded(const std::string& userHash, uint64_t bytes);
  void addDownloaded(const std::string& userHash, uint64_t bytes);
  double scoreRatio(const std::string& userHash);
  const std::vector<PeerCreditState>& list() const { return credits_; }
  size_t loadOptionState(const Option* option);
};

class UploadQueue {
private:
  std::vector<UploadPeer> peers_;
  PeerCreditStore credits_;
  size_t maxSlots_;

  UploadPeer* findPeer(const Endpoint& endpoint);
  const UploadPeer* findPeerByUserHash(const std::string& userHash) const;
  void sortWaiting();
  void recomputeRanks();
  bool canOpenSlot(RequestGroupMan* rgman) const;

public:
  explicit UploadQueue(size_t maxSlots = 3);

  bool requestUpload(const Endpoint& endpoint, const std::string& userHash,
                     const std::string& fileHash, int64_t now,
                     RequestGroupMan* rgman);
  bool isUploading(const Endpoint& endpoint) const;
  uint16_t queueRank(const Endpoint& endpoint) const;
  bool remove(const Endpoint& endpoint);
  void noteUploaded(const Endpoint& endpoint, uint64_t bytes);
  void noteDownloaded(const std::string& userHash, uint64_t bytes);
  size_t uploadingCount() const;
  size_t waitingCount() const;
  const std::vector<UploadPeer>& peers() const { return peers_; }
  PeerCreditStore& credits() { return credits_; }
  const PeerCreditStore& credits() const { return credits_; }
};

std::string createPeerCreditStatePayload(const PeerCreditState& state);
bool parsePeerCreditStatePayload(PeerCreditState& state,
                                 const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_UPLOAD_QUEUE_H
