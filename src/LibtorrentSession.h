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
#include "GroupId.h"

#include <map>
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
  std::map<a2_gid_t, libtorrent::torrent_handle> handles_;

public:
  explicit LibtorrentSession(const Option* option);
  ~LibtorrentSession();

  LibtorrentSession(const LibtorrentSession&) = delete;
  LibtorrentSession& operator=(const LibtorrentSession&) = delete;

  libtorrent::torrent_handle
  addTorrent(a2_gid_t gid, libtorrent::add_torrent_params params);

  void removeTorrent(a2_gid_t gid);
  bool hasTorrent(a2_gid_t gid) const;
  void setTorrentDownloadLimit(a2_gid_t gid, int limit);
  void setTorrentUploadLimit(a2_gid_t gid, int limit);
  void setTorrentMaxConnections(a2_gid_t gid, int limit);
  void setTorrentFilePriorities(a2_gid_t gid,
                                const std::vector<int>& priorities);

  void pollAlerts(std::vector<libtorrent::alert*>& alerts);

  libtorrent::session& nativeSession() { return *session_; }
};

} // namespace aria2

#endif // D_LIBTORRENT_SESSION_H
