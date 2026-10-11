/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Ed2kTask.h"

#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "core/Engine.h"
#include "disk/File.h"
#include "ed2k/Client.h"
#include "ed2k/Compression.h"
#include "ed2k/Hash.h"
#include "ed2k/Network.h"
#include "ed2k/ServerSession.h"
#include "ed2k/Store.h"
#include "state/StateStore.h"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/post.hpp>

#include <algorithm>
#include <filesystem>

namespace aria2::ed2k {

namespace json = boost::json;
using namespace std::chrono_literals;

namespace {

// Sources asked at the same time; the rest wait their turn.
constexpr size_t maxAsking = 12;
constexpr size_t maxSources = 1000;
// Parts are read in pieces of this size when hashing.
constexpr size_t hashChunk = 1 << 20;

std::string flag(bool value) { return value ? "true" : "false"; }

int64_t modificationTime(const std::string& path)
{
  std::error_code error;
  const auto time = std::filesystem::last_write_time(toPath(path), error);
  if (error) {
    return 0;
  }
  return std::chrono::duration_cast<std::chrono::seconds>(
             time.time_since_epoch())
      .count();
}

std::string partHash(File& file, Range range, std::error_code& error)
{
  std::string data(static_cast<size_t>(range.second - range.first), '\0');
  size_t done = 0;
  while (done < data.size()) {
    size_t read = 0;
    const auto size = std::min(hashChunk, data.size() - done);
    error = file.read(range.first + static_cast<int64_t>(done),
                      data.data() + done, size, read);
    if (error) {
      return {};
    }
    if (read == 0) {
      error =
          std::make_error_code(std::errc::io_error); // shorter than expected
      return {};
    }
    done += read;
  }
  return md4Digest(data);
}

} // namespace

Ed2kTask::Ed2kTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
                   Link link)
    : Task(engine, gid, std::move(options)),
      link_(std::move(link)),
      parts_(link_.size),
      ticker_(engine.io())
{
  const auto count = hashSetPartCount(link_.size);
  if (count > 0 && link_.pieceHashes.size() == count &&
      rootHash(link_.pieceHashes) == link_.hash) {
    pieceHashes_ = link_.pieceHashes;
  }
  else {
    link_.pieceHashes.clear();
  }
}

Ed2kTask::~Ed2kTask() = default;

std::string Ed2kTask::defaultPath() const
{
  auto name = options().get("out");
  if (name.empty()) {
    name = safeFileName(link_.name);
  }
  if (name.empty()) {
    name = text::toHex(link_.hash);
  }
  return joinPath(options().get("dir"), name);
}

// --- lifecycle --------------------------------------------------------------

void Ed2kTask::start()
{
  if (phase_ != Phase::Idle) {
    return;
  }
  auto& network = engine().ed2k();
  if (auto* other = network.file(link_.hash); other && other != this) {
    failed(ErrorCode::DuplicateDownload,
           fmt::format("The ED2K file {} is already being downloaded",
                       text::toHex(link_.hash)));
    return;
  }
  network.addBootstrap(options().get("ed2k-server"),
                       options().get("ed2k-server-list"),
                       options().get("ed2k-node-list"));
  if (!strand_) {
    strand_ = engine().disk().strand();
  }
  outcome_ = Outcome::Stop;
  announced_ = false;
  corruptParts_ = 0;
  open();
}

void Ed2kTask::loadProgress()
{
  // Reading progress does not bring the network up.
  const auto record =
      engine().ed2kStarted()
          ? engine().ed2k().store().loadDownload(gid())
          : Store(joinPath(engine().state().directory("ed2k"), "state.db"))
                .loadDownload(gid());
  if (!record || record->fileHash != link_.hash || !fileExists(record->path)) {
    return;
  }
  path_ = record->path;
  sharedBefore_ = record->sharingSeconds;
  for (size_t index = 0; index < record->parts.size() && index < parts_.count();
       ++index) {
    if (record->parts[index]) {
      parts_.verify(index);
    }
  }
  for (const auto& [begin, end] : record->received) {
    parts_.add(begin, end);
  }
}

