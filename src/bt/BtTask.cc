/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "bt/BtTask.h"

#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "bt/Metainfo.h"
#include "bt/Session.h"
#include "core/Engine.h"
#include "disk/File.h"
#include "state/StateStore.h"

#include <boost/asio/ip/address.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/announce_entry.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/write_resume_data.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace aria2::bt {

namespace json = boost::json;

namespace {

std::string hexBits(const lt::bitfield& bits)
{
  return bits.empty() ? std::string()
                      : text::toHex({bits.data(),
                                     static_cast<size_t>(bits.num_bytes())});
}

std::string flag(bool value) { return value ? "true" : "false"; }

int limit(int64_t value)
{
  return value > 0 ? static_cast<int>(std::min<int64_t>(value, INT32_MAX)) : -1;
}

ErrorCode storageError(const lt::error_code& error)
{
  if (error == boost::system::errc::no_space_on_device) {
    return ErrorCode::NotEnoughDiskSpace;
  }
  if (error == boost::system::errc::permission_denied) {
    return ErrorCode::FileOpen;
  }
  return ErrorCode::FileIo;
}

// qBittorrent's peer flag letters.
std::string peerFlags(const lt::peer_info& peer)
{
  std::string out;
  const auto has = [&peer](lt::peer_flags_t bit) {
    return bool(peer.flags & bit);
  };
  if (has(lt::peer_info::interesting)) {
    out += has(lt::peer_info::remote_choked) ? 'd' : 'D';
  }
  else if (!has(lt::peer_info::remote_choked)) {
    out += 'K';
  }
  if (has(lt::peer_info::remote_interested)) {
    out += has(lt::peer_info::choked) ? 'u' : 'U';
  }
  else if (!has(lt::peer_info::choked)) {
    out += '?';
  }
  if (has(lt::peer_info::optimistic_unchoke)) {
    out += 'O';
  }
  if (has(lt::peer_info::snubbed)) {
    out += 'S';
  }
  if (!has(lt::peer_info::outgoing_connection)) {
    out += 'I';
  }
  if (has(lt::peer_info::rc4_encrypted)) {
    out += 'E';
  }
  else if (has(lt::peer_info::plaintext_encrypted)) {
    out += 'e';
  }
  if (has(lt::peer_info::utp_socket)) {
    out += 'P';
  }
  if (peer.source & lt::peer_info::pex) {
    out += 'X';
  }
  if (peer.source & lt::peer_info::dht) {
    out += 'H';
  }
  if (peer.source & lt::peer_info::lsd) {
    out += 'L';
  }
  return out;
}

json::array peerSources(const lt::peer_info& peer)
{
  json::array out;
  const std::pair<lt::peer_source_flags_t, const char*> names[] = {
      {lt::peer_info::tracker, "tracker"},
      {lt::peer_info::dht, "dht"},
      {lt::peer_info::pex, "pex"},
      {lt::peer_info::lsd, "lsd"},
      {lt::peer_info::resume_data, "resume"},
      {lt::peer_info::incoming, "incoming"}};
  for (const auto& [bit, name] : names) {
    if (peer.source & bit) {
      out.push_back(json::string(name));
    }
  }
  return out;
}

} // namespace

BtTask::BtTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
               lt::add_torrent_params params, std::string torrentPath)
    : Task(engine, gid, std::move(options)),
      params_(std::move(params)),
      torrentPath_(std::move(torrentPath)),
      info_(params_.ti)
{
}

BtTask::~BtTask() = default;

std::string BtTask::infoHash() const { return hashText(params_.info_hashes); }

std::string BtTask::savePath() const
{
  return toUtf8(fs::absolute(toPath(options().get("dir"))));
}

std::optional<Task::Moves> BtTask::relocation(const std::string& dir) const
{
  Moves moves;
  if (!info_) {
    return moves; // nothing is stored before the metadata arrives
  }
  const auto from = savePath();
  const auto to = toUtf8(fs::absolute(toPath(dir)));
  const auto add = [&](const std::string& relative) {
    const auto source = joinPath(from, relative);
    if (fileExists(source)) {
      moves.emplace_back(source, joinPath(to, relative));
    }
  };
  const auto& layout = info_->layout();
  for (const auto index : layout.file_range()) {
    if (layout.pad_file_at(index)) {
      continue;
    }
    const auto renamed = params_.renamed_files.find(index);
    add(renamed != params_.renamed_files.end() ? renamed->second
                                               : layout.file_path(index));
  }
  // Pieces of unselected files that overlap selected ones (libtorrent's part
  // file, named after the info hash its storage uses).
  const auto hash = info_->info_hash();
  add("." +
      text::toHex({reinterpret_cast<const char*>(hash.data()), hash.size()}) +
      ".parts");
  return moves;
}

