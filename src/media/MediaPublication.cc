/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"
#include "media/MediaStore.h"
#include "media/MediaTransport.h"
#include <cstdint>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <utility>
#include "media/MediaJob.h"
#include "media/MediaFiles.h"
#include "media/MediaMuxer.h"
#include "base/Log.h"
#include <filesystem>

namespace aria2::media {

void MediaJob::complete(const Publication& publication)
{
  syncParent(publication.output);
  auto value = snapshot();
  value.state = "complete";
  value.completedLength = value.totalLength = publication.bytes;
  {
    std::lock_guard<std::mutex> lock(control->mutex);
    control->snapshot = std::move(value);
  }
  // Once publication succeeded, cleanup errors cannot turn valid output
  // into a failed download. The publication record makes cleanup retryable.
  try {
    // Segments fetched ahead past a range end are still open, and Windows
    // cannot delete open files.
    transport.invalidate();
    std::filesystem::remove_all(nativePath(taskDirectory));
    store.remove();
  }
  catch (const std::exception& error) {
    log::warn("media: recovery cleanup failed: {}", failureMessage(error));
  }
}

bool MediaJob::recoverPublication()
{
  auto publication = store.publication();
  if (!publication)
    return false;
  if (publication->output != snapshot().path ||
      publication->staging !=
          publication->output + "." + gid + ".media-partial")
    throw std::runtime_error(
        "Media output options changed during finalization");
  auto matches = [&](const std::string& path) {
    return std::filesystem::is_regular_file(nativePath(path)) &&
           std::filesystem::file_size(nativePath(path)) ==
               static_cast<uint64_t>(publication->bytes) &&
           Transport::digest(path) == publication->digest;
  };
  if (matches(publication->output)) {
    std::filesystem::remove(nativePath(publication->staging));
    complete(*publication);
    return true;
  }
  if (matches(publication->staging)) {
    Muxer::publish(publication->staging, publication->output,
                   option->flag("allow-overwrite"));
    complete(*publication);
    return true;
  }
  std::filesystem::remove(nativePath(publication->staging));
  store.clearPublication();
  return false;
}

void MediaJob::finalize()
{
  auto value = snapshot();
  value.state = "finalizing";
  publish(value);
  auto staging = Muxer::stage(
      store.segments(), value.path, taskDirectory, option->get("media-format"),
      option->get("media-video") != "none",
      option->get("media-audio") != "none",
      option->get("media-subtitles") != "none", control, value.duration,
      value.live, value.protocol == "collection",
      option->integer("media-start-time") > 0 ||
          option->integer("media-end-time") > 0);
  Publication publication{
      value.path, staging, Transport::digest(staging),
      static_cast<int64_t>(std::filesystem::file_size(nativePath(staging)))};
  store.preparePublication(publication);
  if (control->cancel)
    return;
  Muxer::publish(staging, value.path, option->flag("allow-overwrite"));
  complete(publication);
}

} // namespace aria2::media