// Picks the file: our own partial download when the stored record still
// matches it, else a new file. An existing file of the right size is checked
// against the part hashes, so data from elsewhere is reused.
void Ed2kTask::open()
{
  phase_ = Phase::Opening;
  opened_ = false;
  parts_ = Parts(link_.size);
  waitingForHashSet_.clear();
  unchecked_ = false;
  auto& store = engine().ed2k().store();
  const auto record = store.loadDownload(gid());
  bool truncate = true;
  bool check = false;
  path_ = record && !record->path.empty() ? record->path : defaultPath();
  if (record && record->fileHash == link_.hash && fileExists(path_) &&
      fileSize(path_) == link_.size &&
      record->modified == modificationTime(path_)) {
    // Our own file, unchanged since we saved its progress.
    for (size_t index = 0;
         index < record->parts.size() && index < parts_.count(); ++index) {
      if (record->parts[index]) {
        parts_.verify(index);
      }
    }
    for (const auto& [begin, end] : record->received) {
      if (const auto filled = parts_.add(begin, end)) {
        waitingForHashSet_.push_back(*filled); // checked once open
      }
    }
    sharedBefore_ = record->sharingSeconds;
    if (pieceHashes_.empty() && !record->link.empty()) {
      try {
        auto stored = parseLink(record->link);
        if (stored.hash == link_.hash &&
            stored.pieceHashes.size() == hashSetPartCount(link_.size) &&
            rootHash(stored.pieceHashes) == link_.hash) {
          pieceHashes_ = stored.pieceHashes;
          link_.pieceHashes = pieceHashes_;
        }
      }
      catch (const std::exception&) {
      }
    }
    truncate = false;
  }
  else if (fileExists(path_)) {
    const auto existing = fileSize(path_);
    if (record && record->path == path_) {
      // Ours, but changed since: whatever verifies is kept.
      truncate = false;
      check = existing == link_.size;
    }
    else if (existing == link_.size || options().flag("continue")) {
      if (existing > link_.size) {
        failed(ErrorCode::CannotResume,
               fmt::format("{} is larger than the ED2K file", path_));
        phase_ = Phase::Idle;
        return;
      }
      truncate = false;
      check = true;
    }
    else if (!options().flag("allow-overwrite")) {
      auto numbered = options().flag("auto-file-renaming") ? numberedPath(path_)
                                                           : std::nullopt;
      if (!numbered) {
        failed(ErrorCode::FileExists, fmt::format("{} already exists", path_));
        phase_ = Phase::Idle;
        return;
      }
      path_ = std::move(*numbered);
    }
    sharedBefore_ = 0;
  }
  else {
    sharedBefore_ = 0;
  }
  const auto allocation = parseAllocation(options().get("file-allocation"));
  auto file = std::make_shared<File>();
  engine().disk().run(
      *strand_,
      [file, path = path_, truncate, allocation, size = link_.size] {
        auto error = file->open(path, truncate);
        if (error) {
          return error;
        }
        if (allocation == Allocation::Sparse) {
          file->makeSparse();
        }
        if (allocation == Allocation::Preallocate && file->size() < size) {
          return file->allocate(size);
        }
        if (file->size() < size) {
          return file->resize(size);
        }
        return std::error_code();
      },
      [this, self = shared_from_this(), file, check](std::error_code error) {
        file_ = file;
        opened(error, check);
      });
}

void Ed2kTask::opened(std::error_code error, bool check)
{
  if (phase_ != Phase::Opening) {
    quiesce(); // stopped meanwhile
    return;
  }
  if (error) {
    finish(Outcome::Fail, ErrorCode::FileCreate,
           fmt::format("Cannot open {}: {}", path_, error.message()));
    return;
  }
  opened_ = true;
  log::info("{}: ED2K {} -> {}", gidText(gid()), link_.name, path_);
  if (check) {
    checkAll();
    return;
  }
  // Filled parts of a stored download that were not verified yet.
  auto pending = std::exchange(waitingForHashSet_, {});
  run();
  for (const auto index : pending) {
    verifyFilled(index);
  }
}

