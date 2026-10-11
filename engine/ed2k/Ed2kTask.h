/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Task.h"
#include "disk/DiskPool.h"
#include "ed2k/Link.h"
#include "ed2k/Parts.h"

#include <boost/asio/steady_timer.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aria2 {
class File;
}

namespace aria2::ed2k {

class Client;
class Network;
enum class Origin;

// One ED2K file link. The file is downloaded from sources found in the link,
// on servers, in Kad and through other clients, part by part, each part
// verified against its MD4 hash before it is shared. When complete it stays
// shared (status "seeder") until seed-time or seed-ratio is reached.
class Ed2kTask final : public Task {
public:
  Ed2kTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
           Link link);
  ~Ed2kTask() override;

  void start() override;
  void stop(bool force, std::function<void()> done) override;

  int64_t totalLength() const override { return link_.size; }
  int64_t completedLength() const override;
  int64_t downloadSpeed() const override;
  int64_t uploadSpeed() const override;
  int64_t uploadLength() const override { return uploaded_; }
  int connections() const override;
  std::vector<TaskFile> files() const override;
  void status(boost::json::object& out, const StatusKeys& keys) const override;
  std::vector<std::string> sessionUris() const override;
  void checkpoint() override;
  void loadProgress() override;
  void discardState() override;
  void optionChanged(std::string_view name) override;
  bool countsTowardConcurrency() const override;

  const Link& link() const { return link_; }
  const std::string& hash() const { return link_.hash; }
  const std::string& name() const { return link_.name; }
  int64_t size() const { return link_.size; }
  const Parts& parts() const { return parts_; }

  // Used by clients.
  bool downloading() const { return phase_ == Phase::Downloading; }
  // Parts others may download from us; empty when the file is complete.
  std::vector<bool> sharedParts() const;
  bool sharesAnything() const;
  bool needsHashSet() const;
  const std::vector<std::string>& pieceHashes() const { return pieceHashes_; }
  bool acceptHashSet(std::vector<std::string> hashes);
  std::vector<Range> claim(Client& client, const std::vector<bool>& available,
                           size_t count);
  void release(Client& client, const Range& block);
  void releaseAll(Client& client);
  bool claimed(Client& client, int64_t begin, int64_t end) const;
  bool wants(const std::vector<bool>& available) const;
  // Data of a claimed block, written in the order of arrival.
  void receive(int64_t begin, std::string data);
  // A block read for upload; `compressed` holds its deflated form when that
  // is smaller.
  struct Block {
    std::string data;
    std::string compressed;
  };
  void read(int64_t begin, int64_t end, bool compress,
            std::function<void(std::optional<Block>)> done);
  void addSource(const Endpoint& endpoint, uint32_t clientId,
                 const Endpoint& server, Origin origin);
  const std::vector<std::shared_ptr<Client>>& sources() const
  {
    return sources_;
  }
  void sourceChanged();
  void noteUploaded(int64_t bytes);

private:
  enum class Phase { Idle, Opening, Checking, Downloading, Sharing, Closing };
  enum class Outcome { Stop, Complete, Fail };

  void open();
  void opened(std::error_code error, bool check);
  void checkAll();
  void checked();
  void hashPart(size_t index, std::function<void(bool)> done);
  void digestPart(size_t index, std::function<void(std::string)> done);
  void verifyFilled(size_t index);
  void partVerified(size_t index, bool ok);
  void downloaded();
  void run();
  void tick();
  void schedule();
  void dropSources();
  void evaluateSharing();
  void finish(Outcome outcome, ErrorCode code = ErrorCode::Finished,
              std::string message = {});
  void quiesce();
  std::vector<int> availability() const;
  int64_t sharingSeconds() const;
  std::string defaultPath() const;

  Link link_;
  std::string path_;
  Parts parts_;
  std::vector<std::string> pieceHashes_;
  std::vector<std::shared_ptr<Client>> sources_;
  Phase phase_ = Phase::Idle;
  Outcome outcome_ = Outcome::Stop;
  ErrorCode failure_ = ErrorCode::Finished;
  std::string failureMessage_;
  std::shared_ptr<File> file_;
  std::optional<DiskPool::Strand> strand_;
  int pendingWrites_ = 0;
  int pendingHashes_ = 0;
  // Filled parts waiting for the hash set; with `unchecked_`, parts of a
  // file that existed before we knew the hash set, hashed as `localHashes_`.
  std::vector<size_t> waitingForHashSet_;
  bool unchecked_ = false;
  std::vector<std::string> localHashes_;
  std::function<void()> stopped_;
  boost::asio::steady_timer ticker_;
  bool scheduled_ = false;
  SpeedMeter downloadSpeed_;
  mutable SpeedMeter uploadSpeed_;
  int64_t uploaded_ = 0;
  int incompressible_ = 0; // upload blocks in a row that did not shrink
  uint32_t compressionSkips_ = 0;
  int64_t sharedBefore_ = 0; // seconds shared in earlier runs
  Clock::time_point sharingSince_{};
  bool attached_ = false;
  bool opened_ = false;    // the file was opened by this run
  bool announced_ = false; // BtComplete sent for this run
  int corruptParts_ = 0;
};

} // namespace aria2::ed2k
