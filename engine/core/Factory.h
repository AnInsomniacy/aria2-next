/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "options/Options.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace aria2 {

class Engine;
class Task;

using OptionPairs = std::vector<std::pair<std::string, std::string>>;

// A per-task option layer over the global options. Unknown names fail;
// options that cannot be set per task are ignored, as aria2 does.
std::shared_ptr<Options> taskOptions(Engine& engine, const OptionPairs& pairs);

// URIs of one resource (mirrors), a magnet link, an ED2K link, a local
// .torrent or .metalink path, or an HLS/DASH manifest.
std::vector<std::shared_ptr<Task>> createTasks(Engine& engine,
                                               std::vector<std::string> uris,
                                               std::shared_ptr<Options> options);
std::shared_ptr<Task> createTorrentTask(Engine& engine, std::string torrent,
                                        std::vector<std::string> webSeeds,
                                        std::shared_ptr<Options> options);
std::vector<std::shared_ptr<Task>>
createMetalinkTasks(Engine& engine, const std::string& metalink,
                    std::shared_ptr<Options> options);

// Starts downloaded .torrent files as tasks according to follow-torrent.
void installFollower(Engine& engine);

} // namespace aria2