std::string BtTask::resumePath() const
{
  return joinPath(engine().state().directory("bittorrent/torrents"),
                  gidText(gid()) + ".fastresume");
}

std::vector<lt::download_priority_t> BtTask::priorities() const
{
  return info_ ? filePriorities(options(), info_->layout().num_files())
               : std::vector<lt::download_priority_t>{};
}

bool BtTask::awaitingSelection() const
{
  return torrentPath_.empty() && options().flag("pause-metadata") &&
         options().blank("select-file") && engine().global().flag("enable-rpc");
}

lt::add_torrent_params BtTask::addParams()
{
  auto params = params_;
  params.save_path = savePath();
  params.flags &=
      ~(lt::torrent_flags::paused | lt::torrent_flags::auto_managed |
        lt::torrent_flags::stop_when_ready | lt::torrent_flags::upload_mode |
        lt::torrent_flags::seed_mode);
  const auto set = [&params](lt::torrent_flags_t bit, bool on) {
    params.flags = on ? params.flags | bit : params.flags & ~bit;
  };
  // Resume data does not keep the subscription to post_torrent_updates().
  set(lt::torrent_flags::update_subscribe, true);
  set(lt::torrent_flags::sequential_download,
      options().flag("force-sequential"));
  set(lt::torrent_flags::super_seeding, options().flag("bt-super-seeding"));
  set(lt::torrent_flags::disable_pex, !options().flag("enable-peer-exchange"));
  set(lt::torrent_flags::disable_dht, !options().flag("enable-dht"));
  set(lt::torrent_flags::disable_lsd, !options().flag("bt-enable-lpd"));
  if (hashCheckOnly_) {
    // Without resume data libtorrent hashes every existing piece.
    params.have_pieces.clear();
    params.verified_pieces.clear();
    params.unfinished_pieces.clear();
  }
  else if (params.ti && params.have_pieces.empty() &&
           options().flag("bt-seed-unverified")) {
    set(lt::torrent_flags::seed_mode, true);
  }
  params.upload_limit = limit(options().integer("max-upload-limit"));
  params.download_limit = limit(options().integer("max-download-limit"));
  params.max_connections = limit(options().integer("bt-max-peers"));
  params.max_uploads = limit(options().integer("bt-max-uploads-per-torrent"));
  applyTrackerOptions(params, options());
  applyIndexOut(params, options());
  if (params.ti) {
    params.file_priorities = priorities();
    // Saved piece priorities would override the selection; first and last
    // piece priority is applied again once the torrent runs.
    params.piece_priorities.clear();
  }
  else {
    // Nothing is fetched before the files are known and selected.
    set(lt::torrent_flags::default_dont_download, true);
  }
  return params;
}

void BtTask::start()
{
  if (phase_ != Phase::Idle) {
    return;
  }
  hasStatus_ = false;
  finished_ = false;
  fileProgress_.clear();
  outcome_ = Outcome::Stop;
  hashCheckOnly_ = options().flag("hash-check-only");
  if (!options().blank("select-file")) {
    options().erase("pause-metadata"); // the selection is made
  }
  if (!strand_) {
    strand_ = engine().disk().strand();
  }
  auto params = addParams();
  phase_ = Phase::Adding;
  engine().bt().add(std::static_pointer_cast<BtTask>(shared_from_this()),
                    std::move(params));
}

void BtTask::attached(const lt::torrent_handle& handle)
{
  handle_ = handle;
  phase_ = Phase::Running;
  if (stopped_) {
    finish(Outcome::Stop);
    return;
  }
  if (info_ && options().flag("bt-first-last-piece-first")) {
    applyPriorities();
  }
  for (const auto& peer : std::exchange(pendingPeers_, {})) {
    handle_.connect_peer(peer);
  }
  log::info("{}: BitTorrent {} added", gidText(gid()), infoHash());
}

void BtTask::addFailed(const lt::error_code& error)
{
  phase_ = Phase::Idle;
  if (auto done = std::exchange(stopped_, nullptr)) {
    done();
    return;
  }
  failed(error == lt::errors::duplicate_torrent ? ErrorCode::DuplicateInfoHash
                                                : ErrorCode::Unknown,
         "Cannot add the torrent: " + error.message());
}