void Ed2kTask::checkAll()
{
  phase_ = Phase::Checking;
  if (needsHashSet()) {
    // The part hashes of an intact file prove themselves against the file
    // hash. Otherwise they are kept until a source sends the hash set.
    localHashes_.assign(parts_.count(), {});
    for (size_t index = 0; index < parts_.count(); ++index) {
      digestPart(index, [this, index](std::string hash) {
        localHashes_[index] = std::move(hash);
        if (pendingHashes_ == 0 && phase_ == Phase::Checking) {
          auto hashes = localHashes_;
          if (hashes.size() < hashSetPartCount(link_.size)) {
            hashes.push_back(md4Digest("")); // a size on a part boundary
          }
          if (acceptHashSet(std::move(hashes))) {
            for (size_t part = 0; part < parts_.count(); ++part) {
              parts_.verify(part);
            }
          }
          else {
            unchecked_ = true;
          }
          checked();
        }
      });
    }
    return;
  }
  for (size_t index = 0; index < parts_.count(); ++index) {
    hashPart(index, [this, index](bool ok) {
      if (ok) {
        parts_.verify(index);
      }
      if (pendingHashes_ == 0 && phase_ == Phase::Checking) {
        checked();
      }
    });
  }
}

void Ed2kTask::checked()
{
  size_t verified = 0;
  for (size_t index = 0; index < parts_.count(); ++index) {
    verified += parts_.verified(index) ? 1 : 0;
  }
  log::info("{}: {} of {} ED2K parts of {} verified", gidText(gid()), verified,
            parts_.count(), path_);
  run();
}

void Ed2kTask::run()
{
  auto& network = engine().ed2k();
  if (!attached_) {
    network.attach(*this);
    attached_ = true;
  }
  if (parts_.complete()) {
    announced_ = true; // complete before this run: nothing to announce
    downloaded();
    return;
  }
  phase_ = Phase::Downloading;
  for (const auto& source : link_.sources) {
    addSource(source, 0, {}, Origin::Link);
  }
  tick();
}

void Ed2kTask::stop(bool, std::function<void()> done)
{
  stopped_ = std::move(done);
  if (phase_ == Phase::Idle) {
    std::exchange(stopped_, nullptr)();
    return;
  }
  finish(Outcome::Stop);
}

// Leaves the network, lets queued disk work end, then closes the file and
// reports the outcome.
void Ed2kTask::finish(Outcome outcome, ErrorCode code, std::string message)
{
  if (phase_ == Phase::Closing || phase_ == Phase::Idle) {
    if (phase_ == Phase::Idle && outcome == Outcome::Fail) {
      failed(code, std::move(message));
    }
    return;
  }
  outcome_ = outcome;
  failure_ = code;
  failureMessage_ = std::move(message);
  const bool opening = phase_ == Phase::Opening;
  if (phase_ == Phase::Sharing) {
    sharedBefore_ = sharingSeconds();
  }
  phase_ = Phase::Closing;
  ticker_.cancel();
  dropSources();
  if (attached_) {
    engine().ed2k().detach(*this);
    attached_ = false;
  }
  if (!opening) {
    quiesce(); // else opened() continues once the file is open
  }
}

void Ed2kTask::quiesce()
{
  if (phase_ != Phase::Closing || pendingWrites_ > 0 || pendingHashes_ > 0) {
    return;
  }
  auto file = std::exchange(file_, nullptr);
  ++pendingWrites_;
  engine().disk().run(
      *strand_,
      [file] {
        if (file && file->isOpen()) {
          file->sync();
          file->close();
        }
        return true;
      },
      [this, self = shared_from_this()](bool) {
        --pendingWrites_;
        phase_ = Phase::Idle;
        checkpoint();
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
      });
}

// --- verification -----------------------------------------------------------

void Ed2kTask::hashPart(size_t index, std::function<void(bool)> done)
{
  const auto expected = pieceHashes_.empty() ? link_.hash : pieceHashes_[index];
  digestPart(index, [expected, done = std::move(done)](std::string hash) {
    done(hash == expected);
  });
}

void Ed2kTask::digestPart(size_t index, std::function<void(std::string)> done)
{
  const auto range = parts_.part(index);
  ++pendingHashes_;
  engine().disk().run(
      *strand_,
      [file = file_, range] {
        std::error_code error;
        auto hash = file ? partHash(*file, range, error) : std::string();
        return std::make_pair(std::move(hash), error);
      },
      [this, self = shared_from_this(),
       done = std::move(done)](std::pair<std::string, std::error_code> result) {
        --pendingHashes_;
        if (phase_ == Phase::Closing) {
          quiesce();
          return;
        }
        if (result.second) {
          finish(Outcome::Fail, ErrorCode::FileIo,
                 fmt::format("Cannot read {}: {}", path_,
                             result.second.message()));
          return;
        }
        done(std::move(result.first));
      });
}

