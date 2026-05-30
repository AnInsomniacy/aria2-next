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
#ifndef D_ED2K_SHARED_RESPONDER_H
#define D_ED2K_SHARED_RESPONDER_H

#include "common.h"
#include "ed2k_link.h"

#include <deque>
#include <string>

namespace aria2 {

class RequestGroupMan;

namespace ed2k {

class SharedStore;
class UploadQueue;
struct SharedFile;

class SharedResponder {
private:
  SharedStore* store_;
  UploadQueue* uploadQueue_;
  std::deque<std::string>* outbox_;
  Endpoint endpoint_;
  std::string userHash_;
  RequestGroupMan* rgman_;

  const SharedFile* findFile(const std::string& hash) const;
  void queuePacket(uint8_t protocol, uint8_t opcode,
                   const std::string& payload);

public:
  SharedResponder(SharedStore* store, UploadQueue* uploadQueue,
                  RequestGroupMan* rgman, const Endpoint& endpoint,
                  const std::string& userHash,
                  std::deque<std::string>& outbox);

  bool hasFile(const std::string& hash) const;
  void queueNoFile(const std::string& fileHash);
  bool queueFileNameAnswer(const std::string& fileHash);
  bool queueFileStatusAnswer(const std::string& fileHash);
  bool queueHashSetAnswer(const std::string& fileHash);
  bool queueSourceExchangeAnswer(const std::string& fileHash, uint8_t version);
  bool queueAichFileHashAnswer(const std::string& fileHash);
  bool queueAichAnswer(const std::string& fileHash,
                       const std::string& requestPayload);
  bool requestUploadSlot(const std::string& fileHash, int64_t now);
  bool queuePartAnswers(const std::string& requestPayload,
                        bool use64BitOffsets);
};

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_SHARED_RESPONDER_H
