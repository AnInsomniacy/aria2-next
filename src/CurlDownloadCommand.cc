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
#include "CurlDownloadCommand.h"
#include "Command.h"
#include <exception>
#include <memory>
#include <utility>

#include "CurlDownload.h"
#include "CurlDownloadImpl.h"
#include "CurlSession.h"
#include "DownloadEngine.h"
#include "RequestGroup.h"
#include "error_code.h"
#include "media/MediaDownload.h"

namespace aria2 {

CurlDownloadCommand::CurlDownloadCommand(cuid_t cuid,
                                         std::shared_ptr<CurlDownload> download,
                                         CurlSession* session,
                                         RequestGroup* group,
                                         DownloadEngine* engine)
    : Command(cuid),
      download_(std::move(download)),
      session_(session),
      group_(group),
      engine_(engine)
{
  setStatusRealtime();
  group_->increaseNumCommand();
}

CurlDownloadCommand::~CurlDownloadCommand() { group_->decreaseNumCommand(); }

bool CurlDownloadCommand::execute()
{
  session_->advance(download_);
  session_->armTimeout();
  if (group_->isHaltRequested() && !download_->impl_->stopRequested) {
    session_->stop(download_,
                   group_->isPauseRequested() || group_->isShutdownRequested());
  }
  if (download_->failed()) {
    const auto& snapshot = download_->snapshot();
    group_->setLastErrorCode(snapshot.errorCode == error_code::UNDEFINED
                                 ? error_code::NETWORK_PROBLEM
                                 : snapshot.errorCode,
                             snapshot.error.c_str());
  }
  if (download_->stopped()) {
    if (download_->snapshot().mediaManifest && !download_->failed() &&
        !group_->isHaltRequested()) {
      auto mediaDownload =
          std::make_shared<media::Download>(download_->snapshot().currentUri);
      group_->setMediaDownload(mediaDownload);
      group_->setCurlDownload(nullptr);
      try {
        engine_->addCommand(mediaDownload->start(group_, engine_));
      }
      catch (const std::exception& error) {
        group_->setLastErrorCode(error_code::NETWORK_PROBLEM, error.what());
      }
    }
    return true;
  }
  engine_->addCommand(std::unique_ptr<Command>(this));
  return false;
}

} // namespace aria2
