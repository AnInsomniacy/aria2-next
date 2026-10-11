/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "media/MediaTask.h"

#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "core/Engine.h"
#include "disk/File.h"
#include "media/MediaFiles.h"
#include "media/MediaSession.h"
#include "media/MediaStore.h"
#include "state/StateStore.h"

#include <boost/json/array.hpp>

#include <filesystem>

namespace aria2::media {

namespace json = boost::json;
using namespace std::chrono_literals;

namespace {

constexpr auto pollInterval = 200ms;

std::string urlPath(const std::string& uri)
{
  const auto parts = uri::parse(uri);
  return parts ? text::lower(parts->path) : std::string();
}

// out, else the page title or file name hint, else the URL's file name; the
// extension always follows media-format.
std::string mediaName(const Options& options, const std::string& uri)
{
  const auto format = options.get("media-format");
  auto name = options.get("out");
  if (name.empty()) {
    name = safeFileName(options.get("filename-hint"));
    if (!name.empty() && options.get("filename-hint-source") == "title") {
      // A title is not a file name; "Part 1.5" keeps its dot.
      return name + "." + format;
    }
    if (name.empty()) {
      name = uri::fileName(uri);
    }
    if (name.empty()) {
      name = "media";
    }
  }
  auto path = toPath(name);
  path.replace_extension("." + format);
  return toUtf8(path);
}

ErrorCode errorCodeFor(FailureCode code)
{
  return code == FailureCode::AuthenticationRequired ? ErrorCode::HttpAuth
                                                     : ErrorCode::Network;
}

} // namespace

MediaTask::MediaTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
                     std::string uri)
    : Task(engine, gid, std::move(options)),
      uri_(std::move(uri)),
      timer_(engine.io())
{
  restore();
}

MediaTask::~MediaTask()
{
  if (control_) {
    control_->requestCancel();
  }
}

bool MediaTask::handles(const std::string& uri, const Options& options)
{
  const auto mode = options.get("media");
  if (mode == "hls" || mode == "dash" || mode == "collection") {
    return true;
  }
  if (mode != "auto") {
    return false;
  }
  const auto path = urlPath(uri);
  return path.ends_with(".m3u8") || path.ends_with(".mpd");
}

bool MediaTask::manifestType(std::string_view contentType)
{
  const auto type = text::trim(contentType.substr(0, contentType.find(';')));
  for (const auto* manifest :
       {"application/vnd.apple.mpegurl", "application/x-mpegurl",
        "audio/mpegurl", "audio/x-mpegurl", "application/dash+xml"}) {
    if (text::iequals(type, manifest)) {
      return true;
    }
  }
  return false;
}

// A task from a saved session shows its recorded progress before it starts.
void MediaTask::restore()
{
  directory_ = engine().state().directory("media");
  if (fileExists(joinPath(directory_, "state.db"))) {
    try {
      Store store(directory_, gidText(gid()));
      if (store.load(snapshot_) && snapshot_.state != "awaiting-selection") {
        snapshot_.state = "paused";
      }
    }
    catch (const std::exception& error) {
      snapshot_.error = failureMessage(error);
    }
  }
}

std::string MediaTask::outputPath() const
{
  if (!snapshot_.path.empty()) {
    return snapshot_.path;
  }
  return toUtf8(std::filesystem::absolute(
      toPath(joinPath(options().get("dir"), mediaName(options(), uri_)))));
}

int64_t MediaTask::downloadLimit() const
{
  auto limit = options().integer("max-download-limit");
  // The overall limit is shared evenly; media workers cannot draw from the
  // HTTP token bucket on the control thread.
  if (const auto overall =
          engine().global().integer("max-overall-download-limit");
      overall > 0) {
    const auto share =
        std::max<int64_t>(1, overall / static_cast<int64_t>(std::max<size_t>(
                                           1, engine().active().size())));
    limit = limit > 0 ? std::min(limit, share) : share;
  }
  return limit;
}

void MediaTask::start()
{
  try {
    if (snapshot_.protocol.empty()) {
      const auto mode = options().get("media");
      snapshot_.protocol = mode == "collection" ? "collection"
                           : mode == "dash" || urlPath(uri_).ends_with(".mpd")
                               ? "dash"
                               : "hls";
    }
    if (options().flag("dry-run")) {
      fail(ErrorCode::Option, "Use media-pause-after-probe to inspect media");
    }
    if (!options().blank("checksum")) {
      fail(ErrorCode::Option,
           "A source checksum cannot validate a remuxed presentation; "
           "use media=file to download the source unchanged");
    }
    auto path = toUtf8(std::filesystem::absolute(
        toPath(joinPath(options().get("dir"), mediaName(options(), uri_)))));
    if (fileExists(path) && !options().flag("allow-overwrite")) {
      // A pending publication of this task may already have placed it.
      Store store(directory_, gidText(gid()));
      const auto publication = store.publication();
      if (!publication || publication->output != path) {
        auto numbered = options().flag("auto-file-renaming")
                            ? numberedPath(path)
                            : std::nullopt;
        if (!numbered) {
          fail(ErrorCode::FileExists, "{} already exists", path);
        }
        path = std::move(*numbered);
        // Recovery must reuse this destination.
        options().set("out", toUtf8(toPath(path).filename()));
      }
    }
    for (const auto& task : engine().active()) {
      if (task.get() == this) {
        continue;
      }
      for (const auto& file : task->files()) {
        if (file.path == path) {
          fail(ErrorCode::DuplicateDownload, "Another task is writing {}",
               path);
        }
      }
    }
    std::filesystem::create_directories(nativePath(path).parent_path());
    snapshot_.path = std::move(path);
    snapshot_.state = "probing";
    snapshot_.error.clear();
    snapshot_.errorCode = FailureCode::None;
    control_ = std::make_shared<Control>();
    control_->snapshot = snapshot_;
    control_->finish = std::exchange(finishRequested_, false);
    control_->downloadLimit = downloadLimit();
    accounted_ = 0;
    session_ = std::make_unique<Session>(options().snapshot(), uri_,
                                         gidText(gid()), directory_, control_);
  }
  catch (const Error& error) {
    snapshot_.state = "error";
    snapshot_.error = error.what();
    throw;
  }
  catch (const std::exception& error) {
    snapshot_.state = "error";
    snapshot_.error = failureMessage(error);
    snapshot_.errorCode = failureCode(error);
    fail(ErrorCode::FileIo, "{}", snapshot_.error);
  }
  schedulePoll();
}

