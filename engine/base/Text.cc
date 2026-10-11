/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Text.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>

namespace aria2::text {

std::string_view trim(std::string_view value)
{
  constexpr std::string_view space = " \t\r\n\v\f";
  const auto first = value.find_first_not_of(space);
  if (first == std::string_view::npos) {
    return {};
  }
  return value.substr(first, value.find_last_not_of(space) - first + 1);
}

std::vector<std::string> split(std::string_view value, char separator,
                               bool keepEmpty)
{
  std::vector<std::string> parts;
  for (size_t begin = 0;;) {
    const auto end = value.find(separator, begin);
    const auto part = trim(value.substr(begin, end - begin));
    if (keepEmpty || !part.empty()) {
      parts.emplace_back(part);
    }
    if (end == std::string_view::npos) {
      return parts;
    }
    begin = end + 1;
  }
}

std::string join(const std::vector<std::string>& values, std::string_view glue)
{
  std::string result;
  for (const auto& value : values) {
    if (!result.empty()) {
      result += glue;
    }
    result += value;
  }
  return result;
}

std::string lower(std::string_view value)
{
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(), [](char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
  });
  return result;
}

bool iequals(std::string_view a, std::string_view b)
{
  return a.size() == b.size() && lower(a) == lower(b);
}

bool istartsWith(std::string_view value, std::string_view prefix)
{
  return value.size() >= prefix.size() &&
         iequals(value.substr(0, prefix.size()), prefix);
}

std::string replaceAll(std::string value, std::string_view from,
                       std::string_view to)
{
  if (from.empty()) {
    return value;
  }
  for (size_t at = 0; (at = value.find(from, at)) != std::string::npos;
       at += to.size()) {
    value.replace(at, from.size(), to);
  }
  return value;
}

bool isUtf8(std::string_view value)
{
  for (size_t i = 0; i < value.size();) {
    const auto c = static_cast<unsigned char>(value[i]);
    size_t length = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3
                                : (c >> 3) == 0x1e  ? 4
                                                    : 0;
    if (length == 0 || i + length > value.size() || (length == 2 && c < 0xc2) ||
        (length == 4 && c > 0xf4)) {
      return false;
    }
    uint32_t code = length == 1 ? c : c & (0x7f >> length);
    for (size_t k = 1; k < length; ++k) {
      const auto next = static_cast<unsigned char>(value[i + k]);
      if ((next & 0xc0) != 0x80) {
        return false;
      }
      code = code << 6 | (next & 0x3f);
    }
    if ((length == 3 && (code < 0x800 || (code >= 0xd800 && code <= 0xdfff))) ||
        (length == 4 && (code < 0x10000 || code > 0x10ffff))) {
      return false;
    }
    i += length;
  }
  return true;
}

std::string latin1ToUtf8(std::string_view value)
{
  std::string result;
  result.reserve(value.size());
  for (const char ch : value) {
    const auto c = static_cast<unsigned char>(ch);
    if (c < 0x80) {
      result += ch;
    }
    else {
      result += static_cast<char>(0xc0 | c >> 6);
      result += static_cast<char>(0x80 | (c & 0x3f));
    }
  }
  return result;
}

namespace {
int hexValue(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
} // namespace

std::string percentDecode(std::string_view value)
{
  std::string result;
  result.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const auto high = hexValue(value[i + 1]);
      const auto low = hexValue(value[i + 2]);
      if (high >= 0 && low >= 0) {
        result += static_cast<char>(high << 4 | low);
        i += 2;
        continue;
      }
    }
    result += value[i];
  }
  return result;
}

std::string percentEncode(std::string_view value)
{
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string result;
  for (const char ch : value) {
    const auto c = static_cast<unsigned char>(ch);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
        c == '~') {
      result += ch;
    }
    else {
      result += '%';
      result += digits[c >> 4];
      result += digits[c & 0xf];
    }
  }
  return result;
}

std::string toHex(std::string_view bytes)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (const char ch : bytes) {
    const auto c = static_cast<unsigned char>(ch);
    result += digits[c >> 4];
    result += digits[c & 0xf];
  }
  return result;
}

