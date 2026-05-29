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
#ifndef D_LIBTORRENT_COMMAND_H
#define D_LIBTORRENT_COMMAND_H

#include "TimeBasedCommand.h"
#include "TimerA2.h"
#include "error_code.h"
#include "LibtorrentStatCounter.h"

#include <libtorrent/torrent_handle.hpp>

namespace aria2 {

class DownloadEngine;
class RequestGroup;
class LibtorrentSession;

class LibtorrentCommand : public TimeBasedCommand {
private:
  RequestGroup* requestGroup_;
  LibtorrentSession* session_;
  libtorrent::torrent_handle handle_;
  LibtorrentStatCounter statCounter_;
  Timer sharingTimer_;
  bool resumeDataRequested_;
  bool torrentAdded_;
  bool btCompleteNotified_;
  bool sharingTimerStarted_;
  bool resumeDataSynced_;

  void addTorrent();
  void pollAlerts();
  void updateStatus();
  void requestResumeData();
  void syncResumeData();
  void storeResumeData(const libtorrent::add_torrent_params& params);
  void storeResumeStatus(const libtorrent::add_torrent_params& params);
  void reportBtDownloadComplete();
  void finishDownload();
  void failDownload(error_code::Value code, const std::string& message);

public:
  LibtorrentCommand(cuid_t cuid, RequestGroup* requestGroup,
                    DownloadEngine* engine);
  ~LibtorrentCommand();

  void preProcess() CXX11_OVERRIDE;
  void process() CXX11_OVERRIDE;
};

} // namespace aria2

#endif // D_LIBTORRENT_COMMAND_H
