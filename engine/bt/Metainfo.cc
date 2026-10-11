/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "bt/Metainfo.h"

#include "base/Error.h"
#include "base/Path.h"
#include "base/Text.h"

#include <boost/json/array.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>

#include <algorithm>

namespace aria2::bt {

namespace json = boost::json;

namespace {

lt::load_torrent_limits limits()
{
  lt::load_torrent_limits result;
  result.max_buffer_size = 64 << 20;
  return result;
}

std::string hex(const lt::sha1_hash& hash)
{
  return hash.is_all_zeros() ? std::string() : text::toHex(std::string_view(hash.data(), static_cast<size_t>(hash.size())));
}

std::string hex(const lt::sha256_hash& hash)
{
  return hash.is_all_zeros() ? std::string() : text::toHex(std::string_view(hash.data(), static_cast<size_t>(hash.size())));
}

lt::download_priority_t priorityNamed(std::string_view name)
{
  if (name == "off") {
    return lt::dont_download;
  }
  if (name == "normal") {
    return lt::default_priority;
  }
  if (name == "high") {
    return lt::download_priority_t{6};
  }
  if (name == "top") {
    return lt::top_priority;
  }
  fail(ErrorCode::Option, "Invalid file priority {}", name);
}

} // namespace

bool isMagnet(std::string_view uri) { return text::istartsWith(uri, "magnet:"); }

bool looksLikeTorrent(std::string_view data)
{
  return data.size() > 2 && data.front() == 'd' &&
         data.find("4:info") != std::string_view::npos;
}

lt::add_torrent_params parseTorrent(std::string_view data)
{
  lt::error_code error;
  auto params = lt::load_torrent_buffer({data.data(), static_cast<std::ptrdiff_t>(data.size())},
                                        error, limits());
  if (error || !params.ti) {
    fail(ErrorCode::TorrentParse, "Invalid torrent file: {}", error.message());
  }
  return params;
}

lt::add_torrent_params parseMagnet(std::string_view uri)
{
  lt::error_code error;
  auto params = lt::parse_magnet_uri(uri, error);
  if (error) {
    fail(ErrorCode::MagnetParse, "Invalid magnet link: {}", error.message());
  }
  return params;
}

std::string hashText(const lt::info_hash_t& hashes)
{
  return hashes.has_v1() ? hex(hashes.v1) : hex(hashes.v2);
}

std::string magnetLink(const lt::add_torrent_params& params)
{
  // Only what identifies the content: no peers or file selection from
  // resume data.
  lt::add_torrent_params link;
  link.info_hashes = params.info_hashes;
  link.name = params.ti ? params.ti->name() : params.name;
  link.trackers = params.trackers;
  link.url_seeds = params.url_seeds;
  return lt::make_magnet_uri(link);
}

std::vector<int> parseSelection(std::string_view value, int fileCount)
{
  std::vector<int> result;
  for (const auto& part : text::split(value, ',')) {
    const auto item = text::trim(part);
    if (item.empty()) {
      continue;
    }
    const auto dash = item.find('-');
    const auto first = text::toInt(item.substr(0, dash));
    const auto last = dash == std::string_view::npos ? first
                                                     : text::toInt(item.substr(dash + 1));
    if (!first || !last || *first < 1 || *last < *first || *last > fileCount) {
      fail(ErrorCode::Option, "Invalid select-file entry {} for {} file(s)", item, fileCount);
    }
    for (auto index = *first; index <= *last; ++index) {
      result.push_back(static_cast<int>(index));
    }
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

std::vector<lt::download_priority_t> filePriorities(const Options& options,
                                                    int fileCount)
{
  const auto count = static_cast<size_t>(fileCount);
  std::vector<lt::download_priority_t> priorities(count, lt::default_priority);
  if (!options.blank("select-file")) {
    std::fill(priorities.begin(), priorities.end(), lt::dont_download);
    for (const auto index : parseSelection(options.get("select-file"), fileCount)) {
      priorities[static_cast<size_t>(index - 1)] = lt::default_priority;
    }
  }
  for (const auto& part : text::split(options.get("bt-file-priority"), ',')) {
    const auto item = text::trim(part);
    if (item.empty()) {
      continue;
    }
    const auto equals = item.find('=');
    const auto index = text::toInt(item.substr(0, equals));
    if (equals == std::string_view::npos || !index || *index < 1 ||
        *index > fileCount) {
      fail(ErrorCode::Option, "Invalid bt-file-priority entry {}", item);
    }
    priorities[static_cast<size_t>(*index - 1)] =
        priorityNamed(text::trim(item.substr(equals + 1)));
  }
  return priorities;
}

void applyTrackerOptions(lt::add_torrent_params& params, const Options& options)
{
  params.tracker_tiers.resize(params.trackers.size(), 0);
  const auto excluded = text::split(options.get("bt-exclude-tracker"), ',');
  const bool all = std::any_of(excluded.begin(), excluded.end(), [](const auto& uri) {
    return text::trim(uri) == "*";
  });
  for (size_t i = params.trackers.size(); i-- > 0;) {
    const bool drop = all || std::any_of(excluded.begin(), excluded.end(),
                                         [&](const auto& uri) {
                                           return text::trim(uri) == params.trackers[i];
                                         });
    if (drop) {
      params.trackers.erase(params.trackers.begin() + static_cast<std::ptrdiff_t>(i));
      params.tracker_tiers.erase(params.tracker_tiers.begin() +
                                 static_cast<std::ptrdiff_t>(i));
    }
  }
  int tier = params.tracker_tiers.empty()
                 ? 0
                 : *std::max_element(params.tracker_tiers.begin(), params.tracker_tiers.end()) + 1;
  for (const auto& part : text::split(options.get("bt-tracker"), ',')) {
    const std::string uri(text::trim(part));
    if (!uri.empty() &&
        std::find(params.trackers.begin(), params.trackers.end(), uri) == params.trackers.end()) {
      params.trackers.push_back(uri);
      params.tracker_tiers.push_back(tier++);
    }
  }
}

void applyIndexOut(lt::add_torrent_params& params, const Options& options)
{
  if (!params.ti) {
    return;
  }
  const auto fileCount = params.ti->layout().num_files();
  for (const auto& entry : text::split(options.get("index-out"), '\n')) {
    const auto equals = entry.find('=');
    const auto index = text::toInt(std::string_view(entry).substr(0, equals));
    if (equals == std::string::npos || !index || *index < 1 || *index > fileCount) {
      fail(ErrorCode::Option, "Invalid index-out entry {}", entry);
    }
    const auto path = entry.substr(equals + 1);
    if (!isContainedRelativePath(path)) {
      fail(ErrorCode::Option, "index-out path {} must stay inside the download directory",
           path);
    }
    params.renamed_files[lt::file_index_t{static_cast<int>(*index - 1)}] = path;
  }
}

json::object describe(const lt::add_torrent_params& params)
{
  json::object out;
  const auto& info = *params.ti;
  const auto& layout = info.layout();
  out["name"] = info.name();
  out["mode"] = layout.num_files() > 1 ? "multi" : "single";
  out["infoHashV1"] = hex(params.info_hashes.v1);
  out["infoHashV2"] = hex(params.info_hashes.v2);
  out["totalLength"] = std::to_string(info.total_size());
  json::array files;
  for (const auto index : layout.file_range()) {
    if (layout.pad_file_at(index)) {
      continue;
    }
    files.push_back(json::object{
        {"index", std::to_string(static_cast<int>(index) + 1)},
        {"path", toUtf8(toPath(layout.file_path(index)))},
        {"length", std::to_string(layout.file_size(index))}});
  }
  out["files"] = std::move(files);
  return out;
}

} // namespace aria2::bt
