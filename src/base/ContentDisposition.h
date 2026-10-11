/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <string>
#include <string_view>

namespace aria2 {

// Extracts the file name from a Content-Disposition value the way browsers
// do: filename* (RFC 5987) wins over filename; quoted strings, bare tokens,
// MIME encoded-words, raw UTF-8, Latin-1 and percent-encoded UTF-8 are
// accepted. Directory components are dropped. Returns UTF-8 or empty.
std::string contentDispositionFileName(std::string_view header);

} // namespace aria2