void BtTask::stop(bool, std::function<void()> done)
{
  stopped_ = std::move(done);
  switch (phase_) {
  case Phase::Idle:
    std::exchange(stopped_, nullptr)();
    break;
  case Phase::Running:
    finish(Outcome::Stop);
    break;
  case Phase::Adding: // attached() or addFailed() continues
  case Phase::Saving:
  case Phase::Removing: // detached() reports the stop
    break;
  }
}

void BtTask::finish(Outcome outcome, ErrorCode code, std::string message)
{
  if (phase_ != Phase::Running) {
    return;
  }
  outcome_ = outcome;
  failure_ = code;
  failureMessage_ = std::move(message);
  phase_ = Phase::Saving;
  handle_.pause();
  saveResume(lt::torrent_handle::flush_disk_cache |
             lt::torrent_handle::save_info_dict);
}

void BtTask::saveResume(lt::resume_data_flags_t flags)
{
  ++pendingSaves_;
  handle_.save_resume_data(flags);
}

void BtTask::resumeSaved(const lt::add_torrent_params& params)
{
  auto ti = params_.ti;
  params_ = params;
  if (!params_.ti) {
    params_.ti = std::move(ti);
  }
  auto data = lt::write_resume_data_buf(params_);
  ++pendingWrites_;
  engine().disk().run(
      *strand_,
      [path = resumePath(), data = std::move(data)] {
        return writeFileAtomically(path, {data.data(), data.size()});
      },
      [this, self = shared_from_this()](bool ok) {
        --pendingWrites_;
        if (!ok) {
          log::warn("{}: cannot write the resume data {}", gidText(gid()),
                    resumePath());
        }
        maybeRemove();
      });
}

void BtTask::maybeRemove()
{
  // The torrent leaves only after its resume data reached the disk, so a
  // later discardState() cannot race a write.
  if (phase_ == Phase::Saving && pendingSaves_ == 0 && pendingWrites_ == 0) {
    phase_ = Phase::Removing;
    engine().bt().remove(handle_);
  }
}

void BtTask::detached()
{
  handle_ = {};
  phase_ = Phase::Idle;
  if (auto done = std::exchange(stopped_, nullptr)) {
    done();
    return;
  }
  switch (outcome_) {
  case Outcome::Complete:
    complete();
    break;
  case Outcome::Fail:
    failed(failure_, failureMessage_);
    break;
  case Outcome::Stop:
    break;
  }
}

void BtTask::handle(lt::alert* alert)
{
  if (auto* saved = lt::alert_cast<lt::save_resume_data_alert>(alert)) {
    --pendingSaves_;
    resumeSaved(saved->params);
  }
  else if (auto* unsaved =
               lt::alert_cast<lt::save_resume_data_failed_alert>(alert)) {
    --pendingSaves_;
    if (unsaved->error != lt::errors::resume_data_not_modified) {
      log::warn("{}: resume data not saved: {}", gidText(gid()),
                unsaved->error.message());
    }
    maybeRemove();
  }
  else if (lt::alert_cast<lt::metadata_received_alert>(alert)) {
    metadataReceived();
  }
  else if (lt::alert_cast<lt::torrent_checked_alert>(alert)) {
    if (hashCheckOnly_) {
      finish(Outcome::Complete);
    }
  }
  else if (auto* progress = lt::alert_cast<lt::file_progress_alert>(alert)) {
    fileProgress_.assign(progress->files.begin(), progress->files.end());
  }
  else if (auto* fileError = lt::alert_cast<lt::file_error_alert>(alert)) {
    finish(Outcome::Fail, storageError(fileError->error),
           fmt::format("{}: {}", fileError->filename(),
                       fileError->error.message()));
  }
  else if (auto* torrentError =
               lt::alert_cast<lt::torrent_error_alert>(alert)) {
    finish(Outcome::Fail, storageError(torrentError->error),
           torrentError->error.message());
  }
  else if (auto* rejected =
               lt::alert_cast<lt::fastresume_rejected_alert>(alert)) {
    log::info("{}: resume data rejected ({}); checking the files",
              gidText(gid()), rejected->error.message());
  }
}