void Ed2kTask::verifyFilled(size_t index)
{
  if (needsHashSet()) {
    if (std::find(waitingForHashSet_.begin(), waitingForHashSet_.end(),
                  index) == waitingForHashSet_.end()) {
      waitingForHashSet_.push_back(index);
    }
    return;
  }
  hashPart(index, [this, index](bool ok) { partVerified(index, ok); });
}

void Ed2kTask::partVerified(size_t index, bool ok)
{
  if (phase_ != Phase::Downloading || parts_.verified(index)) {
    return;
  }
  if (!ok) {
    ++corruptParts_;
    log::warn("{}: ED2K part {} of {} is corrupt; downloading it again",
              gidText(gid()), index, link_.name);
    parts_.reject(index);
    return;
  }
  parts_.verify(index);
  log::debug("{}: ED2K part {} verified", gidText(gid()), index);
  engine().ed2k().server().filesChanged();
  if (parts_.complete()) {
    downloaded();
  }
}

bool Ed2kTask::needsHashSet() const
{
  return hashSetPartCount(link_.size) > 0 && pieceHashes_.empty();
}

bool Ed2kTask::acceptHashSet(std::vector<std::string> hashes)
{
  if (!needsHashSet()) {
    return true;
  }
  if (hashes.size() != hashSetPartCount(link_.size) ||
      rootHash(hashes) != link_.hash) {
    return false;
  }
  pieceHashes_ = std::move(hashes);
  link_.pieceHashes = pieceHashes_;
  log::debug("{}: ED2K hash set of {} accepted", gidText(gid()), link_.name);
  for (const auto index : std::exchange(waitingForHashSet_, {})) {
    verifyFilled(index);
  }
  if (std::exchange(unchecked_, false)) {
    // An existing file: parts nobody wrote to since they were hashed may
    // already be right.
    for (size_t index = 0; index < parts_.count(); ++index) {
      const auto [begin, end] = parts_.part(index);
      if (!parts_.verified(index) &&
          parts_.received().covered(begin, end) == 0 &&
          localHashes_[index] == pieceHashes_[index]) {
        parts_.verify(index);
      }
    }
    localHashes_.clear();
    if (parts_.complete() && phase_ == Phase::Downloading) {
      downloaded();
    }
  }
  return true;
}

void Ed2kTask::downloaded()
{
  dropSources();
  phase_ = Phase::Sharing;
  sharingSince_ = Clock::now();
  engine().ed2k().server().filesChanged();
  ++pendingWrites_;
  engine().disk().run(
      *strand_,
      [file = file_] { return file ? file->sync() : std::error_code(); },
      [this, self = shared_from_this()](std::error_code) {
        --pendingWrites_;
        quiesce();
      });
  if (!announced_ && completedLength() == link_.size) {
    announced_ = true;
    log::notice("Download complete: {}", path_);
    engine().emit(Event::BtComplete, *this);
  }
  checkpoint();
  tick();
}

// --- sources ----------------------------------------------------------------

void Ed2kTask::addSource(const Endpoint& endpoint, uint32_t clientId,
                         const Endpoint& server, Origin origin)
{
  if (phase_ != Phase::Downloading || sources_.size() >= maxSources ||
      !endpoint.port) {
    return;
  }
  auto& network = engine().ed2k();
  boost::system::error_code error;
  auto address = boost::asio::ip::make_address_v4(endpoint.host, error);
  const bool lowId = clientId != 0 && clientId < 0x1000000;
  if (lowId) {
    address = {};
  }
  else if (error || address.is_unspecified()) {
    log::debug("{}: ignoring ED2K source {}:{}", gidText(gid()), endpoint.host,
               endpoint.port);
    return;
  }
  // Ourselves, as servers and other clients list us too.
  if (!lowId && endpoint.port == network.tcpPort() &&
      (address.is_loopback() || address == network.publicAddress())) {
    return;
  }
  if (lowId && clientId == network.clientId() &&
      endpoint.port == network.tcpPort()) {
    return;
  }
  auto client = network.client(address, endpoint.port, clientId);
  client->learn(endpoint, clientId, server);
  client->setSource(*this, origin);
  if (client->downloadFile() != this ||
      std::find(sources_.begin(), sources_.end(), client) != sources_.end()) {
    return;
  }
  sources_.push_back(std::move(client));
  sourceChanged();
}

void Ed2kTask::dropSources()
{
  for (const auto& source : std::exchange(sources_, {})) {
    if (source->downloadFile() == this) {
      source->dropSource();
    }
  }
}

