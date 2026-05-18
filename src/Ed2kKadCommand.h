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
#ifndef D_ED2K_KAD_COMMAND_H
#define D_ED2K_KAD_COMMAND_H

#include "Command.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "Ed2kKadState.h"
#include "ed2k_link.h"

namespace aria2 {

class DownloadEngine;
class RequestGroup;
class SocketCore;

class Ed2kKadCommand : public Command {
public:
  Ed2kKadCommand(cuid_t cuid, RequestGroup* requestGroup, DownloadEngine* e);
  virtual ~Ed2kKadCommand();

  virtual bool execute() CXX11_OVERRIDE;
  uint16_t getLocalUdpPort() const;
  bool waitLocalUdpReadable(time_t timeout) const;

private:
  RequestGroup* requestGroup_;
  DownloadEngine* e_;
  std::shared_ptr<SocketCore> socket_;
  std::deque<std::pair<ed2k::Endpoint, std::string>> outbox_;
  bool initialized_;
  bool sourceSearchSent_;
  bool keywordSearchSent_;
  int64_t lastServerStatusPoll_;

  void init();
  void queueBootstrap();
  void queueRefresh();
  void queueFirewalledCheck();
  void queueServerStatusPoll();
  void queueSourceSearch();
  void queueKeywordSearch();
  void queueTraversalActions(
      ed2k::KadTraversal& traversal,
      const std::vector<ed2k::KadTraversalAction>& actions);
  void queuePacket(const ed2k::Endpoint& endpoint, uint8_t opcode,
                   const std::string& payload);
  void queueEd2kUdpPacket(const ed2k::Endpoint& endpoint, uint8_t opcode,
                          const std::string& payload);
  void queueEmuleUdpPacket(const ed2k::Endpoint& endpoint, uint8_t opcode,
                           const std::string& payload);
  void sendQueuedPackets();
  void receivePackets();
  void handlePacket(const ed2k::Endpoint& endpoint, uint8_t opcode,
                    const std::string& payload);
  void handleEd2kUdpPacket(const ed2k::Endpoint& endpoint, uint8_t opcode,
                           const std::string& payload);
  int64_t nowSeconds() const;
};

} // namespace aria2

#endif // D_ED2K_KAD_COMMAND_H
