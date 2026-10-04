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
// Keep the Windows socket ABI consistent before native library headers.
#include "DiskAdaptor.h"
#include "FileEntry.h"
#include "Option.h"
#include "PieceStorage.h"
#include "common.h" // IWYU pragma: keep
#include "StreamStore.h"

#include "CurlSession.h"

#include "error_code.h"
#include "transport/HttpHeaders.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cinttypes>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <curl/curl.h>
#include <curl/system.h>
#include <exception>
#include <stdexcept>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ChecksumCheckIntegrityEntry.h"
#include "CurlCheckIntegrityEntry.h"
#include "CurlDownload.h"
#include "media/MediaDownload.h"
#include "CurlDownloadCommand.h"
#include "CurlDownloadImpl.h"
#include "stream/CurlHandle.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "DefaultDiskWriterFactory.h"
#include "DiskWriterFactory.h"
#include "Exception.h"
#include "File.h"
#include "GroupId.h"
#include "Log.h"
#include "Request.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "TimeA2.h"
#include "fmt.h"
#include "prefs.h"
#include "uri.h"
#include "support/Text.h"
#include "support/Encoding.h"
#include "support/FilePath.h"
#include "support/OutputName.h"
#include "wallclock.h"

namespace aria2 {
namespace {
std::string outputPath(RequestGroup* group, const std::string& uriValue)
{
  const auto option = group->getOption();
  std::string name = option->get(PREF_OUT);
  if (name.empty()) {
    const auto& configured =
        group->getDownloadContext()->getFirstFileEntry()->getPath();
    if (!configured.empty()) {
      return configured;
    }
    name = output::suggestedName(*option, uriValue);
  }
  return util::applyDir(option->get(PREF_DIR), name);
}
} // namespace

bool CurlSession::openOutput(CurlDownload* download, bool preserveExisting,
                             bool truncateOwned)
{
  auto& impl = *download->impl_;
  impl.createdOutput = false;
  const auto fallbackError = preserveExisting ? error_code::FILE_OPEN_ERROR
                                              : error_code::FILE_CREATE_ERROR;
  impl.writer.reset();
  try {
    const auto& configuredFactory = impl.group->getDiskWriterFactory();
    if (configuredFactory) {
      impl.writer = configuredFactory->newDiskWriter(impl.path);
    }
    else {
      impl.writer = DefaultDiskWriterFactory().newDiskWriter(impl.path);
    }
    if (!impl.writer) {
      CurlHandle::fail(download, fallbackError,
                       "Unable to create the output writer");
      return false;
    }
    if (preserveExisting) {
      impl.io.submit([writer = impl.writer] {
        writer->openExistingFile();
        writer->enableSparse();
      });
    }
    else if (truncateOwned ||
             impl.group->getOption()->getAsBool(PREF_ALLOW_OVERWRITE)) {
      impl.io.submit([writer = impl.writer] {
        writer->initAndOpenFile();
        writer->enableSparse();
      });
    }
    else {
      impl.io.submit([writer = impl.writer] {
        writer->openNewFile();
        writer->enableSparse();
      });
      impl.createdOutput = true;
    }
    return true;
  }
  catch (const Exception& error) {
    impl.writer.reset();
    CurlHandle::fail(download, error.getErrorCode(), error.what());
  }
  catch (const std::exception& error) {
    impl.writer.reset();
    CurlHandle::fail(download, fallbackError, error.what());
  }
  catch (...) {
    impl.writer.reset();
    CurlHandle::fail(download, fallbackError, "Unable to open the output file");
  }
  return false;
}

// Called on the engine thread after the final payload headers, before any bytes
// are written. Binding out makes the selected path part of native recovery.
bool CurlSession::resolveOutput(CurlDownload* download, CURL* easy)
{
  auto& impl = *download->impl_;
  if (!impl.filenamePending) {
    return true;
  }
  char* effective = nullptr;
  curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective);
  auto option = impl.group->getOption();
  const auto name = output::suggestedName(
      *option, effective ? effective : impl.currentUri,
      http::responseHeader(easy, "Content-Disposition"), impl.currentUri);
  impl.path = util::applyDir(option->get(PREF_DIR), name);
  auto context = impl.group->getDownloadContext();
  context->getFirstFileEntry()->setPath(impl.path);
  if (!impl.dryRun) {
    impl.group->shouldCancelDownloadForSafety();
    impl.path = impl.group->getFirstFilePath();
  }
  context->setBasePath(impl.path);
  option->put(PREF_OUT, impl.path);
  impl.filenamePending = false;
  if (impl.dryRun) {
    return true;
  }
  return openOutput(download, false);
}