void MediaTask::schedulePoll()
{
  timer_.expires_after(pollInterval);
  timer_.async_wait(
      [this, self = shared_from_this()](boost::system::error_code error) {
        if (!error && control_) {
          poll();
        }
      });
}

void MediaTask::poll()
{
  {
    std::lock_guard lock(control_->mutex);
    snapshot_ = control_->snapshot;
  }
  // A presentation found through its Content-Type restores as media without
  // another probe.
  if (!snapshot_.tracks.empty() && options().get("media") == "auto") {
    options().set("media", snapshot_.protocol);
  }
  const auto received = control_->received.load();
  speed_.add(received - accounted_);
  accounted_ = received;
  control_->downloadLimit = downloadLimit();
  if (control_->done) {
    workerDone();
    return;
  }
  schedulePoll();
}

void MediaTask::workerDone()
{
  session_.reset(); // joins the finished worker
  control_.reset();
  if (stopped_) {
    std::exchange(stopped_, nullptr)();
    return;
  }
  if (snapshot_.state == "complete") {
    complete();
  }
  else if (snapshot_.state == "awaiting-selection") {
    engine().pause(shared_from_this(), false);
  }
  else if (snapshot_.state == "error") {
    log::error("{}: media: {}", gidText(gid()), snapshot_.error);
    failed(errorCodeFor(snapshot_.errorCode), snapshot_.error);
  }
  else {
    failed(ErrorCode::Unknown, "The media worker stopped unexpectedly");
  }
}

void MediaTask::stop(bool, std::function<void()> done)
{
  if (!control_) {
    done();
    return;
  }
  // The worker finishes its current write and records progress; poll()
  // reports completion.
  stopped_ = std::move(done);
  control_->requestCancel();
}

bool MediaTask::finishRecording()
{
  if (!snapshot_.live || snapshot_.state == "complete" ||
      state() == TaskState::Complete || state() == TaskState::Removed ||
      state() == TaskState::Error) {
    return false;
  }
  if (control_) {
    control_->requestFinish();
  }
  else {
    finishRequested_ = true;
  }
  return true;
}

void MediaTask::discardState()
{
  try {
    Store::discard(directory_, gidText(gid()));
  }
  catch (const std::exception& error) {
    log::warn("{}: cannot discard media state: {}", gidText(gid()),
              failureMessage(error));
  }
}

int64_t MediaTask::totalLength() const { return snapshot_.totalLength; }

int64_t MediaTask::completedLength() const
{
  return snapshot_.state == "complete" ? snapshot_.completedLength
                                       : snapshot_.downloadedLength;
}

int64_t MediaTask::downloadSpeed() const
{
  return control_ ? speed_.rate() : 0;
}

int MediaTask::connections() const
{
  return control_ ? control_->connections.load() : 0;
}

std::vector<TaskFile> MediaTask::files() const
{
  TaskFile file;
  file.path = outputPath();
  file.length = totalLength();
  file.completed =
      snapshot_.state == "complete" ? snapshot_.completedLength : 0;
  file.uris.push_back({uri_, state() == TaskState::Active});
  return {std::move(file)};
}

void MediaTask::status(json::object& out, const StatusKeys& keys) const
{
  Task::status(out, keys);
  if (!keys.want("media")) {
    return;
  }
  json::array tracks;
  for (const auto& track : snapshot_.tracks) {
    tracks.push_back(json::object{
        {"id", track.id},
        {"type", track.type},
        {"language", track.language},
        {"codec", track.codec},
        {"width", std::to_string(track.width)},
        {"height", std::to_string(track.height)},
        {"bandwidth", std::to_string(track.bandwidth)},
        {"frameRate", fmt::format("{:.6f}", track.frameRate)},
        {"selected", track.selected ? "true" : "false"},
    });
  }
  json::object media{
      {"state", snapshot_.state},
      {"protocol", snapshot_.protocol},
      {"live", snapshot_.live ? "true" : "false"},
      {"duration", std::to_string(snapshot_.duration)},
      {"completedDuration", std::to_string(snapshot_.completedDuration)},
      {"downloadedLength", std::to_string(snapshot_.downloadedLength)},
      {"lengthKnown", snapshot_.totalLength > 0 ? "true" : "false"},
      {"error", snapshot_.error},
      {"errorCode", failureCodeName(snapshot_.errorCode)},
      {"tracks", std::move(tracks)},
  };
  if (!snapshot_.live && snapshot_.duration > 0) {
    media["progress"] = fmt::format("{:.6f}", snapshot_.progress());
  }
  out["media"] = std::move(media);
}

} // namespace aria2::media
