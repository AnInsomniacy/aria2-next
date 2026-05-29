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
#include "LibtorrentStatCounter.h"

namespace aria2 {

LibtorrentStatCounter::LibtorrentStatCounter()
    : downloadedLength_(0), uploadedLength_(0)
{
}

int64_t LibtorrentStatCounter::updateDownload(int64_t downloadedLength)
{
  if (downloadedLength <= downloadedLength_) {
    downloadedLength_ = downloadedLength;
    return 0;
  }
  auto delta = downloadedLength - downloadedLength_;
  downloadedLength_ = downloadedLength;
  return delta;
}

int64_t LibtorrentStatCounter::updateUpload(int64_t uploadedLength)
{
  if (uploadedLength <= uploadedLength_) {
    uploadedLength_ = uploadedLength;
    return 0;
  }
  auto delta = uploadedLength - uploadedLength_;
  uploadedLength_ = uploadedLength;
  return delta;
}

} // namespace aria2