void CurlSession::closeOutput(CurlDownload* download) noexcept
{
  if (!download || !download->impl_->writer) {
    return;
  }
  try {
    download->impl_->io.submit(
        [writer = download->impl_->writer] { writer->closeFile(); });
  }
  catch (const std::exception& error) {
    A2_LOG_ERROR(fmt("Closing stream output failed: %s", error.what()));
  }
  catch (...) {
    A2_LOG_ERROR("Closing stream output failed");
  }
  download->impl_->writer.reset();
}

bool CurlSession::prepare(const std::shared_ptr<CurlDownload>& download,
                          RequestGroup* group)
{
  download->snapshot_.errorCode = error_code::UNDEFINED;
  download->snapshot_.error.clear();
  download->snapshot_.totalLength = 0;
  download->snapshot_.completedLength = 0;
  download->snapshot_.sessionDownloadLength = 0;
  download->snapshot_.connections = 0;
  if (!transport_.get() || download->impl_->uris.empty()) {
    CurlHandle::fail(download.get(), error_code::NETWORK_PROBLEM,
                     "The curl transfer session is unavailable");
    return false;
  }
  auto& impl = *download->impl_;
  for (auto& handle : impl.handles) {
    handle->reset();
  }
  impl.handles.clear();
  closeOutput(download.get());
  impl.group = group;
  impl.endpoints.assign(impl.uris.size() * 3, CurlEndpoint{});
  impl.families = {{CURL_IPRESOLVE_WHATEVER, CURL_IPRESOLVE_WHATEVER}};
  impl.idleWorkers.clear();
  impl.planner.clear();
  impl.etag.clear();
  impl.lastModified.clear();
  impl.dryRun = group->getOption()->getAsBool(PREF_DRY_RUN);
  impl.stopRequested = false;
  impl.plannerConfigured = false;
  impl.kickPending = false;
  impl.fileNotFoundCount = 0;
  impl.rangeValidated = false;
  impl.fullDownload = false;
  impl.fullRestarts = 0;
  impl.fullRetryAt.reset();
  impl.finalCheckpointQueued = false;
  impl.pendingFinalize.reset();
  impl.pendingFailure.reset();
  impl.pendingRestart.clear();
  impl.pendingStop.reset();
  impl.maxConnections = effectiveStreamMaxConnections(group->getOption().get());
  impl.connectionLimit = impl.maxConnections;
  impl.maxRangeSize =
      group->getOption()->getAsLLInt(PREF_STREAM_MAX_RANGE_SIZE);
  impl.connectionEpoch = 0;
  impl.lastRecoveryDownloadLength = 0;
  impl.recoverConnectionsAt = {};
  impl.preferredUriIndex %= impl.uris.size();
  const auto& uriValue = impl.uris[impl.preferredUriIndex];
  impl.currentUri = uriValue;
  const auto& configured = group->getFirstFilePath();
  const bool namedByCaller = !configured.empty() && configured != impl.path;
  impl.path = outputPath(group, uriValue);

  auto context = group->getDownloadContext();
  context->setBasePath(impl.path);
  context->getFirstFileEntry()->setPath(impl.path);

  StreamState state;
  const auto taskId = GroupId::toHex(group->getGID());
  const auto hasState = store_->load(state, taskId, impl.path) &&
                        std::find(impl.uris.begin(), impl.uris.end(),
                                  state.uri) != impl.uris.end() &&
                        state.path == impl.path;
  if (hasState) {
    const auto restored =
        std::find(impl.uris.begin(), impl.uris.end(), state.uri);
    impl.preferredUriIndex = static_cast<size_t>(restored - impl.uris.begin());
    impl.currentUri = *restored;
  }
  uri::UriStruct parsed;
  impl.http = uri::parse(parsed, impl.currentUri) &&
              (util::strieq(parsed.protocol, "http") ||
               util::strieq(parsed.protocol, "https"));
  impl.filenamePending = impl.http && !hasState && !namedByCaller &&
                         group->getOption()->blank(PREF_OUT);
  if (impl.filenamePending) {
    impl.allowFullRestart = true;
    download->snapshot_.state = CurlSnapshot::State::Active;
    return true;
  }
  File output(impl.path);
  const auto existingLength = output.isFile() ? output.size() : 0;
  const bool rangesFit =
      std::all_of(state.completedRanges.begin(), state.completedRanges.end(),
                  [existingLength](const auto& range) {
                    return range.second <= existingLength;
                  });
  const bool restoreState = hasState && output.isFile() && rangesFit &&
                            existingLength >= state.completedLength;
  if (hasState && !restoreState) {
    discardRecovery(download);
  }
  impl.allowFullRestart = true;
  if (restoreState) {
    impl.planner.restore(state.completedRanges);
    impl.etag = http::normalizeStrongEtag(state.etag);
    impl.lastModified = state.lastModified;
    download->snapshot_.totalLength = state.totalLength;
  }
  else if (!impl.dryRun) {
    group->shouldCancelDownloadForSafety();
    impl.path = group->getFirstFilePath();
    context->setBasePath(impl.path);
    group->getOption()->put(PREF_OUT, impl.path);
  }
  if (impl.dryRun) {
    impl.planner.clear();
    download->snapshot_.completedLength = 0;
    download->snapshot_.sessionDownloadLength = 0;
    download->snapshot_.state = CurlSnapshot::State::Active;
    return true;
  }
  const bool preserveExisting =
      restoreState || impl.planner.completedLength() > 0;
  if (!openOutput(download.get(), preserveExisting)) {
    return false;
  }
  download->snapshot_.completedLength = impl.planner.completedLength();
  download->snapshot_.sessionDownloadLength = 0;
  download->snapshot_.state = CurlSnapshot::State::Active;
  return true;
}

