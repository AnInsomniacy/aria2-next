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
#include "Ed2kListenCommand.h"

#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Ed2kAttribute.h"
#include "Ed2kCommand.h"
#include "Ed2kSharedPeerCommand.h"
#include "Ed2kSharedStore.h"
#include "LogFactory.h"
#include "Logger.h"
#include "RecoverableException.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SocketCore.h"
#include "fmt.h"
#include "message.h"

namespace aria2 {

Ed2kListenCommand::Ed2kListenCommand(cuid_t cuid, DownloadEngine* e,
                                     int family)
    : Command(cuid), e_(e), family_(family)
{
}

Ed2kListenCommand::~Ed2kListenCommand()
{
  if (socket_) {
    const uint16_t port = socket_->isOpen() ? socket_->getAddrInfo().port : 0;
    e_->deleteSocketForReadCheck(socket_, this);
    if (port != 0 && e_->getEd2kTcpPort() == port) {
      e_->setEd2kTcpPort(0);
    }
  }
  e_->setEd2kTcpListenActive(false);
}

bool Ed2kListenCommand::bindPort(uint16_t port)
{
  if (socket_) {
    e_->deleteSocketForReadCheck(socket_, this);
  }
  socket_ = std::make_shared<SocketCore>();
  const int ipv = family_ == AF_INET ? 4 : 6;
  try {
    socket_->bind(nullptr, port, family_);
    socket_->beginListen();
    e_->addSocketForReadCheck(socket_, this);
    e_->setEd2kTcpPort(socket_->getAddrInfo().port);
    e_->setEd2kTcpListenActive(true);
    A2_LOG_NOTICE(fmt(_("IPv%d ED2K: listening on TCP port %u"), ipv,
                      socket_->getAddrInfo().port));
    return true;
  }
  catch (RecoverableException& ex) {
    A2_LOG_ERROR_EX(
        fmt("IPv%d ED2K: failed to bind TCP port %u", ipv, port), ex);
    socket_->closeConnection();
  }
  return false;
}

RequestGroup* Ed2kListenCommand::findEd2kRequestGroup() const
{
  const auto& groups = e_->getRequestGroupMan()->getRequestGroups();
  RequestGroup* match = nullptr;
  for (const auto& group : groups) {
    auto dctx = group->getDownloadContext();
    if (dctx && dctx->hasAttribute(CTX_ATTR_ED2K) &&
        !group->downloadFinished() && !group->isHaltRequested()) {
      if (match) {
        return nullptr;
      }
      match = group.get();
    }
  }
  return match;
}

bool Ed2kListenCommand::peerAlreadyActive(RequestGroup* group,
                                          const ed2k::Endpoint& peer) const
{
  auto attrs = getEd2kAttrs(group->getDownloadContext());
  if (!attrs) {
    return false;
  }
  for (const auto& state : attrs->peerStates) {
    if (state.endpoint.host == peer.host && state.endpoint.port == peer.port &&
        (state.connecting || state.accepted)) {
      return true;
    }
  }
  return false;
}

bool Ed2kListenCommand::execute()
{
  auto sharedStore = e_->getRequestGroupMan()->getEd2kSharedStore();
  const bool hasSharedFiles = sharedStore && sharedStore->size() != 0;
  if (e_->isHaltRequested() || (!findEd2kRequestGroup() && !hasSharedFiles)) {
    return true;
  }

  for (int i = 0; i < 3 && socket_->isReadable(0); ++i) {
    try {
      auto peerSocket = socket_->acceptConnection();
      peerSocket->applyIpDscp();
      auto group = findEd2kRequestGroup();
      auto endpoint = peerSocket->getPeerInfo();
      ed2k::Endpoint peer;
      peer.host = endpoint.addr;
      peer.port = endpoint.port;
      if (!group) {
        if (!hasSharedFiles) {
          peerSocket->closeConnection();
          continue;
        }
        e_->addCommand(make_unique<Ed2kSharedPeerCommand>(
            e_->newCUID(), e_, peer, peerSocket));
        A2_LOG_DEBUG(fmt("Accepted ED2K shared peer connection from %s:%u.",
                         peer.host.c_str(), peer.port));
        continue;
      }
      if (peerAlreadyActive(group, peer)) {
        peerSocket->closeConnection();
        continue;
      }
      e_->addCommand(make_unique<Ed2kCommand>(e_->newCUID(), group, e_, peer,
                                              peerSocket));
      A2_LOG_DEBUG(fmt("Accepted ED2K peer connection from %s:%u.",
                       peer.host.c_str(), peer.port));
    }
    catch (RecoverableException& ex) {
      A2_LOG_DEBUG_EX(fmt(MSG_ACCEPT_FAILURE, getCuid()), ex);
    }
  }

  e_->addCommand(std::unique_ptr<Command>(this));
  return false;
}

} // namespace aria2
