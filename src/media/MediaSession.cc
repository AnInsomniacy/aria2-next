/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
extern "C" {
#include <libavutil/log.h>
}

#include "media/MediaSession.h"
#include <cstdarg>
#include <exception>
#include <gpac/setup.h>
#include <gpac/tools.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include "media/MediaJob.h"
#include "media/MediaFiles.h"
#include "base/Log.h"

namespace aria2::media {
namespace {
std::once_flag initialization;
void nativeLog(void* context, int level, const char* format,
               va_list args) noexcept
{
  if (level > AV_LOG_WARNING || !log::enabled(log::Level::Debug))
    return;
  try {
    char line[2048];
    int prefix = 1;
    av_log_format_line2(context, level, format, args, line, sizeof(line),
                        &prefix);
    std::string_view text(line);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
      text.remove_suffix(1);
    log::debug("media: ffmpeg: {}", text);
  }
  catch (...) {
  }
}
void initialize()
{
  // GPAC's null profile disables its independent on-disk application settings.
  if (gf_sys_init(GF_MemTrackerNone, "n") != GF_OK)
    throw std::runtime_error("Cannot initialize GPAC");
  gf_log_set_tools_levels("all@quiet", GF_TRUE);
  av_log_set_level(AV_LOG_WARNING);
  av_log_set_callback(nativeLog);
}
} // namespace

Session::Session(std::shared_ptr<const Options> option, std::string uri,
                 std::string gid, std::string directory,
                 std::shared_ptr<Control> control)
    : control_(std::move(control))
{
  worker_ = std::thread([option = std::move(option), uri = std::move(uri),
                         gid = std::move(gid), directory = std::move(directory),
                         control = control_] {
    try {
      std::call_once(initialization, initialize);
      MediaJob job(option, uri, gid, directory, control);
      job.run();
    }
    catch (const std::exception& error) {
      std::lock_guard<std::mutex> lock(control->mutex);
      if (!control->cancel && control->snapshot.state != "awaiting-selection") {
        control->snapshot.error = failureMessage(error);
        control->snapshot.errorCode = failureCode(error);
        control->snapshot.state = "error";
      }
    }
    std::lock_guard<std::mutex> lock(control->mutex);
    if (control->cancel && control->snapshot.state != "complete" &&
        control->snapshot.state != "awaiting-selection")
      control->snapshot.state = "paused";
    else if (control->finish && control->snapshot.state != "complete" &&
             control->snapshot.state != "error") {
      control->snapshot.state = "error";
      control->snapshot.error = "Recording stopped before its current segment "
                                "completed; resume to retry";
    }
    control->connections = 0;
    control->done = true;
  });
}
Session::~Session()
{
  control_->requestCancel();
  join();
}
void Session::join()
{
  if (worker_.joinable())
    worker_.join();
}

} // namespace aria2::media
