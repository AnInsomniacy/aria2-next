/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// HLS/DASH extensions: aria2.finishMedia ends a live recording and
// aria2.retryMedia restarts a failed presentation from its recorded progress.
#include "rpc/Params.h"

#include "core/Engine.h"
#include "media/MediaTask.h"
#include "options/Options.h"
#include "rpc/Dispatcher.h"

namespace aria2::rpc {

namespace {

std::shared_ptr<media::MediaTask> mediaTask(Engine& engine,
                                            const json::array& params)
{
  auto found =
      std::dynamic_pointer_cast<media::MediaTask>(task(engine, params));
  if (!found) {
    invalid(
        fmt::format("GID {} is not a media download", text(params, 0, "GID")));
  }
  return found;
}

} // namespace

void registerMediaMethods(Dispatcher& d, Engine& e)
{
  d.add("aria2.finishMedia", [&e](json::array& p) -> json::value {
    auto found = mediaTask(e, p);
    if (!found->finishRecording()) {
      invalid("The task is not an active live recording");
    }
    if (found->state() == TaskState::Paused) {
      e.unpause(found);
    }
    return json::string(gidOf(*found));
  });
  d.add("aria2.retryMedia", [&e](json::array& p) -> json::value {
    auto found = mediaTask(e, p);
    if (found->state() != TaskState::Error || !found->failedMedia()) {
      invalid("Only failed media tasks can be retried");
    }
    for (const auto& [name, value] : optionPairs(p, 1)) {
      const auto* spec = findOption(name);
      if (!spec) {
        invalid("Unknown option " + name);
      }
      if (spec->scope & scope::Task) {
        found->options().set(name, value);
      }
    }
    e.requeue(found);
    return json::string(gidOf(*found));
  });
}

} // namespace aria2::rpc
