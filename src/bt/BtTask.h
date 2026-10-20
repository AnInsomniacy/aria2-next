/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "core/Task.h"
#include "disk/DiskPool.h"

#include <boost/json/array.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include <optional>
#include <utility>

namespace aria2::bt {

namespace lt = libtorrent;

// One torrent. While active it is a libtorrent torrent; when it stops, its
// resume data is saved and the torrent leaves the session, so the engine
// queue alone decides what runs.
//
// Magnet links are added with every file deselected. When metadata arrives the
// selection from select-file is applied, or, with pause-metadata over RPC, the
// task pauses until a client sets select-file and unpauses it.
class BtTask final : public Task {
public:
  // `params` come from metainfo or a magnet link, possibly overlaid with
  // stored resume data. `torrentPath` is the stored metainfo, empty for magnets.
  BtTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
         lt::add_torrent_params params, std::string torrentPath);
  ~BtTask() override;

  void start() override;
  void stop(bool force, std::function<void()> done) override;

  int64_t totalLength() const override;
  int64_t completedLength() const override;
  int64_t downloadSpeed() const override;
  int64_t uploadSpeed() const override;
  int64_t uploadLength() const override;
  int connections() const override;
  std::vector<TaskFile> files() const override;
  void status(boost::json::object& out, const StatusKeys& keys) const override;
  std::vector<std::string> sessionUris() const override;
  void checkpoint() override;
  void discardState() override;
  void optionChanged(std::string_view name) override;
  std::optional<Moves> relocation(const std::string& dir) const override;
  bool countsTowardConcurrency() const override;

  std::string infoHash() const;
  // RPC views and controls of a running torrent; Error otherwise.
  boost::json::array peers() const;
  boost::json::array trackers() const;
  void replaceTrackers(const std::vector<std::pair<std::string, int>>& trackers);
  void replaceWebSeeds(const std::vector<std::string>& seeds);
  // "host:port" or "[v6]:port"; returns {added, rejected}. Peers given
  // before the torrent runs are connected when it starts.
  std::pair<int, int> addPeers(const std::vector<std::string>& peers);
  void recheck();
  // Announces to trackers, DHT and local peer discovery now; trackers still
  // enforce their minimum interval.
  void announce();

  // Called by Session on the control thread.
  void attached(const lt::torrent_handle& handle);
  void addFailed(const lt::error_code& error);
  void handle(lt::alert* alert);
  void updated(const lt::torrent_status& status);
  void detached();

private:
  enum class Phase { Idle, Adding, Running, Saving, Removing };
  enum class Outcome { Stop, Complete, Fail };

  lt::add_torrent_params addParams();
  void metadataReceived();
  bool awaitingSelection() const;
  std::vector<lt::download_priority_t> priorities() const;
  void applyPriorities();
  void applyLimits();
  void evaluateSeeding();
  void finish(Outcome outcome, ErrorCode code = ErrorCode::Finished, std::string message = {});
  void saveResume(lt::resume_data_flags_t flags);
  void resumeSaved(const lt::add_torrent_params& params);
  void maybeRemove();
  const lt::torrent_handle& running() const;
  std::string resumePath() const;
  std::string savePath() const;
  int64_t storedCompleted() const;
  int64_t wantedLength() const;
  std::string selectionState() const;
  std::string torrentState() const;

  lt::add_torrent_params params_;
  std::string torrentPath_;
  std::shared_ptr<const lt::torrent_info> info_;
  lt::torrent_handle handle_;
  lt::torrent_status status_;
  bool hasStatus_ = false;
  // libtorrent recomputes its rates once a second; metering the payload
  // totals keeps BT speeds as live as HTTP and ED2K ones.
  mutable SpeedMeter downloadMeter_;
  mutable SpeedMeter uploadMeter_;
  Phase phase_ = Phase::Idle;
  Outcome outcome_ = Outcome::Stop;
  ErrorCode failure_ = ErrorCode::Finished;
  std::string failureMessage_;
  std::function<void()> stopped_;
  int pendingSaves_ = 0;
  int pendingWrites_ = 0;
  bool finished_ = false;      // all wanted data was downloaded once
  bool hashCheckOnly_ = false; // complete after the forced check
  mutable bool wantFileProgress_ = false;
  std::vector<int64_t> fileProgress_;
  std::vector<lt::tcp::endpoint> pendingPeers_;
  std::optional<DiskPool::Strand> strand_;
};

} // namespace aria2::bt
