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
#ifndef D_ED2K_LISTEN_COMMAND_H
#define D_ED2K_LISTEN_COMMAND_H

#include "Command.h"
#include "ed2k_link.h"

#include <memory>

namespace aria2 {

class DownloadEngine;
class RequestGroup;
class SocketCore;

class Ed2kListenCommand : public Command {
private:
  DownloadEngine* e_;
  int family_;
  std::shared_ptr<SocketCore> socket_;

  RequestGroup* findEd2kRequestGroup() const;
  bool peerAlreadyActive(RequestGroup* group, const ed2k::Endpoint& peer) const;

public:
  Ed2kListenCommand(cuid_t cuid, DownloadEngine* e, int family);
  virtual ~Ed2kListenCommand();

  bool bindPort(uint16_t port);
  virtual bool execute() override;
};

} // namespace aria2

#endif // D_ED2K_LISTEN_COMMAND_H
