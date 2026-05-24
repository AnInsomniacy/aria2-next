/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#include "LibtorrentCommand.h"

#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "DownloadFailureException.h"
#include "DiskAdaptor.h"
#include "FileEntry.h"
#include "LibtorrentAttribute.h"
#include "LibtorrentSession.h"
#include "LibtorrentSeedPolicy.h"
#include "LogFactory.h"
#include "Notifier.h"
#include "Option.h"
#include "PieceStorage.h"
#include "RequestGroup.h"
#include "RecoverableException.h"
#include "SingletonHolder.h"
#include "error_code.h"
#include "fmt.h"
#include "prefs.h"
#include "util.h"

#include <algorithm>
#include <cstring>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/announce_entry.hpp>
#include <libtorrent/info_hash.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/write_resume_data.hpp>

namespace aria2 {

namespace lt = libtorrent;

namespace {
std::string getV1InfoHash(const lt::add_torrent_params& params)
{
  if (params.info_hashes.has_v1()) {
    return params.info_hashes.v1.to_string();
  }
  if (params.ti) {
    auto hashes = params.ti->info_hashes();
    if (hashes.has_v1()) {
      return hashes.v1.to_string();
    }
  }
  return {};
}

bool matchesExpectedInfoHash(const lt::add_torrent_params& params,
                             const std::string& expected)
{
  auto actual = getV1InfoHash(params);
  return expected.empty() || (!actual.empty() && actual == expected);
}

bool matchesExpectedInfoHashForSave(const lt::add_torrent_params& params,
                                    const std::string& expected)
{
  auto actual = getV1InfoHash(params);
  return expected.empty() || actual.empty() || actual == expected;
}

lt::add_torrent_params
createAddTorrentParams(LibtorrentAttribute* attrs, const Option* option)
{
  lt::add_torrent_params params;
  lt::error_code ec;

  switch (attrs->sourceType) {
  case LibtorrentAttribute::SourceType::TORRENT_FILE:
    params = lt::load_torrent_file(attrs->sourceUri);
    break;
  case LibtorrentAttribute::SourceType::TORRENT_DATA:
    params = lt::load_torrent_buffer(
        lt::span<char const>(attrs->torrentData.data(),
                             static_cast<int>(attrs->torrentData.size())));
    break;
  case LibtorrentAttribute::SourceType::MAGNET:
    params = lt::parse_magnet_uri(attrs->sourceUri, ec);
    if (ec) {
      throw DOWNLOAD_FAILURE_EXCEPTION2(ec.message(),
                                        error_code::MAGNET_PARSE_ERROR);
    }
    break;
  }

  if (attrs->hasResumeData()) {
    lt::error_code ec;
    auto resumeParams = lt::read_resume_data(
        lt::span<char const>(attrs->getResumeData().data(),
                             static_cast<int>(attrs->getResumeData().size())),
        ec);
    if (!ec && matchesExpectedInfoHash(resumeParams, attrs->infoHash)) {
      resumeParams.save_path = option->get(PREF_DIR);
      resumeParams.url_seeds.assign(attrs->webSeedUris.begin(),
                                    attrs->webSeedUris.end());
      resumeParams.trackers.assign(attrs->trackerUris.begin(),
                                   attrs->trackerUris.end());
      resumeParams.tracker_tiers.assign(attrs->trackerTiers.begin(),
                                        attrs->trackerTiers.end());
      if (!attrs->trackerUris.empty()) {
        resumeParams.flags |= lt::torrent_flags::override_trackers;
      }
      params = std::move(resumeParams);
    }
    else if (!ec) {
      A2_LOG_WARN(
          "Ignoring libtorrent resume data with mismatched infoHash.");
      attrs->setResumeData("");
    }
    else {
      A2_LOG_WARN(fmt("Ignoring invalid libtorrent resume data: %s",
                      ec.message().c_str()));
      attrs->setResumeData("");
    }
  }

  params.save_path = option->get(PREF_DIR);
  params.url_seeds.assign(attrs->webSeedUris.begin(), attrs->webSeedUris.end());
  params.trackers.assign(attrs->trackerUris.begin(), attrs->trackerUris.end());
  params.tracker_tiers.assign(attrs->trackerTiers.begin(),
                              attrs->trackerTiers.end());
  params.file_priorities.assign(attrs->filePriorities.begin(),
                                attrs->filePriorities.end());
  params.flags |= lt::torrent_flags::duplicate_is_error;
  if (!option->getAsBool(PREF_ENABLE_DHT)) {
    params.flags |= lt::torrent_flags::disable_dht;
  }
  if (!option->getAsBool(PREF_BT_ENABLE_LPD)) {
    params.flags |= lt::torrent_flags::disable_lsd;
  }
  if (!option->getAsBool(PREF_ENABLE_PEER_EXCHANGE)) {
    params.flags |= lt::torrent_flags::disable_pex;
  }
  if (!attrs->trackerUris.empty()) {
    params.flags |= lt::torrent_flags::override_trackers;
  }
  if (option->defined(PREF_DHT_ENTRY_POINT_HOST) &&
      option->defined(PREF_DHT_ENTRY_POINT_PORT)) {
    params.dht_nodes.push_back(
        {option->get(PREF_DHT_ENTRY_POINT_HOST),
         option->getAsInt(PREF_DHT_ENTRY_POINT_PORT)});
  }
  params.flags &= ~lt::torrent_flags::paused;
  params.flags &= ~lt::torrent_flags::auto_managed;
  if (attrs->sourceType == LibtorrentAttribute::SourceType::MAGNET &&
      attrs->pauseAfterMetadata && !attrs->metadataPauseApplied) {
    params.flags |= lt::torrent_flags::upload_mode;
    params.flags |= lt::torrent_flags::default_dont_download;
  }
  return params;
}

std::vector<std::vector<std::string>>
createAnnounceList(const std::vector<lt::announce_entry>& trackers)
{
  std::vector<std::vector<std::string>> result;
  for (const auto& tracker : trackers) {
    if (tracker.url.empty()) {
      continue;
    }
    if (result.size() <= tracker.tier) {
      result.resize(static_cast<size_t>(tracker.tier) + 1);
    }
    result[tracker.tier].push_back(tracker.url);
  }
  return result;
}

void storeAnnounceList(LibtorrentAttribute* attrs,
                       const std::vector<std::vector<std::string>>& tiers)
{
  attrs->trackerUris.clear();
  attrs->trackerTiers.clear();
  for (size_t tier = 0; tier < tiers.size(); ++tier) {
    for (const auto& uri : tiers[tier]) {
      attrs->trackerUris.push_back(uri);
      attrs->trackerTiers.push_back(static_cast<int>(tier));
    }
  }
}

std::string peerAddress(const lt::peer_info& peer)
{
  auto address = peer.ip.address();
  if (address.is_v4()) {
    return address.to_v4().to_string();
  }
  if (address.is_v6()) {
    return address.to_v6().to_string();
  }
  return {};
}

std::string peerBitfield(const lt::peer_info& peer)
{
  std::string bitfield;
  if (peer.pieces.empty()) {
    return bitfield;
  }
  bitfield.resize(peer.pieces.num_bytes());
  std::memcpy(&bitfield[0], peer.pieces.data(), bitfield.size());
  return bitfield;
}

std::vector<LibtorrentAttribute::Peer>
createPeerList(const std::vector<lt::peer_info>& peers)
{
  std::vector<LibtorrentAttribute::Peer> result;
  result.reserve(peers.size());
  for (const auto& peer : peers) {
    LibtorrentAttribute::Peer entry;
    auto id = peer.pid.to_string();
    entry.peerId.assign(id.begin(), id.end());
    entry.ip = peerAddress(peer);
    entry.port = peer.ip.port();
    entry.bitfield = peerBitfield(peer);
    entry.downloadSpeed = peer.payload_down_speed;
    entry.uploadSpeed = peer.payload_up_speed;
    entry.amChoking = bool(peer.flags & lt::peer_info::choked);
    entry.peerChoking = bool(peer.flags & lt::peer_info::remote_choked);
    entry.seeder = bool(peer.flags & lt::peer_info::seed);
    result.push_back(std::move(entry));
  }
  return result;
}

bool isPartialSelection(const lt::torrent_status& status)
{
  return status.has_metadata && status.total_wanted < status.total;
}

bool isSharing(const lt::torrent_status& status)
{
  return status.is_seeding || (status.is_finished && isPartialSelection(status));
}

std::string bitfieldBytes(const lt::typed_bitfield<lt::piece_index_t>& pieces)
{
  std::string bitfield;
  if (pieces.empty()) {
    return bitfield;
  }
  bitfield.resize(pieces.num_bytes());
  std::memcpy(&bitfield[0], pieces.data(), bitfield.size());
  return bitfield;
}

int64_t calculateWantedLength(const lt::add_torrent_params& params)
{
  if (!params.ti) {
    return 0;
  }
  const auto& files = params.ti->files();
  int64_t total = 0;
  for (auto i = 0; i < files.num_files(); ++i) {
    if (!params.file_priorities.empty() &&
        static_cast<size_t>(i) < params.file_priorities.size() &&
        params.file_priorities[static_cast<size_t>(i)] == lt::dont_download) {
      continue;
    }
    total += files.file_size(lt::file_index_t(i));
  }
  return total;
}

int64_t calculateCompletedLength(const lt::add_torrent_params& params)
{
  if (!params.ti || params.have_pieces.empty()) {
    return 0;
  }
  const auto& files = params.ti->files();
  const auto pieceLength = params.ti->piece_length();
  int64_t total = 0;
  for (auto i = 0; i < files.num_files(); ++i) {
    if (!params.file_priorities.empty() &&
        static_cast<size_t>(i) < params.file_priorities.size() &&
        params.file_priorities[static_cast<size_t>(i)] == lt::dont_download) {
      continue;
    }
    const auto file = lt::file_index_t(i);
    const auto fileBegin = files.file_offset(file);
    const auto fileEnd = fileBegin + files.file_size(file);
    if (fileEnd <= fileBegin || pieceLength <= 0) {
      continue;
    }
    const auto firstPiece = static_cast<int>(fileBegin / pieceLength);
    const auto lastPiece = static_cast<int>((fileEnd - 1) / pieceLength);
    for (auto pieceIndex = firstPiece; pieceIndex <= lastPiece; ++pieceIndex) {
      auto piece = lt::piece_index_t(pieceIndex);
      if (params.have_pieces[piece]) {
        const auto pieceBegin = static_cast<int64_t>(pieceIndex) * pieceLength;
        const auto pieceEnd = pieceBegin + params.ti->piece_size(piece);
        total += std::max<int64_t>(
            0, std::min(fileEnd, pieceEnd) - std::max(fileBegin, pieceBegin));
      }
    }
  }
  auto wanted = calculateWantedLength(params);
  return std::min(total, wanted);
}

std::vector<lt::download_priority_t>
createFilePriorities(const std::vector<int>& priorities)
{
  std::vector<lt::download_priority_t> result;
  result.reserve(priorities.size());
  for (auto priority : priorities) {
    result.push_back(static_cast<lt::download_priority_t>(priority));
  }
  return result;
}

std::vector<int> createSelectedFilePriorities(const std::string& selectedFiles,
                                              int numFiles)
{
  if (selectedFiles.empty() || numFiles <= 1) {
    return {};
  }

  auto selected = util::parseIntSegments(selectedFiles);
  selected.normalize();
  if (!selected.hasNext()) {
    return {};
  }

  std::vector<int> priorities(static_cast<size_t>(numFiles), 0);
  while (selected.hasNext()) {
    const auto index = selected.next();
    if (index >= 1 && index <= numFiles) {
      priorities[static_cast<size_t>(index - 1)] = 4;
    }
  }
  return priorities;
}

void refreshFileShape(RequestGroup* requestGroup, const lt::torrent_status& st)
{
  if (!st.has_metadata || !st.handle.is_valid()) {
    return;
  }

  auto attrs = getLibtorrentAttrs(requestGroup->getDownloadContext());
  auto& dctx = requestGroup->getDownloadContext();
  if (dctx->knowsTotalLength() && dctx->getTotalLength() == st.total_wanted) {
    return;
  }

  auto ti = st.handle.torrent_file();
  if (!ti) {
    return;
  }
  if (!attrs->selectedFiles.empty() && attrs->filePriorities.empty()) {
    attrs->filePriorities =
        createSelectedFilePriorities(attrs->selectedFiles,
                                     ti->files().num_files());
  }
  if (!attrs->filePriorities.empty() && !attrs->filePrioritiesApplied) {
    st.handle.prioritize_files(createFilePriorities(attrs->filePriorities));
    attrs->filePrioritiesApplied = true;
  }
  const auto& files = ti->files();
  std::vector<std::shared_ptr<FileEntry>> entries;
  entries.reserve(static_cast<size_t>(files.num_files()));
  for (auto i = 0; i < files.num_files(); ++i) {
    auto path = util::applyDir(requestGroup->getOption()->get(PREF_DIR),
                               files.file_path(i));
    entries.push_back(std::make_shared<FileEntry>(path, files.file_size(i),
                                                  files.file_offset(i)));
  }
  if (entries.empty()) {
    return;
  }

  dctx->setFileEntries(entries.begin(), entries.end());
  dctx->setBasePath(entries.front()->getPath());
  dctx->setPieceLength(ti->piece_length());
  dctx->markTotalLengthIsKnown();

  requestGroup->setFileAllocationEnabled(false);
  requestGroup->dropPieceStorage();
  requestGroup->initPieceStorage();
  requestGroup->getPieceStorage()->getDiskAdaptor()->openFile();
}

bool shouldApplyMetadataPause(const LibtorrentAttribute* attrs,
                              const lt::torrent_status& status)
{
  return attrs->sourceType == LibtorrentAttribute::SourceType::MAGNET &&
         attrs->pauseAfterMetadata && !attrs->metadataPauseApplied &&
         status.has_metadata;
}
} // namespace

LibtorrentCommand::LibtorrentCommand(cuid_t cuid, RequestGroup* requestGroup,
                                     DownloadEngine* engine)
    : TimeBasedCommand(cuid, engine, 1_s),
      requestGroup_(requestGroup),
      session_(&engine->getLibtorrentSession()),
      completedLength_(0),
      uploadedLength_(0),
      resumeDataRequestTimer_(Timer::zero()),
      sharingTimer_(Timer::zero()),
      resumeDataRequested_(false),
      torrentAdded_(false),
      btCompleteNotified_(false),
      sharingTimerStarted_(false),
      resumeDataSynced_(false)
{
  setStatusActive();
  requestGroup_->increaseNumCommand();
}

LibtorrentCommand::~LibtorrentCommand()
{
  if (torrentAdded_) {
    session_->removeTorrent(requestGroup_->getGID());
  }
  requestGroup_->decreaseNumCommand();
}

void LibtorrentCommand::preProcess()
{
  if (requestGroup_->isHaltRequested() || getDownloadEngine()->isHaltRequested()) {
    if (handle_.is_valid()) {
      handle_.pause(lt::torrent_handle::graceful_pause);
      requestResumeData();
    }
    pollAlerts();
    if (resumeDataRequested_ &&
        resumeDataRequestTimer_.difference() < std::chrono::seconds(3)) {
      return;
    }
    syncResumeDataOnExit();
    enableExit();
    return;
  }

  if (!torrentAdded_) {
    addTorrent();
  }
  pollAlerts();
  updateStatus();
}

void LibtorrentCommand::process()
{
  if (handle_.is_valid()) {
    getDownloadEngine()->getLibtorrentSession().nativeSession().
        post_torrent_updates();
  }
}

void LibtorrentCommand::addTorrent()
{
  auto attrs = getLibtorrentAttrs(requestGroup_->getDownloadContext());
  auto params = createAddTorrentParams(attrs, requestGroup_->getOption().get());
  if (attrs->hasResumeData()) {
    storeResumeStatus(params);
  }
  try {
    handle_ = session_->addTorrent(requestGroup_->getGID(), std::move(params));
  }
  catch (const RecoverableException& ex) {
    failDownload(ex.getErrorCode(), ex.what());
    return;
  }
  if (!attrs->filePriorities.empty()) {
    attrs->filePrioritiesApplied = true;
  }
  auto option = requestGroup_->getOption();
  handle_.set_download_limit(option->getAsInt(PREF_MAX_DOWNLOAD_LIMIT));
  handle_.set_upload_limit(option->getAsInt(PREF_MAX_UPLOAD_LIMIT));
  handle_.set_max_connections(option->getAsInt(PREF_BT_MAX_PEERS) == 0
                                  ? -1
                                  : option->getAsInt(PREF_BT_MAX_PEERS));
  torrentAdded_ = true;
  A2_LOG_INFO(fmt("GID#%s - Added BitTorrent download to libtorrent.",
                  requestGroup_->getGroupId()->toHex().c_str()));
}

void LibtorrentCommand::pollAlerts()
{
  std::vector<LibtorrentEvent> events;
  session_->pollEvents(requestGroup_->getGID(), events);
  for (const auto& event : events) {
    switch (event.type) {
    case LibtorrentEvent::Type::AddTorrentError:
    case LibtorrentEvent::Type::TorrentError:
    case LibtorrentEvent::Type::FileError:
      failDownload(event.errorCode, event.message);
      break;
    case LibtorrentEvent::Type::SaveResumeData:
      storeResumeData(event.resumeParams);
      break;
    case LibtorrentEvent::Type::SaveResumeDataFailed:
      resumeDataRequested_ = false;
      A2_LOG_WARN(fmt("Failed to save libtorrent resume data: %s",
                      event.message.c_str()));
      break;
    case LibtorrentEvent::Type::MetadataReceived:
      A2_LOG_INFO(fmt("GID#%s - BitTorrent metadata received by libtorrent.",
                      requestGroup_->getGroupId()->toHex().c_str()));
      break;
    }
  }
}

void LibtorrentCommand::updateStatus()
{
  if (!handle_.is_valid()) {
    return;
  }

  auto status = handle_.status(lt::torrent_handle::query_save_path);
  status = handle_.status(lt::torrent_handle::query_save_path |
                          lt::torrent_handle::query_torrent_file |
                          lt::torrent_handle::query_name |
                          lt::torrent_handle::query_pieces);
  refreshFileShape(requestGroup_, status);

  auto attrs = getLibtorrentAttrs(requestGroup_->getDownloadContext());
  if (shouldApplyMetadataPause(attrs, status)) {
    handle_.set_flags(lt::torrent_flags::upload_mode,
                      lt::torrent_flags::upload_mode);
    handle_.pause(lt::torrent_handle::graceful_pause);
    attrs->metadataPauseApplied = true;
    requestGroup_->setPauseRequested(true);
    attrs->status.hasStatus = true;
    attrs->status.complete = false;
    attrs->status.checking = false;
    attrs->status.seeding = false;
    attrs->status.sharing = false;
    attrs->status.hasMetadata = true;
    attrs->status.totalLength = status.total_wanted;
    attrs->status.completedLength = status.total_wanted_done;
    attrs->status.uploadedLength = status.all_time_upload;
    attrs->status.downloadSpeed = 0;
    attrs->status.uploadSpeed = status.upload_payload_rate;
    attrs->status.connections = status.num_peers;
    attrs->status.seeders = status.num_seeds;
    attrs->status.name = status.name;
    if (!status.info_hashes.v1.is_all_zeros()) {
      auto hash = status.info_hashes.v1.to_string();
      attrs->status.infoHash.assign(hash.begin(), hash.end());
    }
    syncResumeData();
    enableExit();
    return;
  }

  attrs->status.hasStatus = true;
  attrs->status.complete = false;
  attrs->status.checking =
      status.state == lt::torrent_status::checking_resume_data ||
      status.state == lt::torrent_status::checking_files;
  attrs->status.seeding = status.is_seeding;
  attrs->status.sharing = isSharing(status);
  attrs->status.hasMetadata = status.has_metadata;
  attrs->status.totalLength = status.total_wanted;
  attrs->status.completedLength = status.total_wanted_done;
  attrs->status.uploadedLength = status.all_time_upload;
  attrs->status.downloadSpeed = status.download_payload_rate;
  attrs->status.uploadSpeed = status.upload_payload_rate;
  attrs->status.connections = status.num_peers;
  attrs->status.seeders = status.num_seeds;
  attrs->status.name = status.name;
  if (attrs->status.sharing && !sharingTimerStarted_) {
    sharingTimer_.reset();
    sharingTimerStarted_ = true;
  }
  if (!status.info_hashes.v1.is_all_zeros()) {
    auto hash = status.info_hashes.v1.to_string();
    attrs->status.infoHash.assign(hash.begin(), hash.end());
  }
  if (!status.pieces.empty()) {
    attrs->status.bitfield.assign(status.pieces.data(),
                                  status.pieces.num_bytes());
  }
  try {
    storeAnnounceList(attrs, createAnnounceList(handle_.trackers()));
    std::vector<lt::peer_info> peers;
    handle_.get_peer_info(peers);
    attrs->peers = createPeerList(peers);
  }
  catch (const std::exception& ex) {
    A2_LOG_DEBUG(fmt("Failed to refresh libtorrent RPC state: %s", ex.what()));
  }

  if (status.total_wanted_done > completedLength_) {
    requestGroup_->getDownloadContext()->updateDownload(
        static_cast<size_t>(status.total_wanted_done - completedLength_));
    completedLength_ = status.total_wanted_done;
  }
  if (status.total_upload > uploadedLength_) {
    requestGroup_->getDownloadContext()->updateUploadLength(
        static_cast<size_t>(status.total_upload - uploadedLength_));
    uploadedLength_ = status.total_upload;
  }

  if (requestGroup_->getPieceStorage()) {
    requestGroup_->getPieceStorage()->markPiecesDone(status.total_wanted_done);
  }

  if (status.is_finished && !status.is_seeding && isPartialSelection(status)) {
    reportBtDownloadComplete();
    requestResumeData();
  }
  else if (status.is_seeding) {
    reportBtDownloadComplete();
    requestGroup_->enableSeedOnly();
    requestResumeData();
  }
  if (isSharing(status) && !resumeDataSynced_) {
    syncResumeData();
  }
  if (isSharing(status) &&
      shouldStopLibtorrentSharing(requestGroup_->getOption().get(),
                                  status.total_wanted,
                                  status.all_time_upload,
                                  std::chrono::duration_cast<
                                      std::chrono::seconds>(
                                      sharingTimer_.difference()))) {
    attrs->status.complete = true;
    attrs->status.sharing = false;
    finishDownload();
  }
}

void LibtorrentCommand::storeResumeStatus(const lt::add_torrent_params& params)
{
  auto attrs = getLibtorrentAttrs(requestGroup_->getDownloadContext());
  attrs->status.hasStatus = true;
  attrs->status.checking = true;
  auto& status = attrs->resumeStatus;
  status.hasStatus = true;
  status.hasMetadata = bool(params.ti);
  status.totalLength = calculateWantedLength(params);
  status.completedLength = calculateCompletedLength(params);
  status.bitfield = bitfieldBytes(params.have_pieces);
  status.infoHash = getV1InfoHash(params);
  if (params.ti) {
    status.name = params.ti->name();
  }
}

void LibtorrentCommand::requestResumeData()
{
  if (!handle_.is_valid() || resumeDataRequested_) {
    return;
  }
  try {
    handle_.save_resume_data();
    resumeDataRequested_ = true;
    resumeDataRequestTimer_.reset();
  }
  catch (const std::exception& ex) {
    A2_LOG_WARN(fmt("Failed to request libtorrent resume data: %s", ex.what()));
  }
}

void LibtorrentCommand::syncResumeDataOnExit()
{
  if (!handle_.is_valid() || !resumeDataRequested_) {
    return;
  }
  syncResumeData();
}

void LibtorrentCommand::syncResumeData()
{
  if (!handle_.is_valid()) {
    return;
  }
  try {
    storeResumeData(handle_.get_resume_data());
    resumeDataSynced_ = true;
  }
  catch (const std::exception& ex) {
    A2_LOG_WARN(fmt("Failed to synchronously save libtorrent resume data: %s",
                    ex.what()));
  }
}

void LibtorrentCommand::storeResumeData(const lt::add_torrent_params& params)
{
  auto attrs = getLibtorrentAttrs(requestGroup_->getDownloadContext());
  if (!matchesExpectedInfoHashForSave(params, attrs->infoHash)) {
    A2_LOG_WARN(
        "Ignoring libtorrent resume data with mismatched infoHash.");
    resumeDataRequested_ = false;
    return;
  }
  auto data = lt::write_resume_data_buf(params);
  attrs->setResumeData(std::string(data.begin(), data.end()));
  resumeDataRequested_ = false;
  try {
    requestGroup_->saveControlFile();
  }
  catch (const RecoverableException& ex) {
    A2_LOG_ERROR_EX(EX_EXCEPTION_CAUGHT, ex);
  }
}

void LibtorrentCommand::reportBtDownloadComplete()
{
  if (btCompleteNotified_) {
    return;
  }
  auto option = requestGroup_->getOption();
  util::executeHookByOptName(requestGroup_, option.get(),
                             PREF_ON_BT_DOWNLOAD_COMPLETE);
  if (SingletonHolder<Notifier>::instance()) {
    SingletonHolder<Notifier>::instance()->notifyDownloadEvent(
        EVENT_ON_BT_DOWNLOAD_COMPLETE, requestGroup_);
  }
  btCompleteNotified_ = true;
}

void LibtorrentCommand::finishDownload()
{
  if (requestGroup_->getPieceStorage()) {
    requestGroup_->getPieceStorage()->markAllPiecesDone();
  }
  requestGroup_->getDownloadContext()->resetDownloadStopTime();
  enableExit();
}

void LibtorrentCommand::failDownload(error_code::Value code,
                                     const std::string& message)
{
  requestGroup_->setLastErrorCode(code, message.c_str());
  requestGroup_->setHaltRequested(true);
  A2_LOG_ERROR(fmt("GID#%s - libtorrent error: %s",
                   requestGroup_->getGroupId()->toHex().c_str(),
                   message.c_str()));
}

} // namespace aria2