void BtTask::metadataReceived()
{
  const auto info = handle_.torrent_file();
  if (!info) {
    return;
  }
  info_ = info;
  params_.ti = std::make_shared<lt::torrent_info>(*info);
  log::notice("{}: metadata of {} received ({} files)", gidText(gid()),
              info->name(), info->layout().num_files());
  if (awaitingSelection()) {
    // Pausing saves the metadata with the resume data.
    engine().pause(shared_from_this(), false);
    return;
  }
  try {
    lt::add_torrent_params renames;
    renames.ti = params_.ti;
    applyIndexOut(renames, options());
    for (const auto& [index, path] : renames.renamed_files) {
      handle_.rename_file(index, path);
    }
    applyPriorities();
  }
  catch (const Error& error) {
    finish(Outcome::Fail, error.code(), error.what());
    return;
  }
  saveResume(lt::torrent_handle::save_info_dict);
}

void BtTask::applyPriorities()
{
  const auto wanted = priorities();
  handle_.prioritize_files(wanted);
  if (!options().flag("bt-first-last-piece-first")) {
    return;
  }
  const auto& layout = info_->layout();
  for (const auto index : layout.file_range()) {
    const auto size = layout.file_size(index);
    if (size == 0 || layout.pad_file_at(index) ||
        wanted[static_cast<size_t>(static_cast<int>(index))] ==
            lt::dont_download) {
      continue;
    }
    handle_.piece_priority(layout.map_file(index, 0, 0).piece,
                           lt::top_priority);
    handle_.piece_priority(layout.map_file(index, size - 1, 0).piece,
                           lt::top_priority);
  }
}

void BtTask::applyLimits()
{
  handle_.set_upload_limit(limit(options().integer("max-upload-limit")));
  handle_.set_download_limit(limit(options().integer("max-download-limit")));
  handle_.set_max_connections(limit(options().integer("bt-max-peers")));
  handle_.set_max_uploads(
      limit(options().integer("bt-max-uploads-per-torrent")));
}

void BtTask::updated(const lt::torrent_status& status)
{
  // A torrent that is leaving the session keeps its last running view.
  if (phase_ == Phase::Saving || phase_ == Phase::Removing) {
    return;
  }
  // The totals restart with every session of the torrent; a drop is no traffic.
  if (hasStatus_) {
    const auto now = Clock::now();
    const auto down =
        status.total_payload_download - status_.total_payload_download;
    const auto up = status.total_payload_upload - status_.total_payload_upload;
    downloadMeter_.add(std::max<int64_t>(down, 0), now);
    uploadMeter_.add(std::max<int64_t>(up, 0), now);
  }
  status_ = status;
  hasStatus_ = true;
  if (phase_ != Phase::Running) {
    return;
  }
  if (wantFileProgress_) {
    wantFileProgress_ = false;
    handle_.post_file_progress(lt::torrent_handle::piece_granularity);
  }
  const bool checking =
      status.state == lt::torrent_status::checking_files ||
      status.state == lt::torrent_status::checking_resume_data;
  if (checking) {
    return;
  }
  const bool done = status.has_metadata && status.total_wanted > 0 &&
                    (status.is_finished || status.is_seeding) &&
                    !awaitingSelection();
  if (done && !finished_) {
    finished_ = true;
    // A torrent that was already complete when it started (resumed, or
    // found on disk) downloaded nothing and is not announced again; one
    // that finished before its first status update is.
    if (status.total_payload_download > 0) {
      log::notice("Download complete: {}",
                  info_ ? info_->name() : bt::hashText(status.info_hashes));
      engine().emit(Event::BtComplete, *this);
    }
    saveResume(lt::torrent_handle::save_info_dict);
  }
  else if (!done) {
    finished_ = false; // more files were selected
  }
  evaluateSeeding();
}

void BtTask::evaluateSeeding()
{
  if (phase_ != Phase::Running || !finished_ || hashCheckOnly_) {
    return;
  }
  bool enough = false;
  if (!options().blank("seed-time")) {
    enough =
        status_.finished_duration.count() >= options().number("seed-time") * 60;
  }
  const auto ratio = options().number("seed-ratio");
  if (ratio > 0 && status_.total_wanted > 0) {
    enough = enough || static_cast<double>(status_.all_time_upload) >=
                           ratio * static_cast<double>(status_.total_wanted);
  }
  if (enough) {
    finish(Outcome::Complete);
  }
}

void BtTask::checkpoint()
{
  if (phase_ == Phase::Running) {
    saveResume(lt::torrent_handle::save_info_dict |
               lt::torrent_handle::only_if_modified);
  }
}

