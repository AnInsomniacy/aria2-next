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
#ifndef D_LIBTORRENT_SESSION_H
#define D_LIBTORRENT_SESSION_H

#include "common.h"

#include <memory>
#include <vector>

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>

namespace libtorrent {
struct alert;
struct add_torrent_params;
} // namespace libtorrent

namespace aria2 {

class Option;

class LibtorrentSession {
private:
  std::unique_ptr<libtorrent::session> session_;

public:
  explicit LibtorrentSession(const Option* option);
  ~LibtorrentSession();

  LibtorrentSession(const LibtorrentSession&) = delete;
  LibtorrentSession& operator=(const LibtorrentSession&) = delete;

  libtorrent::torrent_handle
  addTorrent(libtorrent::add_torrent_params params);

  void removeTorrent(const libtorrent::torrent_handle& handle);

  void pollAlerts(std::vector<libtorrent::alert*>& alerts);

  libtorrent::session& nativeSession() { return *session_; }
};

} // namespace aria2

#endif // D_LIBTORRENT_SESSION_H
