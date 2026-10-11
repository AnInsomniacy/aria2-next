/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "options/Options.h"

#include <boost/json/object.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/download_priority.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aria2::bt {

namespace lt = libtorrent;

bool isMagnet(std::string_view uri);
// A bencoded dictionary, the shape of every .torrent file.
bool looksLikeTorrent(std::string_view data);

// Parse untrusted metainfo within fixed size limits. Errors are
// Error(TorrentParse) and Error(MagnetParse).
lt::add_torrent_params parseTorrent(std::string_view data);
lt::add_torrent_params parseMagnet(std::string_view uri);

// The v1 info hash in hex, or the v2 hash for v2-only torrents.
std::string hashText(const lt::info_hash_t& hashes);
std::string magnetLink(const lt::add_torrent_params& params);

// "1-5,8" as 1-based file indexes. Error(Option) for malformed text or
// indexes outside 1..fileCount.
std::vector<int> parseSelection(std::string_view text, int fileCount);
// select-file chooses files; bt-file-priority (INDEX=off|normal|high|top)
// refines them.
std::vector<lt::download_priority_t> filePriorities(const Options& options,
                                                    int fileCount);
// bt-exclude-tracker removes, then bt-tracker appends one tier per URI.
void applyTrackerOptions(lt::add_torrent_params& params, const Options& options);
// index-out INDEX=PATH entries.
void applyIndexOut(lt::add_torrent_params& params, const Options& options);

// The inspectTorrent result: name, mode, hashes, length and files.
boost::json::object describe(const lt::add_torrent_params& params);

} // namespace aria2::bt
