/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "media/MuxInput.h"

namespace aria2::media::muxing {
// Keep native source clocks and align selected tracks to the same recording
// window. Presentation boundaries are milliseconds; packet offsets are us.
// A ranged download ends all tracks where the first of them ends.
void alignHls(Inputs&, int64_t presentationStart, int64_t nextBoundary,
              int64_t& liveOrigin, bool firstEpoch, bool live,
              const std::string& format, bool ranged);
} // namespace aria2::media::muxing