void BtTask::discardState()
{
  removeFile(resumePath());
  removeFile(joinPath(engine().state().directory("bittorrent/torrents"),
                      infoHash() + ".fastresume")); // named by earlier releases
  // Stored metainfo belongs to this task alone: an info hash runs once.
  if (!torrentPath_.empty() &&
      toPath(torrentPath_).parent_path() ==
          toPath(engine().state().directory("bittorrent/torrents"))) {
    removeFile(torrentPath_);
  }
}

void BtTask::optionChanged(std::string_view name)
{
  if (phase_ != Phase::Running) {
    return;
  }
  try {
    if (name == "select-file" || name == "bt-file-priority" ||
        name == "bt-first-last-piece-first") {
      if (info_) {
        applyPriorities();
      }
    }
    else if (name == "max-upload-limit" || name == "max-download-limit" ||
             name == "bt-max-peers" || name == "bt-max-uploads-per-torrent") {
      applyLimits();
    }
    else if (name == "bt-super-seeding") {
      handle_.set_flags(options().flag(name) ? lt::torrent_flags::super_seeding
                                             : lt::torrent_flags_t{},
                        lt::torrent_flags::super_seeding);
    }
    else if (name == "force-sequential") {
      handle_.set_flags(options().flag(name)
                            ? lt::torrent_flags::sequential_download
                            : lt::torrent_flags_t{},
                        lt::torrent_flags::sequential_download);
    }
    else if (name == "seed-ratio" || name == "seed-time") {
      evaluateSeeding();
    }
  }
  catch (const Error& error) {
    log::warn("{}: option {} not applied: {}", gidText(gid()), name,
              error.what());
  }
}

bool BtTask::countsTowardConcurrency() const
{
  return !(finished_ && options().flag("detach-share-only"));
}

const lt::torrent_handle& BtTask::running() const
{
  if (phase_ != Phase::Running || !handle_.is_valid()) {
    fail(ErrorCode::Unknown, "The torrent of GID {} is not running",
         gidText(gid()));
  }
  return handle_;
}

// ---------------------------------------------------------------- status --

int64_t BtTask::wantedLength() const
{
  if (!info_) {
    return 0;
  }
  std::vector<lt::download_priority_t> wanted;
  try {
    wanted = priorities();
  }
  catch (const Error&) {
  }
  const auto& layout = info_->layout();
  int64_t total = 0;
  for (const auto index : layout.file_range()) {
    const auto i = static_cast<size_t>(static_cast<int>(index));
    if (!layout.pad_file_at(index) &&
        (wanted.empty() || wanted[i] != lt::dont_download)) {
      total += layout.file_size(index);
    }
  }
  return total;
}

int64_t BtTask::storedCompleted() const
{
  if (!info_ || params_.have_pieces.empty()) {
    return 0;
  }
  const auto pieces = static_cast<int64_t>(params_.have_pieces.count());
  return std::min(wantedLength(), pieces * info_->piece_length());
}

int64_t BtTask::totalLength() const
{
  return hasStatus_ && phase_ != Phase::Idle ? status_.total_wanted
                                             : wantedLength();
}

int64_t BtTask::completedLength() const
{
  // While libtorrent checks resume data or files it reports nothing done yet;
  // the stored pieces are the better answer until the check ends.
  const bool checking =
      status_.state == lt::torrent_status::checking_files ||
      status_.state == lt::torrent_status::checking_resume_data;
  if (!hasStatus_ || phase_ == Phase::Idle || checking) {
    return storedCompleted();
  }
  return status_.total_wanted_done;
}

int64_t BtTask::downloadSpeed() const
{
  return hasStatus_ && phase_ == Phase::Running ? downloadMeter_.rate() : 0;
}

int64_t BtTask::uploadSpeed() const
{
  return hasStatus_ && phase_ == Phase::Running ? uploadMeter_.rate() : 0;
}

int64_t BtTask::uploadLength() const
{
  return hasStatus_ ? status_.all_time_upload : params_.total_uploaded;
}

int BtTask::connections() const
{
  return hasStatus_ && phase_ == Phase::Running ? status_.num_peers : 0;
}

