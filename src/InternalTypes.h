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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* copyright --> */
#ifndef D_INTERNAL_TYPES_H
#define D_INTERNAL_TYPES_H

#include "common.h"

#include <string>
#include <utility>
#include <vector>

namespace aria2 {

using KeyVals = std::vector<std::pair<std::string, std::string>>;

enum DownloadEvent {
  EVENT_ON_DOWNLOAD_START = 1,
  EVENT_ON_DOWNLOAD_PAUSE,
  EVENT_ON_DOWNLOAD_STOP,
  EVENT_ON_DOWNLOAD_COMPLETE,
  EVENT_ON_DOWNLOAD_ERROR,
  EVENT_ON_BT_DOWNLOAD_COMPLETE
};

enum OffsetMode {
  OFFSET_MODE_SET,
  OFFSET_MODE_CUR,
  OFFSET_MODE_END
};

enum BtFileMode {
  BT_FILE_MODE_NONE,
  BT_FILE_MODE_SINGLE,
  BT_FILE_MODE_MULTI
};

} // namespace aria2

#endif // D_INTERNAL_TYPES_H
