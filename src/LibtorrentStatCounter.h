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
#ifndef D_LIBTORRENT_STAT_COUNTER_H
#define D_LIBTORRENT_STAT_COUNTER_H

#include <cstdint>

namespace aria2 {

class LibtorrentStatCounter {
private:
  int64_t downloadedLength_;
  int64_t uploadedLength_;

public:
  LibtorrentStatCounter();

  int64_t updateDownload(int64_t downloadedLength);
  int64_t updateUpload(int64_t uploadedLength);
};

} // namespace aria2

#endif // D_LIBTORRENT_STAT_COUNTER_H
