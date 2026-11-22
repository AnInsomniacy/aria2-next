/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "base/Path.h"

#include <string>
#include <string_view>

namespace aria2 {

class Options;

// The file name policy from --filename-invalid-chars, --filename-max-length,
// --filename-truncate-head and --filename-truncate-tail.
FileNamePolicy fileNamePolicy(const Options& options);

// Applies that policy to a name the engine derived from a URI, a header or a
// task. An explicit --out is not passed through this: it may name a
// subdirectory and is used as given.
std::string sanitizeFileName(std::string_view name, const Options& options);

} // namespace aria2
