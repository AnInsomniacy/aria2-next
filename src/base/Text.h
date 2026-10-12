/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aria2::text {

std::string_view trim(std::string_view value);
std::vector<std::string> split(std::string_view value, char separator,
                               bool keepEmpty = false);
std::string join(const std::vector<std::string>& values, std::string_view glue);
std::string lower(std::string_view value);
bool iequals(std::string_view a, std::string_view b);
bool istartsWith(std::string_view value, std::string_view prefix);
std::string replaceAll(std::string value, std::string_view from,
                       std::string_view to);

bool isUtf8(std::string_view value);
// Replaces every byte that does not belong to a well-formed UTF-8 sequence
// with U+FFFD, e.g. ANSI code page text from Windows error messages.
std::string repairUtf8(std::string_view value);
std::string latin1ToUtf8(std::string_view value);
std::string percentDecode(std::string_view value);
std::string percentEncode(std::string_view value); // RFC 3986 unreserved kept

std::string toHex(std::string_view bytes);
std::optional<std::string> fromHex(std::string_view hex);
std::string base64Encode(std::string_view bytes);
std::optional<std::string> base64Decode(std::string_view text);

// Integers in base 10 without signs, spaces or overflow.
std::optional<int64_t> toInt(std::string_view value);
// Decimal or exponent notation in the C locale, without spaces or a '+'.
std::optional<double> toDouble(std::string_view value);
// "1048576", "1M", "1.5K" -> bytes; K and M are binary units.
std::optional<int64_t> toSize(std::string_view value);
// 1536 -> "1.5KiB"; used by console output and logs.
std::string formatSize(int64_t bytes);
std::string formatDuration(int64_t seconds);

} // namespace aria2::text
