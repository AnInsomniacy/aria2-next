/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <boost/asio/steady_timer.hpp>
#include <boost/json/object.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/fwd.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace aria2 {
class Engine;
}

namespace aria2::bt {

namespace lt = libtorrent;

class BtTask;

// The libtorrent session shared by all BitTorrent tasks. libtorrent runs its
// own network and disk threads; its alerts are drained on the control thread,
// so tasks only ever see them there.
//
// State below --state-dir/bittorrent (the layout of earlier releases):
//   session                 DHT routing table
//   torrents/<sha1>.torrent metainfo of tasks added from torrent data
//   torrents/<gid>.fastresume  resume data (earlier releases: <info hash>)
class Session {
public:
  explicit Session(Engine& engine);
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  void optionsChanged(const std::vector<std::string>& names);
  // Saves the DHT state and stops the session. Tasks are detached by then.
  void close();

  // Adds a torrent; the task later receives attached() or addFailed().
  void add(const std::shared_ptr<BtTask>& task, lt::add_torrent_params params);
  // Removes a torrent; the task later receives detached().
  void remove(const lt::torrent_handle& handle);

  lt::session& native() { return *session_; }

  boost::json::object status() const;

private:
  void schedule();
  void drain();
  void dispatch(lt::alert* alert);
  void tick();
  void loadBlocklist();
  void saveState();
  std::shared_ptr<BtTask> taskFor(const lt::torrent_handle& handle) const;

  Engine& engine_;
  std::unique_ptr<lt::session> session_;
  boost::asio::steady_timer ticker_;
  std::atomic<bool> drainPosted_{false};
  bool closed_ = false;

  std::map<const BtTask*, std::weak_ptr<BtTask>> adding_;
  std::map<lt::torrent_handle, std::weak_ptr<BtTask>> tasks_;

  // Diagnostics for getBtSessionStatus.
  std::map<std::string, int> listenPorts_; // address -> port (TCP and uTP share it)
  int mappedTcpPort_ = 0;
  int mappedUdpPort_ = 0;
  std::string portMappingError_;
  std::string externalAddress_;
  std::vector<std::int64_t> counters_;
  int dhtNodesMetric_ = -1;
  int peersMetric_ = -1;
  int downloadedMetric_ = -1;
  int uploadedMetric_ = -1;
  size_t blocklistRules_ = 0;
  int ticks_ = 0;
};

} // namespace aria2::bt
