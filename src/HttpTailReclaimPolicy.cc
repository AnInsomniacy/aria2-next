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
#include "HttpTailReclaimPolicy.h"

#include "util.h"

namespace aria2 {
namespace {
bool isHttpProtocol(const std::string& protocol)
{
  return util::strieq(protocol, "http") || util::strieq(protocol, "https");
}
} // namespace

bool isHttpTailBlocked(const HttpTailReclaimState& state)
{
  if (!isHttpProtocol(state.protocol) || state.p2pInvolved ||
      state.totalLength <= 0 || state.pendingLength <= 0 ||
      state.hasMissingUnusedPiece || state.numConcurrentCommand <= 1 ||
      state.numStreamCommand <= 0 ||
      state.numStreamCommand >= state.numConcurrentCommand) {
    return false;
  }

  return true;
}

bool shouldReclaimHttpTailSegment(const HttpTailReclaimState& state)
{
  if (!isHttpTailBlocked(state)) {
    return false;
  }

  if (state.currentSessionDownloadLength > state.lastSessionDownloadLength) {
    return false;
  }

  return state.noProgressTime >= state.stallTime;
}

} // namespace aria2
