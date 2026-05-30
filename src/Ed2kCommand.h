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
#include "ed2k_compression.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"

#include <array>
#include <memory>
#include <deque>
#include <vector>
#include <cstdint>

namespace aria2 {

class SocketCore;
class ARC4Encryptor;

namespace ed2k {
struct SharedFile;
class SharedResponder;
} // namespace ed2k

class Ed2kCommand : public AbstractCommand {
private:
  enum class Mode { SERVER, PEER };
  enum class State {
    INIT,
    RESOLVING,
    CONNECTING,
    OBFUSCATION_WRITE,
    OBFUSCATION_READ_MAGIC,
    OBFUSCATION_READ_METHOD,
    OBFUSCATION_READ_PADDING,
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
  std::deque<bool> outboxEncrypted_;
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
  bool incoming_;
  bool serverRequestSent_;
  bool closeAfterOutbox_;
  std::deque<uint32_t> pendingCallbackClientIds_;
  ed2k::EmulePeerInfo localPeerInfo_;
  ed2k::EmulePeerInfo remotePeerInfo_;
  std::unique_ptr<ARC4Encryptor> obfuscationEncryptor_;
  std::unique_ptr<ARC4Encryptor> obfuscationDecryptor_;
  std::string obfuscationWriteBuf_;
  size_t obfuscationWriteOffset_;
  std::array<char, 4> obfuscationMagicBuf_;
  size_t obfuscationMagicRead_;
  std::array<char, 2> obfuscationMethodBuf_;
  size_t obfuscationMethodRead_;
  std::string obfuscationPaddingBuf_;
  size_t obfuscationPaddingRead_;
  bool obfuscationEnabled_;
  struct CompressedPartState {
    ed2k::PartRange block;
    ed2k::CompressedPartInflater inflater;
  };
  std::vector<std::unique_ptr<CompressedPartState>> compressedPartStates_;

  bool isExpectedServerEof() const;
  bool shouldObfuscatePeerConnection() const;
  void initPeerObfuscation();
  bool flushObfuscationHandshake();
  bool readObfuscationMagic();
  bool readObfuscationMethod();
  bool readObfuscationPadding();
  void encryptPacket(std::string& data);
  void decryptData(char* data, size_t length);
  void resetCompressedPartInflaters();
  CompressedPartState* findCompressedPartState(int64_t begin);
  CompressedPartState* getOrCreateCompressedPartState(
      const ed2k::PartRange& block);
  void releaseCompletedCompressedPartState(const ed2k::PartRange& block);
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
  bool queueGetSources();
  void queueSearchRequest();
  void queueCallbackRequest(uint32_t clientId);
  uint32_t localEd2kClientId() const;
  ed2k::Endpoint localEd2kServerEndpoint() const;
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
  void queueCancelTransfer();
  bool sendPendingCancelTransfer();
  bool expireStalledTransfer();
  ed2k::SharedResponder createSharedResponder();
  bool updatePeerEndpointFromHello(bool helloPacket);
  void addPeer(const ed2k::Endpoint& peer);
  void addPeers(const std::vector<ed2k::Endpoint>& peers);
  void schedulePendingPeers();
  void handlePartData(int64_t begin, const std::string& data);

protected:
  virtual bool executeInternal() CXX11_OVERRIDE;

public:
  Ed2kCommand(cuid_t cuid, RequestGroup* requestGroup, DownloadEngine* e,
              ed2k::Endpoint endpoint, bool serverMode,
              bool countAsDownloadCommand = true);
  Ed2kCommand(cuid_t cuid, RequestGroup* requestGroup, DownloadEngine* e,
              ed2k::Endpoint endpoint,
              const std::shared_ptr<SocketCore>& socket);
  virtual ~Ed2kCommand();

  virtual bool execute() CXX11_OVERRIDE;
};

} // namespace aria2

#endif // D_ED2K_COMMAND_H
