/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "media/MediaError.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace aria2::media {

struct Track {
  std::string id, type, language, codec;
  int width = 0, height = 0;
  int64_t bandwidth = 0;
  bool selected = false;
  double frameRate = 0;
};

// What a media worker reports. Durations are milliseconds.
struct Snapshot {
  // waiting, probing, awaiting-selection, downloading, recording, finalizing,
  // paused, complete, error
  std::string state = "waiting";
  std::string protocol, path, error;
  FailureCode errorCode = FailureCode::None;
  std::vector<Track> tracks;
  int64_t duration = 0;
  int64_t completedDuration = 0;
  int64_t completedLength = 0;
  int64_t downloadedLength = 0;
  int64_t totalLength = 0;
  bool live = false;

  double progress() const
  {
    if (live || duration <= 0) {
      return 0;
    }
    return state == "complete"
               ? 1.0
               : std::clamp(static_cast<double>(completedDuration) / duration,
                            0.0, 1.0);
  }
};

// The only state shared between the control thread and a media worker.
// `snapshot` is guarded by `mutex`; `wake` interrupts the worker's waits.
struct Control {
  std::atomic<bool> cancel{false};
  std::atomic<bool> finish{false};
  std::atomic<bool> done{false};
  std::atomic<int64_t> received{0};
  std::atomic<int> connections{0};
  std::atomic<int64_t> downloadLimit{0};
  std::mutex mutex;
  std::condition_variable wake;
  Snapshot snapshot;

  void requestCancel()
  {
    {
      std::lock_guard lock(mutex);
      cancel = true;
    }
    wake.notify_all();
  }
  void requestFinish()
  {
    {
      std::lock_guard lock(mutex);
      finish = true;
    }
    wake.notify_all();
  }
};

} // namespace aria2::media