std::vector<TaskFile> BtTask::files() const
{
  wantFileProgress_ = true;
  if (!info_) {
    TaskFile file;
    file.path = joinPath(options().get("dir"),
                         params_.name.empty() ? infoHash()
                                              : safeFileName(params_.name));
    return {file};
  }
  std::vector<lt::download_priority_t> wanted;
  try {
    wanted = priorities();
  }
  catch (const Error&) {
  }
  const auto& layout = info_->layout();
  const auto root = options().get("dir");
  std::vector<TaskFile> result;
  for (const auto index : layout.file_range()) {
    if (layout.pad_file_at(index)) {
      continue;
    }
    const auto i = static_cast<size_t>(static_cast<int>(index));
    const auto renamed = params_.renamed_files.find(index);
    TaskFile file;
    file.index = static_cast<int>(i) + 1;
    file.path =
        joinPath(root, toUtf8(toPath(renamed != params_.renamed_files.end()
                                         ? renamed->second
                                         : layout.file_path(index))));
    file.length = layout.file_size(index);
    file.selected = wanted.empty() || wanted[i] != lt::dont_download;
    file.completed = i < fileProgress_.size()
                         ? fileProgress_[i]
                         : (finished_ && file.selected ? file.length : 0);
    result.push_back(std::move(file));
  }
  return result;
}

std::string BtTask::selectionState() const
{
  if (!torrentPath_.empty() || !info_ || !options().flag("pause-metadata") ||
      (state() != TaskState::Paused && state() != TaskState::Waiting)) {
    return "none";
  }
  return options().blank("select-file") ? "awaiting" : "ready";
}

std::string BtTask::torrentState() const
{
  switch (state()) {
  case TaskState::Waiting:
    return "queued";
  case TaskState::Paused:
    return "paused";
  case TaskState::Error:
    return "error";
  case TaskState::Complete:
  case TaskState::Removed:
    return "stopped";
  case TaskState::Active:
    break;
  }
  if (phase_ == Phase::Saving || phase_ == Phase::Removing) {
    return "stopping";
  }
  if (phase_ != Phase::Running || !hasStatus_) {
    return "adding";
  }
  switch (status_.state) {
  case lt::torrent_status::checking_files:
  case lt::torrent_status::checking_resume_data:
    return "checking";
  case lt::torrent_status::downloading_metadata:
    return "downloadingMetadata";
  case lt::torrent_status::finished:
    return "finished";
  case lt::torrent_status::seeding:
    return "seeding";
  default:
    return "downloading";
  }
}

void BtTask::status(json::object& out, const StatusKeys& keys) const
{
  Task::status(out, keys);
  const bool live = hasStatus_ && phase_ == Phase::Running;
  const auto put = [&](std::string_view key, json::value value) {
    if (keys.want(key)) {
      out[key] = std::move(value);
    }
  };
  put("infoHash", json::string(infoHash()));
  put("numSeeders", json::string(std::to_string(live ? status_.num_seeds : 0)));
  // As in aria2, a torrent seeds once its selected files are complete;
  // libtorrent counts it a seed only with every piece.
  put("seeder", json::string(flag(live && finished_)));
  if (info_) {
    put("pieceLength", json::string(std::to_string(info_->piece_length())));
    put("numPieces", json::string(std::to_string(info_->num_pieces())));
    if (keys.want("bitfield")) {
      out["bitfield"] =
          hexBits(live ? static_cast<const lt::bitfield&>(status_.pieces)
                       : static_cast<const lt::bitfield&>(params_.have_pieces));
    }
  }
  if (live && status_.state == lt::torrent_status::checking_files) {
    put("verifiedLength",
        json::string(std::to_string(static_cast<int64_t>(
            static_cast<double>(status_.total_wanted) * status_.progress))));
    put("verifyIntegrityPending", json::string("true"));
  }
  if (!keys.want("bittorrent")) {
    return;
  }
  json::object bt;
  std::map<int, json::array> tiers;
  for (size_t i = 0; i < params_.trackers.size(); ++i) {
    const auto tier =
        i < params_.tracker_tiers.size() ? params_.tracker_tiers[i] : 0;
    tiers[tier].push_back(json::string(params_.trackers[i]));
  }
  json::array announceList;
  for (auto& [tier, uris] : tiers) {
    announceList.push_back(std::move(uris));
  }
  bt["announceList"] = std::move(announceList);
  json::array webSeeds;
  for (const auto& seed : params_.url_seeds) {
    webSeeds.push_back(json::string(seed));
  }
  bt["webSeeds"] = std::move(webSeeds);
  bt["magnetLink"] = magnetLink(params_);
  bt["infoHashV1"] = params_.info_hashes.has_v1()
                         ? text::toHex({params_.info_hashes.v1.data(), 20})
                         : std::string();
  bt["infoHashV2"] = params_.info_hashes.has_v2()
                         ? text::toHex({params_.info_hashes.v2.data(), 32})
                         : std::string();
  bt["state"] = torrentState();
  bt["fileSelectionState"] = selectionState();
  if (info_) {
    bt["info"] = json::object{{"name", info_->name()}};
    bt["mode"] = info_->layout().num_files() > 1 ? "multi" : "single";
    bt["privateTorrent"] = flag(info_->priv());
    if (!params_.comment.empty()) {
      bt["comment"] = params_.comment;
    }
    if (params_.creation_date > 0) {
      bt["creationDate"] = static_cast<int64_t>(params_.creation_date);
    }
  }
  else if (!params_.name.empty()) {
    bt["info"] = json::object{{"name", params_.name}};
  }
  if (live) {
    bt["numPeers"] = std::to_string(status_.num_peers);
    bt["numSeeds"] = std::to_string(status_.num_seeds);
    bt["numComplete"] = std::to_string(std::max(status_.num_complete, 0));
    bt["numIncomplete"] = std::to_string(std::max(status_.num_incomplete, 0));
    bt["progress"] = fmt::format("{:.6f}", status_.progress_ppm / 1e6);
    if (status_.distributed_copies >= 0) {
      bt["availability"] = fmt::format("{:.3f}", status_.distributed_copies);
    }
    bt["currentTracker"] = status_.current_tracker;
    bt["activeTime"] = std::to_string(status_.active_duration.count());
    bt["finishedTime"] = std::to_string(status_.finished_duration.count());
    // libtorrent's seeding time skips torrents with unselected files.
    bt["seedingTime"] = std::to_string(
        finished_ ? status_.finished_duration.count() : 0);
  }
  out["bittorrent"] = std::move(bt);
}