void CurlSession::restorePaused(const std::shared_ptr<CurlDownload>& download,
                                RequestGroup* group)
{
  auto& impl = *download->impl_;
  if (download->snapshot_.state != CurlSnapshot::State::Waiting ||
      impl.uris.empty()) {
    return;
  }
  impl.path = outputPath(group, impl.uris.front());
  download->snapshot_.state = CurlSnapshot::State::Paused;
  auto context = group->getDownloadContext();
  context->setBasePath(impl.path);
  context->getFirstFileEntry()->setPath(impl.path);

  StreamState state;
  File file(impl.path);
  const auto groupId = GroupId::toHex(group->getGID());
  if (!store_->load(state, groupId, impl.path) || state.gid != groupId ||
      state.path != impl.path || !file.isFile() ||
      std::find(impl.uris.begin(), impl.uris.end(), state.uri) ==
          impl.uris.end()) {
    return;
  }
  const auto size = file.size();
  if (size < state.completedLength ||
      std::any_of(
          state.completedRanges.begin(), state.completedRanges.end(),
          [size](const auto& range) { return range.second > size; })) {
    return;
  }
  impl.group = group;
  impl.planner.restore(state.completedRanges);
  impl.etag = http::normalizeStrongEtag(state.etag);
  impl.lastModified = state.lastModified;
  impl.currentUri = state.uri;
  download->snapshot_.currentUri = state.uri;
  download->snapshot_.totalLength = state.totalLength;
  download->snapshot_.completedLength = impl.planner.completedLength();
  context->getFirstFileEntry()->setLength(state.totalLength);
  context->markTotalLengthIsKnown();
}

bool CurlSession::checkpoint(const std::shared_ptr<CurlDownload>& download,
                             bool force)
{
  auto& impl = *download->impl_;
  if (!impl.group || impl.dryRun || impl.filenamePending ||
      (!force && impl.stopRequested) ||
      (!force && !impl.lastCheckpoint.isZero() &&
       impl.lastCheckpoint.difference(global::wallclock()) <
           std::chrono::seconds(1))) {
    return true;
  }
  StreamState state;
  state.gid = GroupId::toHex(impl.group->getGID());
  state.uri = impl.currentUri;
  state.path = impl.path;
  state.etag = impl.etag;
  state.lastModified = impl.lastModified;
  state.totalLength = download->snapshot_.totalLength;
  state.completedLength = impl.planner.completedLength();
  state.completedRanges = impl.planner.completedRanges();
  if (!force && impl.io.busy())
    return true;
  impl.io.submit([store = store_, state = std::move(state)] {
    if (!store->save(state))
      throw std::runtime_error("Unable to persist stream recovery state");
  });
  impl.lastCheckpoint = global::wallclock();
  return true;
}