void Ed2kTask::sourceChanged()
{
  if (scheduled_ || phase_ != Phase::Downloading) {
    return;
  }
  scheduled_ = true;
  boost::asio::post(engine().io(), [this, self = shared_from_this()] {
    scheduled_ = false;
    schedule();
  });
}

// Asks idle sources whose retry time came, a few at a time.
void Ed2kTask::schedule()
{
  if (phase_ != Phase::Downloading) {
    return;
  }
  const auto now = Clock::now();
  size_t asking = 0;
  for (const auto& source : sources_) {
    const auto state = source->download();
    asking += state == Client::Download::Connecting ||
                      state == Client::Download::Asking
                  ? 1
                  : 0;
  }
  for (const auto& source : sources_) {
    if (asking >= maxAsking) {
      break;
    }
    const auto state = source->download();
    const bool due = now >= source->nextAttempt();
    if (due &&
        (state == Client::Download::Idle ||
         state == Client::Download::NoNeededParts ||
         (state == Client::Download::NoFile && source->failures() == 0))) {
      source->askForFile();
      ++asking;
    }
  }
}

void Ed2kTask::tick()
{
  if (phase_ != Phase::Downloading && phase_ != Phase::Sharing) {
    return;
  }
  if (phase_ == Phase::Downloading) {
    // Sources that keep failing make room for new ones.
    std::erase_if(sources_, [this](const std::shared_ptr<Client>& source) {
      const bool dead = source->failures() >= 6 &&
                        source->download() == Client::Download::Idle &&
                        !source->connected();
      if (dead) {
        source->dropSource();
      }
      return dead || source->downloadFile() != this;
    });
    schedule();
  }
  else {
    evaluateSharing();
    if (phase_ != Phase::Sharing) {
      return;
    }
  }
  ticker_.expires_after(1s);
  ticker_.async_wait([this, self = shared_from_this()](
                         const boost::system::error_code& error) {
    if (!error) {
      tick();
    }
  });
}

std::vector<int> Ed2kTask::availability() const
{
  std::vector<int> counts(parts_.count(), 0);
  for (const auto& source : sources_) {
    const auto& status = source->partStatus();
    if (source->download() != Client::Download::Downloading &&
        source->download() != Client::Download::Queued) {
      continue;
    }
    for (size_t index = 0; index < counts.size(); ++index) {
      counts[index] +=
          status.empty() || (index < status.size() && status[index]) ? 1 : 0;
    }
  }
  return counts;
}

std::vector<Range> Ed2kTask::claim(Client& client,
                                   const std::vector<bool>& available,
                                   size_t count)
{
  if (phase_ != Phase::Downloading) {
    return {};
  }
  return parts_.claim(&client, available, count, availability(),
                      options().flag("ed2k-preview-priority"));
}

void Ed2kTask::release(Client& client, const Range& block)
{
  parts_.release(&client, block);
}

void Ed2kTask::releaseAll(Client& client) { parts_.releaseAll(&client); }

bool Ed2kTask::claimed(Client& client, int64_t begin, int64_t end) const
{
  return parts_.claimed(&client, begin, end);
}

bool Ed2kTask::wants(const std::vector<bool>& available) const
{
  return phase_ == Phase::Downloading && parts_.wants(available);
}

// --- data -------------------------------------------------------------------

void Ed2kTask::receive(int64_t begin, std::string data)
{
  if (phase_ != Phase::Downloading || data.empty()) {
    return;
  }
  const auto end =
      std::min<int64_t>(begin + static_cast<int64_t>(data.size()), link_.size);
  if (begin < 0 || begin >= end) {
    return;
  }
  data.resize(static_cast<size_t>(end - begin));
  downloadSpeed_.add(end - begin);
  const auto filled = parts_.add(begin, end);
  ++pendingWrites_;
  engine().disk().run(
      *strand_,
      [file = file_, begin, data = std::move(data)] {
        return file ? file->write(begin, data.data(), data.size())
                    : std::make_error_code(std::errc::bad_file_descriptor);
      },
      [this, self = shared_from_this()](std::error_code error) {
        --pendingWrites_;
        if (error && phase_ != Phase::Closing) {
          finish(Outcome::Fail,
                 error == std::errc::no_space_on_device
                     ? ErrorCode::NotEnoughDiskSpace
                     : ErrorCode::FileIo,
                 fmt::format("Cannot write {}: {}", path_, error.message()));
          return;
        }
        quiesce();
      });
  // Queued behind the write on the same strand, so the hash sees the data.
  if (filled) {
    verifyFilled(*filled);
  }
}