std::vector<std::string> BtTask::sessionUris() const
{
  return {torrentPath_.empty() ? magnetLink(params_) : torrentPath_};
}

// ------------------------------------------------------------ RPC views --

json::array BtTask::peers() const
{
  std::vector<lt::peer_info> list;
  running().get_peer_info(list);
  json::array out;
  for (const auto& peer : list) {
    const auto has = [&peer](lt::peer_flags_t bit) {
      return bool(peer.flags & bit);
    };
    json::object entry;
    const auto remote = peer.remote_endpoint();
    entry["ip"] = remote.address().to_string();
    entry["port"] = std::to_string(remote.port());
    entry["peerId"] = text::percentEncode(std::string_view(
        peer.pid.data(), static_cast<size_t>(peer.pid.size())));
    entry["peerClientName"] = peer.client;
    entry["bitfield"] = hexBits(peer.pieces);
    entry["amChoking"] = flag(has(lt::peer_info::choked));
    entry["peerChoking"] = flag(has(lt::peer_info::remote_choked));
    entry["downloadSpeed"] = std::to_string(peer.payload_down_speed);
    entry["uploadSpeed"] = std::to_string(peer.payload_up_speed);
    entry["downloaded"] = std::to_string(peer.total_download);
    entry["uploaded"] = std::to_string(peer.total_upload);
    entry["progress"] = fmt::format("{:.6f}", peer.progress_ppm / 1e6);
    entry["seeder"] = flag(has(lt::peer_info::seed));
    entry["state"] = has(lt::peer_info::connecting)  ? "connecting"
                     : has(lt::peer_info::handshake) ? "handshaking"
                                                     : "connected";
    entry["transport"] = has(lt::peer_info::utp_socket) ? "utp" : "tcp";
    entry["encryption"] = has(lt::peer_info::ssl_socket)      ? "tls"
                          : has(lt::peer_info::rc4_encrypted) ? "rc4"
                          : has(lt::peer_info::plaintext_encrypted)
                              ? "encryptedHandshake"
                              : "plain";
    entry["incoming"] = flag(!has(lt::peer_info::outgoing_connection));
    entry["flags"] = peerFlags(peer);
    entry["sources"] = peerSources(peer);
    out.push_back(std::move(entry));
  }
  return out;
}

