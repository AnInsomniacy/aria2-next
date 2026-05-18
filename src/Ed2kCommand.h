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
#ifndef D_ED2K_COMMAND_H
#define D_ED2K_COMMAND_H

#include "AbstractCommand.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"

#include <array>
#include <deque>
#include <vector>

namespace aria2 {

class Ed2kCommand : public AbstractCommand {
private:
  enum class Mode { SERVER, PEER };
  enum class State {
    INIT,
    RESOLVING,
    CONNECTING,
    WRITE,
    READ_HEADER,
    READ_BODY,
    DONE
  };

  Mode mode_;
  ed2k::Endpoint endpoint_;
  State state_;
  std::vector<std::string> resolvedAddresses_;
  std::string connectedHostname_;
  std::string connectedAddr_;
  uint16_t connectedPort_;
  std::deque<std::string> outbox_;
  std::array<char, 6> headerBuf_;
  size_t headerRead_;
  ed2k::PacketHeader currentHeader_;
  std::string body_;
  size_t bodyRead_;
  bool peerFileStatusReceived_;
  bool peerFileRequestSent_;
  bool peerAccepted_;
  bool sourceExchangeRequested_;
  bool aichFileHashRequested_;
  bool use64BitOffsets_;
  ed2k::EmulePeerInfo localPeerInfo_;
  ed2k::EmulePeerInfo remotePeerInfo_;

  void startResolve();
  void startConnect();
  bool flushOutbox();
  bool readHeader();
  bool readBody();
  void handlePacket();
  void handleServerPacket();
  void handlePeerPacket();
  void queuePacket(uint8_t protocol, uint8_t opcode, const std::string& payload);
  void queueServerLogin();
  void queueGetSources();
  void queueSearchRequest();
  void queueCallbackRequest(uint32_t clientId);
  void queuePeerHello();
  void queuePeerHelloAnswer();
  void queueEmuleInfo(bool answer);
  void queuePeerFileRequest();
  void queuePeerFileStatusRequest();
  void queuePeerHashSetRequest();
  void queueAichFileHashRequest();
  void queueAichRecoveryRequest(size_t pieceIndex);
  void queueSourceExchangeRequest();
  void queueSourceExchangeAnswer(uint8_t version);
  void queuePeerStartUpload();
  void queuePeerPartRequest();
  void addPeer(const ed2k::Endpoint& peer);
  void addPeers(const std::vector<ed2k::Endpoint>& peers);
  void schedulePendingPeers();
  void handlePartData(int64_t begin, const std::string& data);
  int64_t expectedPartLength(int64_t begin);
  bool verifyPiece(size_t index);
  void completeVerifiedSegment(size_t index);
  std::string readPiece(size_t index);

protected:
  virtual bool executeInternal() CXX11_OVERRIDE;

public:
  Ed2kCommand(cuid_t cuid, RequestGroup* requestGroup, DownloadEngine* e,
              ed2k::Endpoint endpoint, bool serverMode,
              bool countAsDownloadCommand = true);
  virtual ~Ed2kCommand();

  virtual bool execute() CXX11_OVERRIDE;
};

} // namespace aria2

#endif // D_ED2K_COMMAND_H