void Ed2kTask::read(int64_t begin, int64_t end, bool compress,
                    std::function<void(std::optional<Block>)> done)
{
  if (!file_ || (phase_ != Phase::Downloading && phase_ != Phase::Sharing)) {
    done(std::nullopt);
    return;
  }
  // Data that did not shrink lately is sent as it is, with a probe now and
  // then; compressing runs on the disk thread.
  compress = compress && (incompressible_ < 4 || ++compressionSkips_ % 32 == 0);
  engine().disk().run(
      *strand_,
      [file = file_, begin, end, compress] {
        Block block;
        block.data.resize(static_cast<size_t>(end - begin));
        size_t done = 0;
        while (done < block.data.size()) {
          size_t read = 0;
          if (file->read(begin + static_cast<int64_t>(done),
                         block.data.data() + done, block.data.size() - done,
                         read) ||
              read == 0) {
            return std::optional<Block>();
          }
          done += read;
        }
        if (compress) {
          deflateBlock(block.compressed, block.data);
        }
        return std::optional<Block>(std::move(block));
      },
      [this, self = shared_from_this(), compress,
       done = std::move(done)](std::optional<Block> block) {
        if (compress && block) {
          incompressible_ = block->compressed.empty() ? incompressible_ + 1 : 0;
        }
        done(std::move(block));
      });
}

void Ed2kTask::noteUploaded(int64_t bytes)
{
  uploaded_ += bytes;
  uploadSpeed_.add(bytes);
}

std::vector<bool> Ed2kTask::sharedParts() const
{
  if (parts_.complete()) {
    return {};
  }
  return parts_.verifiedParts();
}

bool Ed2kTask::sharesAnything() const
{
  if (phase_ != Phase::Downloading && phase_ != Phase::Sharing) {
    return false;
  }
  const auto& verified = parts_.verifiedParts();
  return std::find(verified.begin(), verified.end(), true) != verified.end();
}

// --- sharing ----------------------------------------------------------------

int64_t Ed2kTask::sharingSeconds() const
{
  if (phase_ != Phase::Sharing) {
    return sharedBefore_;
  }
  return sharedBefore_ + std::chrono::duration_cast<std::chrono::seconds>(
                             Clock::now() - sharingSince_)
                             .count();
}

void Ed2kTask::evaluateSharing()
{
  if (phase_ != Phase::Sharing) {
    return;
  }
  bool enough = false;
  if (!options().blank("seed-time")) {
    enough = static_cast<double>(sharingSeconds()) >=
             options().number("seed-time") * 60;
  }
  const auto ratio = options().number("seed-ratio");
  if (ratio > 0 && link_.size > 0) {
    enough = enough || static_cast<double>(uploaded_) >=
                           ratio * static_cast<double>(link_.size);
  }
  if (enough) {
    log::notice("{}: sharing of {} finished", gidText(gid()), link_.name);
    finish(Outcome::Complete);
  }
}

bool Ed2kTask::countsTowardConcurrency() const
{
  return !(phase_ == Phase::Sharing && options().flag("detach-share-only"));
}

void Ed2kTask::optionChanged(std::string_view name)
{
  if (name == "seed-time" || name == "seed-ratio") {
    evaluateSharing();
  }
}

// --- state ------------------------------------------------------------------

void Ed2kTask::checkpoint()
{
  // Only files this task opened have progress worth keeping.
  if (!opened_ || (phase_ != Phase::Downloading && phase_ != Phase::Sharing &&
                   phase_ != Phase::Idle)) {
    return;
  }
  DownloadRecord record;
  record.gid = gid();
  record.fileHash = link_.hash;
  record.size = link_.size;
  record.link = toFileLink(link_);
  record.path = path_;
  record.modified = modificationTime(path_);
  record.complete = parts_.complete();
  record.sharingSeconds = sharingSeconds();
  record.parts = parts_.verifiedParts();
  // Bytes of parts not verified yet; verified parts are stored as such.
  for (const auto& [begin, end] : parts_.received().ranges()) {
    for (auto index = parts_.partOf(begin); index <= parts_.partOf(end - 1);
         ++index) {
      const auto part = parts_.part(index);
      if (!parts_.verified(index)) {
        record.received.emplace_back(std::max(begin, part.first),
                                     std::min(end, part.second));
      }
    }
  }
  engine().ed2k().store().saveDownload(record);
}

