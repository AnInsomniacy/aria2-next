/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "core/Factory.h"

#include "base/Error.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "core/Engine.h"
#include "http/HttpTask.h"

namespace aria2 {

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
    const auto parts = uri::parse(uri);
    const auto scheme = parts ? parts->scheme : std::string();
    if (isHttp(scheme)) {
      mirrors.push_back(std::move(uri));
      continue;
    }
    fail(ErrorCode::Option, "Unsupported URI: {}", uri::redact(uri));
  }
  const auto gid = assignGid(engine, *options);
  options->erase("gid");
  return {std::make_shared<http::HttpTask>(engine, gid, std::move(options),
                                           std::move(mirrors))};
}

std::shared_ptr<Task> createTorrentTask(Engine&, std::string,
                                        std::vector<std::string>,
                                        std::shared_ptr<Options>)
{
  fail(ErrorCode::TorrentParse, "BitTorrent is not available");
}

std::vector<std::shared_ptr<Task>> createMetalinkTasks(Engine&, const std::string&,
                                                       std::shared_ptr<Options>)
{
  fail(ErrorCode::MetalinkParse, "Metalink is not available");
}

} // namespace aria2
