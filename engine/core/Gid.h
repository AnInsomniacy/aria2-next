/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aria2 {

// Task identifiers are non-zero 64-bit values written as 16 hex digits.
using Gid = uint64_t;

Gid newGid();
std::string gidText(Gid gid);
std::optional<Gid> parseGid(std::string_view text);

} // namespace aria2