void Ed2kTask::discardState()
{
  if (engine().ed2kStarted()) {
    engine().ed2k().store().removeDownload(gid());
  }
}

// --- status -----------------------------------------------------------------

int64_t Ed2kTask::completedLength() const { return parts_.completed(); }

int64_t Ed2kTask::downloadSpeed() const
{
  return phase_ == Phase::Downloading
             ? const_cast<SpeedMeter&>(downloadSpeed_).rate()
             : 0;
}

int64_t Ed2kTask::uploadSpeed() const
{
  return phase_ == Phase::Downloading || phase_ == Phase::Sharing
             ? uploadSpeed_.rate()
             : 0;
}

int Ed2kTask::connections() const
{
  int count = 0;
  for (const auto& source : sources_) {
    count += source->connected() ? 1 : 0;
  }
  return count;
}

std::vector<TaskFile> Ed2kTask::files() const
{
  TaskFile file;
  file.path = path_.empty() ? defaultPath() : path_;
  file.length = link_.size;
  file.completed = completedLength();
  file.uris.push_back({toFileLink(link_), phase_ != Phase::Idle});
  return {file};
}

std::vector<std::string> Ed2kTask::sessionUris() const
{
  return {toFileLink(link_)};
}

void Ed2kTask::status(json::object& out, const StatusKeys& keys) const
{
  Task::status(out, keys);
  const auto put = [&](std::string_view key, json::value value) {
    if (keys.want(key)) {
      out[key] = std::move(value);
    }
  };
  put("seeder", json::string(flag(phase_ == Phase::Sharing)));
  put("pieceLength", json::string(std::to_string(PIECE_LENGTH)));
  put("numPieces", json::string(std::to_string(parts_.count())));
  if (keys.want("bitfield")) {
    std::string bits((parts_.count() + 7) / 8, '\0');
    for (size_t index = 0; index < parts_.count(); ++index) {
      if (parts_.verified(index)) {
        bits[index / 8] =
            static_cast<char>(bits[index / 8] | (0x80 >> (index % 8)));
      }
    }
    out["bitfield"] = text::toHex(bits);
  }
  if (phase_ == Phase::Checking) {
    put("verifyIntegrityPending", json::string("true"));
  }
  if (!keys.want("ed2k")) {
    return;
  }
  json::object ed2k;
  ed2k["ed2kLink"] = toFileLink(link_);
  ed2k["hash"] = text::toHex(link_.hash);
  ed2k["name"] = link_.name;
  ed2k["length"] = std::to_string(link_.size);
  ed2k["completedLength"] = std::to_string(completedLength());
  ed2k["partHashCount"] = std::to_string(pieceHashes_.size());
  if (!link_.aichHash.empty()) {
    ed2k["aichRoot"] = text::toHex(link_.aichHash);
  }
  size_t queued = 0;
  size_t accepted = 0;
  size_t dead = 0;
  size_t lowId = 0;
  size_t callback = 0;
  for (const auto& source : sources_) {
    const auto state = source->download();
    queued += state == Client::Download::Queued ? 1 : 0;
    accepted += state == Client::Download::Downloading ? 1 : 0;
    dead += state == Client::Download::NoFile || source->failures() > 0 ? 1 : 0;
    lowId += source->lowId() ? 1 : 0;
    callback +=
        source->lowId() && state == Client::Download::Connecting ? 1 : 0;
  }
  ed2k["peerCount"] = std::to_string(sources_.size());
  ed2k["queuedPeerCount"] = std::to_string(queued);
  ed2k["acceptedPeerCount"] = std::to_string(accepted);
  ed2k["deadPeerCount"] = std::to_string(dead);
  ed2k["lowIdPeerCount"] = std::to_string(lowId);
  ed2k["callbackWaitingPeerCount"] = std::to_string(callback);
  ed2k["sharingTime"] = std::to_string(sharingSeconds());
  ed2k["corruptPartCount"] = std::to_string(corruptParts_);
  if (engine().ed2kStarted()) {
    engine().ed2k().describe(ed2k);
  }
  out["ed2k"] = std::move(ed2k);
}

} // namespace aria2::ed2k