void CurlSession::finalize(const std::shared_ptr<CurlDownload>& download,
                           curl_off_t reportedFileTime)
{
  auto& impl = *download->impl_;
  if (impl.io.busy()) {
    impl.pendingFinalize = reportedFileTime;
    return;
  }
  closeOutput(download.get());
  if (impl.io.busy()) {
    impl.pendingFinalize = reportedFileTime;
    return;
  }
  auto length = File(impl.path).size();
  if (download->snapshot_.totalLength > 0) {
    length = download->snapshot_.totalLength;
  }
  if (reportedFileTime >= 0 &&
      impl.group->getOption()->getAsBool(PREF_REMOTE_TIME)) {
    File(impl.path).utime(Time(), Time(static_cast<time_t>(reportedFileTime)));
  }
  download->snapshot_.completedLength = length;
  download->snapshot_.totalLength = length;
  auto context = impl.group->getDownloadContext();
  context->getFirstFileEntry()->setLength(length);
  context->markTotalLengthIsKnown();
  if (!impl.group->getPieceStorage()) {
    impl.group->initPieceStorage();
    impl.group->getPieceStorage()->getDiskAdaptor()->openExistingFile();
  }
  context->resetDownloadStopTime();
  if (!impl.finalCheckpointQueued) {
    checkpoint(download, true);
    impl.finalCheckpointQueued = true;
    impl.pendingFinalize = reportedFileTime;
    return;
  }
  impl.pendingFinalize.reset();
  impl.group->getPieceStorage()->markAllPiecesDone();
  download->snapshot_.state = CurlSnapshot::State::Complete;
  eraseTask(download.get());
  if (context->isPieceHashVerificationAvailable()) {
    auto entry = std::unique_ptr<CurlCheckIntegrityEntry>(
        new CurlCheckIntegrityEntry(impl.group));
    entry->initValidator();
    engine_->getCheckIntegrityMan()->pushEntry(std::move(entry));
  }
  else if (context->isChecksumVerificationAvailable()) {
    auto entry = std::unique_ptr<ChecksumCheckIntegrityEntry>(
        new ChecksumCheckIntegrityEntry(impl.group));
    entry->initValidator();
    engine_->getCheckIntegrityMan()->pushEntry(std::move(entry));
  }
  engine_->setNoWait(true);
  engine_->setRefreshInterval(std::chrono::milliseconds(0));
}

void CurlSession::failTask(const std::shared_ptr<CurlDownload>& download,
                           error_code::Value errorCode,
                           const std::string& message, bool retainState)
{
  auto& impl = *download->impl_;
  auto finalMessage = message;
  impl.pendingStop.reset();
  impl.pendingFinalize.reset();
  impl.pendingRestart.clear();
  impl.stopRequested = true;
  if (impl.io.busy()) {
    download->snapshot_.state = CurlSnapshot::State::Active;
    impl.pendingFailure = std::make_pair(errorCode, message);
    impl.retainFailure = retainState;
    cancelHandles(download);
    return;
  }
  if (retainState) {
    if (!download->failed()) {
      try {
        for (auto& handle : impl.handles) {
          stream::flushWriteBuffer(impl, *handle);
        }
        download->snapshot_.completedLength = impl.planner.completedLength();
      }
      catch (const Exception& error) {
        errorCode = error.getErrorCode();
        finalMessage = error.what();
      }
      catch (const std::exception& error) {
        errorCode = error_code::FILE_IO_ERROR;
        finalMessage = error.what();
      }
    }
    if (!impl.finalCheckpointQueued) {
      checkpoint(download, true);
      impl.finalCheckpointQueued = true;
    }
  }
  else if (impl.group && !impl.finalCheckpointQueued) {
    discardRecovery(download);
    impl.finalCheckpointQueued = true;
  }
  cancelHandles(download);
  closeOutput(download.get());
  if (impl.io.busy()) {
    impl.pendingFailure = std::make_pair(errorCode, finalMessage);
    download->snapshot_.state = CurlSnapshot::State::Active;
    impl.retainFailure = retainState;
    return;
  }
  impl.pendingFailure.reset();
  CurlHandle::fail(download.get(), errorCode, finalMessage);
  A2_LOG_ERROR(fmt("component=stream event=task_failed gid=%s error_code=%d "
                   "completed=%" PRId64 " uri=%s message=%s",
                   CurlHandle::gid(download.get()).c_str(),
                   static_cast<int>(errorCode),
                   download->snapshot_.completedLength,
                   logging::sanitizeUri(impl.currentUri).c_str(),
                   logging::sanitizeText(finalMessage).c_str()));
  eraseTask(download.get());
  if (engine_) {
    engine_->setNoWait(true);
  }
}

} // namespace aria2