json::array BtTask::trackers() const
{
  json::array out;
  std::set<std::string> userTrackers;
  for (const auto& part : text::split(options().get("bt-tracker"), ',')) {
    userTrackers.emplace(text::trim(part));
  }
  const auto now = lt::clock_type::now();
  for (const auto& tracker : running().trackers()) {
    int fails = 0;
    int seeders = -1;
    int leechers = -1;
    int downloads = -1;
    bool updating = false;
    std::string message;
    std::string error;
    int64_t next = -1;
    for (const auto& endpoint : tracker.endpoints) {
      for (const auto& hash : endpoint.info_hashes) {
        // A listen address that cannot reach the tracker at all (IPv6 for
        // an IPv4-only tracker) is skipped, not a failure of the tracker.
        if (hash.last_error == lt::errors::announce_skipped) {
          continue;
        }
        fails = std::max(fails, static_cast<int>(hash.fails));
        seeders = std::max(seeders, hash.scrape_complete);
        leechers = std::max(leechers, hash.scrape_incomplete);
        downloads = std::max(downloads, hash.scrape_downloaded);
        updating = updating || hash.updating;
        if (!hash.message.empty()) {
          message = hash.message;
        }
        if (hash.last_error) {
          error = hash.last_error.message();
        }
        if (hash.next_announce > now) {
          const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                   hash.next_announce - now)
                                   .count();
          next = next < 0 ? seconds : std::min<int64_t>(next, seconds);
        }
      }
    }
    const auto status = updating           ? "updating"
                        : tracker.verified ? "working"
                        : !error.empty()   ? "error"
                                           : "notContacted";
    // libtorrent files every tracker given at add time under the magnet
    // link, including those from --bt-tracker.
    const auto source =
        userTrackers.count(tracker.url)                             ? "user"
        : (tracker.source & lt::announce_entry::source_torrent)     ? "torrent"
        : (tracker.source & lt::announce_entry::source_magnet_link) ? "magnet"
        : (tracker.source & lt::announce_entry::source_tex)         ? "exchange"
                                                                    : "user";
    json::object entry{
        {"url", tracker.url},
        {"tier", std::to_string(tracker.tier)},
        {"source", source},
        {"status", status},
        {"failures", std::to_string(fails)},
        {"seeders", std::to_string(seeders)},
        {"leechers", std::to_string(leechers)},
        {"downloads", std::to_string(downloads)},
        {"nextAnnounce", std::to_string(std::max<int64_t>(next, 0))},
        {"updating", flag(updating)},
        {"verified", flag(tracker.verified)}};
    if (!error.empty() || !message.empty()) {
      entry["message"] = error.empty() ? message : error;
    }
    out.push_back(std::move(entry));
  }
  return out;
}

void BtTask::replaceTrackers(
    const std::vector<std::pair<std::string, int>>& trackers)
{
  std::vector<lt::announce_entry> entries;
  params_.trackers.clear();
  params_.tracker_tiers.clear();
  for (const auto& [url, tier] : trackers) {
    lt::announce_entry entry(url);
    entry.tier = static_cast<std::uint8_t>(std::clamp(tier, 0, 255));
    entries.push_back(std::move(entry));
    params_.trackers.push_back(url);
    params_.tracker_tiers.push_back(tier);
  }
  running().replace_trackers(entries);
}

void BtTask::replaceWebSeeds(const std::vector<std::string>& seeds)
{
  const auto& handle = running();
  for (const auto& current : handle.url_seeds()) {
    if (std::find(seeds.begin(), seeds.end(), current) == seeds.end()) {
      handle.remove_url_seed(current);
    }
  }
  for (const auto& seed : seeds) {
    handle.add_url_seed(seed);
  }
  params_.url_seeds = seeds;
}

std::pair<int, int> BtTask::addPeers(const std::vector<std::string>& peers)
{
  int added = 0;
  for (const auto& peer : peers) {
    const auto colon = peer.rfind(':');
    auto host = std::string_view(peer).substr(0, colon);
    if (host.size() > 2 && host.front() == '[' && host.back() == ']') {
      host = host.substr(1, host.size() - 2);
    }
    const auto port =
        colon == std::string::npos
            ? std::nullopt
            : text::toInt(std::string_view(peer).substr(colon + 1));
    boost::system::error_code error;
    const auto address = boost::asio::ip::make_address(host, error);
    if (error || !port || *port < 1 || *port > 65535) {
      continue;
    }
    const lt::tcp::endpoint endpoint(address,
                                     static_cast<unsigned short>(*port));
    if (phase_ == Phase::Running) {
      handle_.connect_peer(endpoint);
    }
    else {
      pendingPeers_.push_back(endpoint);
    }
    ++added;
  }
  return {added, static_cast<int>(peers.size()) - added};
}

void BtTask::recheck() { running().force_recheck(); }

} // namespace aria2::bt
