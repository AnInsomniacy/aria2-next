/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 The aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#include "DownloadContext.h"
#include "File.h"
#include "Option.h"
#include "RequestGroup.h"
#include <libtorrent/session.hpp>
#include "BtDownload.h"
#include "BtDownloadImpl.h"
#include "BtSession.h"
#include "BtSnapshot.h"
#include "Log.h"
#include "fmt.h"
#include "prefs.h"
#include "support/Text.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <libtorrent/error_code.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/web_seed_entry.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include "bittorrent/BtSessionInternal.h"

namespace aria2 {
namespace bt_session {
} // namespace bt_session
namespace {
lt::web_seed_entry::headers_t webSeedHeaders(const Option* option)
{
  lt::web_seed_entry::headers_t headers;
  std::istringstream lines(option->get(PREF_HEADER));
  std::string line;
  while (std::getline(lines, line)) {
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    auto name = util::strip(line.substr(0, separator));
    auto value = line.substr(separator + 1);
    const auto firstValue = value.find_first_not_of(" \t");
    value = firstValue == std::string::npos ? std::string()
                                             : value.substr(firstValue);
    if (!name.empty()) {
      headers.emplace_back(std::move(name), std::move(value));
    }
  }
  return headers;
}
} // namespace
using namespace bt_session;

void BtSession::prepareFreshAdd(BtDownload* download)
{
  if (!download || !download->group() || !download->impl_->params.ti) {
    return;
  }

  auto& params = download->impl_->params;
  params.file_priorities = makeFilePriorities(download->group());
  const auto priorityCount =
      std::min(params.file_priorities.size(), download->snapshot_.files.size());
  for (size_t index = 0; index < priorityCount; ++index) {
    download->snapshot_.files[index].priority = static_cast<int>(
        static_cast<std::uint8_t>(params.file_priorities[index]));
  }
  params.have_pieces.clear();
  params.verified_pieces.clear();
  params.unfinished_pieces.clear();
  params.piece_priorities.clear();
  params.completed_time = 0;
  params.finished_time = 0;
  params.seeding_time = 0;
  params.last_seen_complete = 0;
  params.flags &=
      ~(lt::torrent_flags::auto_managed |
        lt::torrent_flags::default_dont_download |
        lt::torrent_flags::seed_mode | lt::torrent_flags::stop_when_ready);
  params.flags |= lt::torrent_flags::paused;

  download->impl_->filePriorityUpdatePending = false;
  download->impl_->resumeAfterFilePriorityUpdate = false;
  download->impl_->desiredFilePriorities = params.file_priorities;
  download->impl_->appliedFilePriorities = params.file_priorities;
  download->impl_->desiredPiecePriorities.clear();
  download->impl_->appliedPiecePriorities.clear();
  download->impl_->recheckAfterAdd = true;
  download->impl_->resumeAfterRecheck = download->impl_->runRequested;
  download->beginProgressVerification();
  download->snapshot_.complete = false;
  download->snapshot_.selectedComplete = false;
  download->snapshot_.state = BtSnapshot::State::Recovering;
  download->clearError();
  if (!download->impl_->resumePath.empty()) {
    File(download->impl_->resumePath).remove();
  }
}

void BtSession::beginNativeDelete(const std::shared_ptr<BtDownload>& download,
                                  DeleteIntent intent)
{
  if (!download) {
    return;
  }
  const auto handle = download->impl_->handle;
  if (!handle.is_valid() || !handle.in_session()) {
    download->impl_->nativeState = BtNativeState::Detached;
    if (intent == DeleteIntent::Permanent) {
      forgetHandles(download.get());
      download->finishStopping();
      impl_->downloads.erase(download->impl_->gid);
    }
    return;
  }

  if (intent == DeleteIntent::Replace) {
    prepareFreshAdd(download.get());
  }

  const auto key = hashKey(handle.info_hashes());
  if (key.empty()) {
    download->setError("BitTorrent native handle has no info hash");
    download->shutdownStage_ = BtDownload::ShutdownStage::Complete;
    download->impl_->nativeState = BtNativeState::Detached;
    return;
  }

  impl_->pendingDeletes[key] = {download, intent};
  download->impl_->nativeState = BtNativeState::Removing;
  if (intent == DeleteIntent::Permanent) {
    download->beginRemoving();
  }
  impl_->session->remove_torrent(handle, lt::session::delete_partfile);
}

void BtSession::forgetHandles(BtDownload* download)
{
  for (auto it = impl_->handles.begin(); it != impl_->handles.end();) {
    const auto owner = it->second.lock();
    if (!owner || owner.get() == download) {
      it = impl_->handles.erase(it);
    }
    else {
      ++it;
    }
  }
}

void BtSession::finishNativeDelete(const std::string& key,
                                   const std::string& error)
{
  const auto found = impl_->pendingDeletes.find(key);
  if (found == impl_->pendingDeletes.end()) {
    return;
  }
  auto pending = std::move(found->second);
  impl_->pendingDeletes.erase(found);
  const auto& download = pending.download;
  forgetHandles(download.get());
  download->impl_->handle = {};
  download->impl_->nativeState = BtNativeState::Detached;

  if (!error.empty()) {
    if (pending.intent == DeleteIntent::Replace) {
      download->setError("Unable to reset BitTorrent partfile: " + error);
      download->shutdownStage_ = BtDownload::ShutdownStage::Complete;
      download->snapshot_.state = BtSnapshot::State::Error;
    }
    else {
      A2_LOG_ERROR(
          fmt("Failed to delete BitTorrent partfile: %s", error.c_str()));
      download->finishStopping();
    }
    impl_->downloads.erase(download->impl_->gid);
    return;
  }

  if (pending.intent == DeleteIntent::Replace) {
    download->impl_->nativeState = BtNativeState::Adding;
    impl_->session->async_add_torrent(download->impl_->params);
    return;
  }

  download->finishStopping();
  impl_->downloads.erase(download->impl_->gid);
}

bool BtSession::recoverPartfile(const std::shared_ptr<BtDownload>& download,
                                const BtErrorSnapshot& error)
{
  if (!download || download->impl_->partfileRecoveryAttempted ||
      error.category != "libtorrent" ||
      error.code != static_cast<int>(lt::errors::file_too_short) ||
      (error.operation != "partfile_read" &&
       error.operation != "partfile_write")) {
    return false;
  }

  download->impl_->partfileRecoveryAttempted = true;
  download->impl_->filePriorityUpdatePending = false;
  download->impl_->resumeAfterFilePriorityUpdate = false;
  download->impl_->runRequested =
      download->group() && !download->group()->isPauseRequested();
  A2_LOG_WARN("Recovering a structurally invalid BitTorrent partfile");
  beginNativeDelete(download, DeleteIntent::Replace);
  return true;
}

void BtSession::attach(const std::shared_ptr<BtDownload>& download,
                       RequestGroup* group, AttachMode mode)
{
  if (!download || !group) {
    return;
  }

  const bool running = mode == AttachMode::Running;
  download->impl_->runRequested = running;
  download->initialize(group);
  download->configure(group->getOption().get());

  if (download->impl_->params.ti &&
      (download->snapshot().files.empty() ||
       group->getDownloadContext()->getFileEntries().size() !=
           static_cast<size_t>(
               download->impl_->params.ti->layout().num_files()))) {
    updateDownloadContext(download.get(), group);
  }
  if (download->impl_->params.ti) {
    download->impl_->params.file_priorities = makeFilePriorities(group);
  }

  impl_->downloads[group->getGID()] = download;

  if (download->impl_->nativeState == BtNativeState::Adding) {
    return;
  }
  if (download->impl_->nativeState == BtNativeState::Attached &&
      download->impl_->handle.is_valid() &&
      download->impl_->handle.in_session()) {
    impl_->handles[download->impl_->handle] = download;
    if (!running) {
      download->snapshot_.state = BtSnapshot::State::Paused;
      return;
    }

    if (download->fileSelectionApplying()) {
      download->configure(group->getOption().get());
      download->updateSelection(group->getDownloadContext());
      beginNativeDelete(download, DeleteIntent::Replace);
      return;
    }

    download->impl_->handle.clear_error();
    const bool filePriorityUpdatePending =
        applyDownloadOptions(download, group->getOption().get());
    if (filePriorityUpdatePending) {
      download->impl_->resumeAfterFilePriorityUpdate = true;
    }
    else {
      const bool selectionApplying = download->fileSelectionApplying();
      download->completeFileSelectionApply();
      if (selectionApplying) {
        requestProgressRefresh(download.get());
      }
      resumeTorrent(download.get());
    }
    return;
  }
  if (download->impl_->nativeState == BtNativeState::Removing) {
    return;
  }

  if (running) {
    download->impl_->params.flags &=
        ~(lt::torrent_flags::paused | lt::torrent_flags::auto_managed |
          lt::torrent_flags::stop_when_ready);
  }
  else {
    download->impl_->params.flags &=
        ~(lt::torrent_flags::paused | lt::torrent_flags::auto_managed);
    download->impl_->params.flags |= lt::torrent_flags::stop_when_ready;
    download->snapshot_.state = BtSnapshot::State::Paused;
    download->beginProgressVerification();
  }
  download->impl_->desiredFilePriorities =
      download->impl_->params.file_priorities;
  download->impl_->appliedFilePriorities =
      download->impl_->params.file_priorities;
  download->impl_->appliedPiecePriorities.clear();
  download->impl_->params.url_seed_headers =
      webSeedHeaders(group->getOption().get());
  download->impl_->nativeState = BtNativeState::Adding;
  impl_->session->async_add_torrent(download->impl_->params);
}

void BtSession::requestStop(const std::shared_ptr<BtDownload>& download,
                            BtDownload::StopReason reason)
{
  download->requestStop(reason);
  if (reason == BtDownload::StopReason::Stop &&
      download->shutdownStage() == BtDownload::ShutdownStage::PendingHandle &&
      download->impl_->handle.is_valid()) {
    beginNativeDelete(download, DeleteIntent::Permanent);
    return;
  }
  if (download->shutdownStage() != BtDownload::ShutdownStage::PendingHandle) {
    return;
  }
  if (!download->impl_->handle.is_valid()) {
    return;
  }
  download->beginSavingResume();
  download->impl_->checkpointPending = false;
  download->impl_->handle.pause();
  if (download->impl_->resumeSaveOutstanding) {
    download->impl_->stopSavePending = true;
    return;
  }
  download->impl_->resumeSaveOutstanding = true;
  download->impl_->handle.save_resume_data(lt::torrent_handle::save_info_dict);
}

void BtSession::discard(const std::shared_ptr<BtDownload>& download)
{
  if (!download) {
    return;
  }
  if (download->group()) {
    impl_->downloads[download->group()->getGID()] = download;
  }
  download->group_ = nullptr;
  download->stopReason_ = BtDownload::StopReason::Stop;
  download->shutdownStage_ = BtDownload::ShutdownStage::Removing;
  if (download->impl_->nativeState == BtNativeState::Adding) {
    download->impl_->nativeState = BtNativeState::Removing;
    return;
  }
  const auto handle = download->impl_->handle;
  if (!handle.is_valid() || !handle.in_session()) {
    forgetHandles(download.get());
    impl_->downloads.erase(download->impl_->gid);
    download->impl_->nativeState = BtNativeState::Detached;
    return;
  }
  beginNativeDelete(download, DeleteIntent::Permanent);
}

} // namespace aria2
