/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "core/Factory.h"

#include "base/Error.h"
#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "bt/BtTask.h"
#include "bt/Metainfo.h"
#include "core/Engine.h"
#include "disk/File.h"
#include "ed2k/Ed2kTask.h"
#include "ed2k/Link.h"
#include "ed2k/Packet.h"
#include "http/HttpTask.h"
#include "http/Metalink.h"
#include "media/MediaTask.h"
#include "state/StateStore.h"

#include <libtorrent/hasher.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/torrent_info.hpp>

#include <algorithm>
#include <optional>

namespace aria2 {

namespace lt = libtorrent;

std::shared_ptr<Options> taskOptions(Engine& engine, const OptionPairs& pairs)
{
  auto options = std::make_shared<Options>(engine.sharedGlobal());
  for (const auto& [name, value] : pairs) {
    const auto* spec = findOption(name);
    if (spec && !(spec->scope & scope::Task)) {
      continue;
    }
    options->add(name, value);
  }
  return options;
}

namespace {

Gid assignGid(Engine& engine, Options& options)
{
  if (!options.blank("gid")) {
    const auto gid = parseGid(options.get("gid"));
    if (!gid) {
      fail(ErrorCode::Option, "Invalid GID {}", options.get("gid"));
    }
    if (engine.find(*gid)) {
      fail(ErrorCode::Option, "GID {} is already in use", options.get("gid"));
    }
    options.erase("gid");
    return *gid;
  }
  Gid gid;
  do {
    gid = newGid();
  } while (engine.find(gid));
  return gid;
}

bool isHttp(const std::string& scheme)
{
  return scheme == "http" || scheme == "https" || scheme == "sftp";
}

std::string torrentDirectory(Engine& engine)
{
  return engine.state().directory("bittorrent/torrents");
}

// Metainfo is kept under its SHA-1 so the session can name it.
std::string storeTorrent(Engine& engine, std::string_view data)
{
  const auto hash =
      lt::hasher(data.data(), static_cast<int>(data.size())).final();
  const auto path =
      joinPath(torrentDirectory(engine),
               text::toHex(std::string_view(hash.data(),
                                            static_cast<size_t>(hash.size()))) +
                   ".torrent");
  if (!fileExists(path) && !writeFileAtomically(path, data)) {
    fail(ErrorCode::FileCreate, "Cannot store torrent data in {}", path);
  }
  return path;
}

std::optional<lt::add_torrent_params> readResume(const std::string& path,
                                                 const lt::info_hash_t& hashes)
{
  const auto data = readFile(path);
  if (!data) {
    return std::nullopt;
  }
  lt::error_code error;
  auto resumed = lt::read_resume_data(
      {data->data(), static_cast<std::ptrdiff_t>(data->size())}, error);
  if (error || resumed.info_hashes != hashes) {
    log::warn("Ignoring resume data {}: {}", path,
              error ? error.message() : std::string("info hash mismatch"));
    return std::nullopt;
  }
  return resumed;
}

// Progress, file priorities and metadata saved when the task last stopped.
// Earlier releases named the file after the info hash; such a file is used
// only for the task restored into the same directory.
void overlayResume(Engine& engine, Gid gid, const Options& options,
                   lt::add_torrent_params& params)
{
  const auto directory = torrentDirectory(engine);
  auto resumed = readResume(joinPath(directory, gidText(gid) + ".fastresume"),
                            params.info_hashes);
  if (!resumed) {
    resumed = readResume(
        joinPath(directory, bt::hashText(params.info_hashes) + ".fastresume"),
        params.info_hashes);
    std::error_code error;
    if (resumed && !fs::equivalent(toPath(resumed->save_path),
                                   toPath(options.get("dir")), error)) {
      resumed.reset();
    }
  }
  if (!resumed) {
    return;
  }
  if (params.ti) {
    resumed->ti = params.ti;
  }
  for (auto& seed : params.url_seeds) {
    if (std::find(resumed->url_seeds.begin(), resumed->url_seeds.end(), seed) ==
        resumed->url_seeds.end()) {
      resumed->url_seeds.push_back(std::move(seed));
    }
  }
  params = std::move(*resumed);
}

void rejectDuplicate(Engine& engine, const lt::add_torrent_params& params)
{
  const auto hash = bt::hashText(params.info_hashes);
  const auto clash = [&hash](const std::shared_ptr<Task>& task) {
    const auto* other = dynamic_cast<const bt::BtTask*>(task.get());
    return other && other->infoHash() == hash;
  };
  const auto active = engine.active();
  if (std::any_of(active.begin(), active.end(), clash) ||
      std::any_of(engine.waiting().begin(), engine.waiting().end(), clash)) {
    fail(ErrorCode::DuplicateInfoHash,
         "Info hash {} is already being downloaded", hash);
  }
}

std::shared_ptr<Task> torrentTask(Engine& engine, lt::add_torrent_params params,
                                  std::string torrentPath,
                                  std::shared_ptr<Options> options)
{
  rejectDuplicate(engine, params);
  const bool restored = !options->blank("gid");
  const auto gid = assignGid(engine, *options);
  if (restored) {
    overlayResume(engine, gid, *options, params);
  }
  return std::make_shared<bt::BtTask>(engine, gid, std::move(options),
                                      std::move(params),
                                      std::move(torrentPath));
}

std::shared_ptr<Task> ed2kTask(Engine& engine, const std::string& uri,
                               std::shared_ptr<Options> options)
{
  ed2k::Link link;
  try {
    link = ed2k::parseLink(uri);
  }
  catch (const ed2k::ProtocolError& error) {
    fail(ErrorCode::Option, "Invalid ED2K link: {}", error.what());
  }
  if (link.type != ed2k::LinkType::FILE) {
    fail(ErrorCode::Option, "Only ED2K file links can be downloaded");
  }
  const auto clash = [&link](const std::shared_ptr<Task>& task) {
    const auto* other = dynamic_cast<const ed2k::Ed2kTask*>(task.get());
    return other && other->hash() == link.hash;
  };
  const auto active = engine.active();
  if (std::any_of(active.begin(), active.end(), clash) ||
      std::any_of(engine.waiting().begin(), engine.waiting().end(), clash)) {
    fail(ErrorCode::DuplicateDownload,
         "The ED2K file {} is already being downloaded",
         text::toHex(link.hash));
  }
  const auto gid = assignGid(engine, *options);
  return std::make_shared<ed2k::Ed2kTask>(engine, gid, std::move(options),
                                          std::move(link));
}

std::vector<std::shared_ptr<Task>>
localFileTasks(Engine& engine, const std::string& path,
               std::shared_ptr<Options> options)
{
  const auto content = readFile(path);
  if (!content) {
    fail(ErrorCode::FileOpen, "Cannot read {}", path);
  }
  if (bt::looksLikeTorrent(*content)) {
    return {createTorrentTask(engine, *content, {}, std::move(options))};
  }
  return createMetalinkTasks(engine, *content, std::move(options));
}

} // namespace

std::vector<std::shared_ptr<Task>> createTasks(Engine& engine,
                                               std::vector<std::string> uris,
                                               std::shared_ptr<Options> options)
{
  if (uris.empty()) {
    fail(ErrorCode::Option, "No URI to download");
  }
  std::vector<std::string> mirrors;
  for (auto& uri : uris) {
    if (uri::isThunder(uri)) {
      auto carried = uri::unwrapThunder(uri);
      if (!carried) {
        fail(ErrorCode::Option, "Invalid Thunder link: {}", uri::redact(uri));
      }
      uri = std::move(*carried);
    }
    if (ed2k::isLink(uri)) {
      return {ed2kTask(engine, uri, std::move(options))};
    }
    if (bt::isMagnet(uri)) {
      auto params = bt::parseMagnet(uri);
      return {torrentTask(engine, std::move(params), {}, std::move(options))};
    }
    // Local .torrent and .metalink files, as sessions also name them. A
    // Windows path would otherwise parse as a URL with a drive-letter scheme.
    if (fileExists(uri)) {
      return localFileTasks(engine, uri, std::move(options));
    }
    const auto parts = uri::parse(uri);
    const auto scheme = parts ? parts->scheme : std::string();
    if (isHttp(scheme)) {
      mirrors.push_back(std::move(uri));
      continue;
    }
    if (scheme == "file" && fileExists(text::percentDecode(parts->path))) {
      return localFileTasks(engine, text::percentDecode(parts->path),
                            std::move(options));
    }
    fail(ErrorCode::Option, "Unsupported URI: {}", uri::redact(uri));
  }
  const auto gid = assignGid(engine, *options);
  if (media::MediaTask::handles(mirrors.front(), *options)) {
    if (mirrors.size() > 1) {
      fail(ErrorCode::Option, "A media presentation has one source URI");
    }
    return {std::make_shared<media::MediaTask>(engine, gid, std::move(options),
                                               std::move(mirrors.front()))};
  }
  return {std::make_shared<http::HttpTask>(engine, gid, std::move(options),
                                           std::move(mirrors))};
}

std::shared_ptr<Task> createTorrentTask(Engine& engine, std::string torrent,
                                        std::vector<std::string> webSeeds,
                                        std::shared_ptr<Options> options)
{
  auto params = bt::parseTorrent(torrent);
  for (auto& seed : webSeeds) {
    params.url_seeds.push_back(std::move(seed));
  }
  auto path = storeTorrent(engine, torrent);
  return torrentTask(engine, std::move(params), std::move(path),
                     std::move(options));
}

void installFollower(Engine& engine)
{
  engine.setFollower([&engine](Task& finished) {
    const auto* download = dynamic_cast<const http::HttpTask*>(&finished);
    if (!download) {
      return;
    }
    const auto& path = download->path();
    const bool torrent =
        text::iequals(toPath(path).extension().string(), ".torrent") ||
        text::istartsWith(download->contentType(), "application/x-bittorrent");
    const bool metalink =
        !torrent && http::isMetalink(path, download->contentType());
    const auto mode =
        finished.options().get(torrent ? "follow-torrent" : "follow-metalink");
    if ((!torrent && !metalink) || mode == "false") {
      return;
    }
    try {
      const auto content = readFile(path);
      if (!content) {
        fail(ErrorCode::FileOpen, "Cannot read {}", path);
      }
      // Followers inherit the download's options except its own identity.
      auto options = std::make_shared<Options>(engine.sharedGlobal());
      for (const auto& [name, value] : finished.options().own()) {
        if (name != "out" && name != "gid" && name != "checksum") {
          options->set(name, value);
        }
      }
      auto tasks =
          torrent ? std::vector{createTorrentTask(engine, *content, {},
                                                  std::move(options))}
                  : createMetalinkTasks(engine, *content, std::move(options));
      for (const auto& task : tasks) {
        task->following = finished.gid();
        finished.followedBy.push_back(task->gid());
      }
      if (mode == "mem") {
        removeFile(path);
      }
      for (auto& task : tasks) {
        engine.add(std::move(task));
      }
    }
    catch (const Error& error) {
      log::error("Cannot follow {}: {}", path, error.what());
    }
  });
}

// Each file of a Metalink becomes an HTTP task over its mirrors, verified by
// the strongest whole-file hash. select-file chooses files by position.
std::vector<std::shared_ptr<Task>>
createMetalinkTasks(Engine& engine, const std::string& metalink,
                    std::shared_ptr<Options> options)
{
  const auto files = http::parseMetalink(metalink);
  std::vector<int> selection;
  if (!options->blank("select-file")) {
    selection = bt::parseSelection(options->get("select-file"),
                                   static_cast<int>(files.size()));
  }
  std::vector<std::shared_ptr<Task>> tasks;
  for (size_t i = 0; i < files.size(); ++i) {
    if (!selection.empty() &&
        std::find(selection.begin(), selection.end(),
                  static_cast<int>(i + 1)) == selection.end()) {
      continue;
    }
    const auto& file = files[i];
    auto own = std::make_shared<Options>(engine.sharedGlobal());
    for (const auto& [name, value] : options->own()) {
      if (name != "out" && name != "select-file" &&
          (name != "gid" || tasks.empty())) {
        own->set(name, value);
      }
    }
    own->set("out", file.name);
    if (!file.checksum.empty()) {
      own->set("checksum", file.checksum);
    }
    for (auto& task : createTasks(engine, file.urls, std::move(own))) {
      if (auto http = std::dynamic_pointer_cast<http::HttpTask>(task)) {
        http->setMetalink(file.size, file.pieces);
      }
      tasks.push_back(std::move(task));
    }
  }
  return tasks;
}

} // namespace aria2
