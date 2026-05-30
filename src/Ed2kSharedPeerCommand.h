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
#ifndef D_ED2K_SHARED_PEER_COMMAND_H
#define D_ED2K_SHARED_PEER_COMMAND_H

#include "Command.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"

#include <array>
#include <deque>
#include <memory>

namespace aria2 {

class DownloadEngine;
class SocketCore;

namespace ed2k {
class SharedResponder;
} // namespace ed2k

class Ed2kSharedPeerCommand : public Command {
private:
  enum class State { READ_HEADER, READ_BODY, WRITE, DONE };

  DownloadEngine* e_;
  ed2k::Endpoint endpoint_;
  std::shared_ptr<SocketCore> socket_;
  State state_;
  std::deque<std::string> outbox_;
  std::array<char, 6> headerBuf_;
  size_t headerRead_;
  ed2k::PacketHeader currentHeader_;
  std::string body_;
  size_t bodyRead_;
  ed2k::EmulePeerInfo localPeerInfo_;
  ed2k::EmulePeerInfo remotePeerInfo_;
  bool writeCheck_;

  bool flushOutbox();
  bool readHeader();
  bool readBody();
  void handlePacket();
  void handleEdonkeyPacket();
  void handleEmulePacket();
  void queuePacket(uint8_t protocol, uint8_t opcode,
                   const std::string& payload);
  void queuePeerHelloAnswer();
  void queueEmuleInfo(bool answer);
  ed2k::SharedResponder createSharedResponder();
  void addCommandSelf();
  void setReadCheck();
  void setWriteCheck(bool enabled);

public:
  Ed2kSharedPeerCommand(cuid_t cuid, DownloadEngine* e,
                        const ed2k::Endpoint& endpoint,
                        const std::shared_ptr<SocketCore>& socket);
  virtual ~Ed2kSharedPeerCommand();

  virtual bool execute() CXX11_OVERRIDE;
};

} // namespace aria2

#endif // D_ED2K_SHARED_PEER_COMMAND_H
