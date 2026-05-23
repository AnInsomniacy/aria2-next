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
#include "SingletonHolder.h"
#include "error_code.h"
#include "fmt.h"
#include "prefs.h"
#include "util.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/load_torrent.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/write_resume_data.hpp>

namespace aria2 {

namespace lt = libtorrent;

namespace {
lt::add_torrent_params
createAddTorrentParams(const LibtorrentAttribute* attrs, const Option* option)
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
    if (!ec) {
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
    else {
      A2_LOG_WARN(fmt("Ignoring invalid libtorrent resume data: %s",
                      ec.message().c_str()));
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
  return params;
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
  dctx->markTotalLengthIsKnown();

  requestGroup->setFileAllocationEnabled(false);
  requestGroup->dropPieceStorage();
  requestGroup->initPieceStorage();
  requestGroup->getPieceStorage()->getDiskAdaptor()->openFile();
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
      resumeDataRequested_(false),
      torrentAdded_(false),
      btCompleteNotified_(false)
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
  handle_ = session_->addTorrent(requestGroup_->getGID(), std::move(params));
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
  std::vector<lt::alert*> alerts;
  session_->pollAlerts(alerts);
  for (auto alert : alerts) {
    if (auto addAlert = lt::alert_cast<lt::add_torrent_alert>(alert)) {
      if (addAlert->error) {
        failDownload(addAlert->error.message());
      }
    }
    else if (auto errAlert = lt::alert_cast<lt::torrent_error_alert>(alert)) {
      failDownload(errAlert->error.message());
    }
    else if (auto fileAlert = lt::alert_cast<lt::file_error_alert>(alert)) {
      failDownload(fileAlert->error.message());
    }
    else if (auto resumeAlert =
                 lt::alert_cast<lt::save_resume_data_alert>(alert)) {
      storeResumeData(resumeAlert->params);
    }
    else if (auto resumeError =
                 lt::alert_cast<lt::save_resume_data_failed_alert>(alert)) {
      resumeDataRequested_ = false;
      A2_LOG_WARN(fmt("Failed to save libtorrent resume data: %s",
                      resumeError->error.message().c_str()));
    }
    else if (lt::alert_cast<lt::metadata_received_alert>(alert)) {
      A2_LOG_INFO(fmt("GID#%s - BitTorrent metadata received by libtorrent.",
                      requestGroup_->getGroupId()->toHex().c_str()));
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
  attrs->status.hasStatus = true;
  attrs->status.complete = false;
  attrs->status.seeding = status.is_seeding;
  attrs->status.hasMetadata = status.has_metadata;
  attrs->status.totalLength = status.total_wanted;
  attrs->status.completedLength = status.total_wanted_done;
  attrs->status.uploadedLength = status.all_time_upload;
  attrs->status.downloadSpeed = status.download_payload_rate;
  attrs->status.uploadSpeed = status.upload_payload_rate;
  attrs->status.connections = status.num_peers;
  attrs->status.name = status.name;
  if (!status.info_hashes.v1.is_all_zeros()) {
    auto hash = status.info_hashes.v1.to_string();
    attrs->status.infoHash.assign(hash.begin(), hash.end());
  }
  if (!status.pieces.empty()) {
    attrs->status.bitfield.assign(status.pieces.data(),
                                  status.pieces.num_bytes());
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

  if (status.is_finished && !status.is_seeding) {
    reportBtDownloadComplete();
    attrs->status.complete = true;
    requestResumeData();
    finishDownload();
  }
  else if (status.is_seeding) {
    reportBtDownloadComplete();
    requestGroup_->enableSeedOnly();
    requestResumeData();
    if (!hasLibtorrentSeedLimit(requestGroup_->getOption().get()) ||
        shouldStopLibtorrentSeeding(requestGroup_->getOption().get(),
                                    status.total_wanted,
                                    status.all_time_upload,
                                    status.seeding_duration)) {
      attrs->status.complete = true;
      finishDownload();
    }
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

void LibtorrentCommand::storeResumeData(const lt::add_torrent_params& params)
{
  auto attrs = getLibtorrentAttrs(requestGroup_->getDownloadContext());
  auto data = lt::write_resume_data_buf(params);
  attrs->setResumeData(std::string(data.begin(), data.end()));
  resumeDataRequested_ = false;
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

void LibtorrentCommand::failDownload(const std::string& message)
{
  requestGroup_->setLastErrorCode(error_code::UNKNOWN_ERROR, message.c_str());
  requestGroup_->setHaltRequested(true);
  A2_LOG_ERROR(fmt("GID#%s - libtorrent error: %s",
                   requestGroup_->getGroupId()->toHex().c_str(),
                   message.c_str()));
}

} // namespace aria2