std::optional<std::string> fromHex(std::string_view hex)
{
  if (hex.size() % 2) {
    return std::nullopt;
  }
  std::string result;
  result.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    const auto high = hexValue(hex[i]);
    const auto low = hexValue(hex[i + 1]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    result += static_cast<char>(high << 4 | low);
  }
  return result;
}

namespace {
constexpr char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string base64Encode(std::string_view bytes)
{
  std::string result;
  result.reserve((bytes.size() + 2) / 3 * 4);
  for (size_t i = 0; i < bytes.size(); i += 3) {
    uint32_t block = static_cast<unsigned char>(bytes[i]) << 16;
    if (i + 1 < bytes.size())
      block |= static_cast<unsigned char>(bytes[i + 1]) << 8;
    if (i + 2 < bytes.size())
      block |= static_cast<unsigned char>(bytes[i + 2]);
    result += alphabet[block >> 18 & 63];
    result += alphabet[block >> 12 & 63];
    result += i + 1 < bytes.size() ? alphabet[block >> 6 & 63] : '=';
    result += i + 2 < bytes.size() ? alphabet[block & 63] : '=';
  }
  return result;
}

std::optional<std::string> base64Decode(std::string_view text)
{
  std::array<int, 256> table;
  table.fill(-1);
  for (int i = 0; i < 64; ++i) {
    table[static_cast<unsigned char>(alphabet[i])] = i;
  }
  table['-'] = 62; // URL-safe alphabet
  table['_'] = 63;
  std::string result;
  uint32_t block = 0;
  int bits = 0;
  for (const char ch : text) {
    if (ch == '=' || ch == '\r' || ch == '\n' || ch == ' ') {
      continue;
    }
    const auto value = table[static_cast<unsigned char>(ch)];
    if (value < 0) {
      return std::nullopt;
    }
    block = block << 6 | static_cast<uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      result += static_cast<char>(block >> bits & 0xff);
    }
  }
  return result;
}

std::optional<int64_t> toInt(std::string_view value)
{
  int64_t result = 0;
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (value.empty() || error != std::errc() ||
      end != value.data() + value.size() || result < 0) {
    return std::nullopt;
  }
  return result;
}

std::optional<int64_t> toSize(std::string_view value)
{
  value = trim(value);
  int64_t unit = 1;
  if (!value.empty()) {
    switch (value.back()) {
    case 'K':
    case 'k':
      unit = int64_t{1} << 10;
      break;
    case 'M':
    case 'm':
      unit = int64_t{1} << 20;
      break;
    case 'G':
    case 'g':
      unit = int64_t{1} << 30;
      break;
    }
    if (unit != 1) {
      value.remove_suffix(1);
    }
  }
  if (value.empty() || value.front() == '-' || value.front() == '+') {
    return std::nullopt;
  }
  double number = 0;
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), number);
  if (error != std::errc() || end != value.data() + value.size() ||
      !std::isfinite(number) || number * unit > 9.2e18) {
    return std::nullopt;
  }
  return static_cast<int64_t>(number * static_cast<double>(unit));
}

std::string formatSize(int64_t bytes)
{
  constexpr std::array<const char*, 5> units{"B", "KiB", "MiB", "GiB", "TiB"};
  auto value = static_cast<double>(bytes);
  size_t unit = 0;
  while (value >= 1024 && unit + 1 < units.size()) {
    value /= 1024;
    ++unit;
  }
  return unit == 0 ? fmt::format("{}B", bytes)
                   : fmt::format("{:.1f}{}", value, units[unit]);
}

std::string formatDuration(int64_t seconds)
{
  if (seconds >= 3600) {
    return fmt::format("{}h{}m{}s", seconds / 3600, seconds % 3600 / 60,
                       seconds % 60);
  }
  if (seconds >= 60) {
    return fmt::format("{}m{}s", seconds / 60, seconds % 60);
  }
  return fmt::format("{}s", seconds);
}

} // namespace aria2::text
