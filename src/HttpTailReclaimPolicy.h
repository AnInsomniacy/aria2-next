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
#ifndef D_HTTP_TAIL_RECLAIM_POLICY_H
#define D_HTTP_TAIL_RECLAIM_POLICY_H

#include "common.h"

#include <chrono>
#include <string>

namespace aria2 {

struct HttpTailReclaimState {
  std::string protocol;
  bool p2pInvolved;
  int64_t totalLength;
  int64_t pendingLength;
  bool hasMissingUnusedPiece;
  int numConcurrentCommand;
  int numStreamCommand;
  uint64_t currentSessionDownloadLength;
  uint64_t lastSessionDownloadLength;
  std::chrono::seconds noProgressTime;
  std::chrono::seconds stallTime;
};

bool isHttpTailBlocked(const HttpTailReclaimState& state);

bool shouldReclaimHttpTailSegment(const HttpTailReclaimState& state);

} // namespace aria2

#endif // D_HTTP_TAIL_RECLAIM_POLICY_H
