/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"
#include "media/MediaJob.h"
#include "media/Control.h"
#include <chrono>
#include <gpac/dash.h>
#include <gpac/setup.h>
#include <ios>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include "media/MediaFiles.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace aria2::media {

MediaJob::MediaJob(std::shared_ptr<const Options> op, std::string source,
                   std::string id, std::string root,
                   std::shared_ptr<Control> ctl)
    : option(std::move(op)),
      control(std::move(ctl)),
      uri(std::move(source)),
      gid(std::move(id)),
      directory(std::move(root)),
      taskDirectory(utf8(nativePath(directory) / "tasks" / gid)),
      store(directory, gid),
      transport(option.get(), uri, taskDirectory, control),
      io(*this)
{
  live = control->snapshot.live;
  auto identity = Transport::fingerprint(
      uri + "\n" + option->get("media-video") + "\n" +
      option->get("media-audio") + "\n" + option->get("media-subtitles") +
      "\n" + option->get("media-format") + "\n" +
      option->get("media-request-contexts") + "\n" +
      option->get("media-input") + "\n" + option->get("media-start-time") +
      "\n" + option->get("media-end-time") + "\n" + option->get("header") +
      "\n" + option->get("http-user") + "\n" + option->get("http-passwd"));
  if (store.identity(identity)) {
    std::lock_guard<std::mutex> lock(control->mutex);
    control->snapshot.downloadedLength = 0;
    control->snapshot.completedDuration = 0;
  }
  for (const auto& segment : store.segments()) {
    remember(segment);
    transport.retain(segment.path);
    transport.retain(segment.init);
  }
  std::filesystem::permissions(nativePath(taskDirectory),
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace);
  dash = gf_dash_new(
      io.get(), 1000, 0, GF_TRUE, GF_TRUE, GF_DASH_SELECT_BANDWIDTH_HIGHEST,
      live && snapshot().protocol == "hls" && !retained.empty() ? 100 : 0);
  if (!dash)
    throw std::runtime_error("Cannot create native HLS/DASH client");
  gf_dash_set_algo(dash, GF_DASH_ALGO_NONE);
  gf_dash_disable_speed_adaptation(dash, GF_TRUE);
}
MediaJob::~MediaJob()
{
  if (dash)
    gf_dash_del(dash);
}

Snapshot MediaJob::snapshot()
{
  std::lock_guard<std::mutex> lock(control->mutex);
  return control->snapshot;
}
void MediaJob::publish(Snapshot value)
{
  store.save(value);
  std::lock_guard<std::mutex> lock(control->mutex);
  control->snapshot = std::move(value);
}
void MediaJob::waitForNext()
{
  const auto delay = std::chrono::milliseconds(
      std::max<u32>(10, gf_dash_get_min_wait_ms(dash)));
  std::unique_lock<std::mutex> lock(control->mutex);
  control->wake.wait_for(lock, delay,
                         [&] { return control->cancel || control->finish; });
}

void MediaJob::manifestUpdated(const char* name, const char* path, int group)
{
  if (gf_dash_is_dynamic_mpd(dash)) {
    return;
  }
  std::ifstream input(nativePath(path), std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(input)), {});
  if (store.manifest(std::to_string(group) + ":" + (name ? name : ""),
                     Transport::fingerprint(text))) {
    transport.invalidate();
    retained.clear();
    coverage.clear();
    lastSegments.clear();
    retainedPaths.clear();
    retainedBytes = 0;
    auto value = snapshot();
    value.downloadedLength = 0;
    value.completedDuration = 0;
    publish(value);
  }
}

} // namespace aria2::media
