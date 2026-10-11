/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/ContentDisposition.h"

#include "base/Text.h"

#include <algorithm>
#include <optional>

namespace aria2 {
namespace {

struct Parameter {
  std::string name;
  std::string value;
};

// Splits `type; a=1; b="x;y"` into parameters, honouring quoted strings.
std::vector<Parameter> parameters(std::string_view header)
{
  std::vector<Parameter> result;
  size_t at = header.find(';');
  while (at != std::string_view::npos && at < header.size()) {
    ++at;
    const auto equals = header.find('=', at);
    const auto next = header.find(';', at);
    if (equals == std::string_view::npos ||
        (next != std::string_view::npos && next < equals)) {
      at = next;
      continue;
    }
    Parameter parameter{text::lower(text::trim(header.substr(at, equals - at))),
                        {}};
    at = equals + 1;
    while (at < header.size() && (header[at] == ' ' || header[at] == '\t')) {
      ++at;
    }
    if (at < header.size() && header[at] == '"') {
      for (++at; at < header.size() && header[at] != '"'; ++at) {
        if (header[at] == '\\' && at + 1 < header.size()) {
          ++at;
        }
        parameter.value += header[at];
      }
      at = header.find(';', at);
    }
    else {
      const auto end = header.find(';', at);
      parameter.value = text::trim(header.substr(at, end - at));
      at = end;
    }
    result.push_back(std::move(parameter));
  }
  return result;
}

std::string asUtf8(std::string value)
{
  return text::isUtf8(value) ? value : text::latin1ToUtf8(value);
}

// RFC 5987: charset'language'percent-encoded-value.
std::optional<std::string> extendedValue(std::string_view value)
{
  const auto first = value.find('\'');
  const auto second = first == std::string_view::npos
                          ? std::string_view::npos
                          : value.find('\'', first + 1);
  if (second == std::string_view::npos) {
    return std::nullopt;
  }
  const auto charset = text::lower(value.substr(0, first));
  auto decoded = text::percentDecode(value.substr(second + 1));
  if (charset == "utf-8") {
    return text::isUtf8(decoded) ? std::optional(decoded) : std::nullopt;
  }
  if (charset == "iso-8859-1") {
    return text::latin1ToUtf8(decoded);
  }
  return std::nullopt;
}

// =?charset?B|Q?text?= as some servers send instead of filename*.
std::optional<std::string> encodedWord(std::string_view value)
{
  if (!value.starts_with("=?") || !value.ends_with("?=")) {
    return std::nullopt;
  }
  const auto body = value.substr(2, value.size() - 4);
  const auto parts = text::split(body, '?', true);
  if (parts.size() != 3) {
    return std::nullopt;
  }
  std::optional<std::string> decoded;
  if (text::iequals(parts[1], "b")) {
    decoded = text::base64Decode(parts[2]);
  }
  else if (text::iequals(parts[1], "q")) {
    auto quoted = text::replaceAll(parts[2], "_", " ");
    decoded = text::percentDecode(text::replaceAll(quoted, "=", "%"));
  }
  if (!decoded) {
    return std::nullopt;
  }
  const auto charset = text::lower(parts[0]);
  if (charset == "iso-8859-1") {
    return text::latin1ToUtf8(*decoded);
  }
  return charset == "utf-8" && text::isUtf8(*decoded) ? decoded : std::nullopt;
}

std::string plainValue(const std::string& value)
{
  if (auto word = encodedWord(value)) {
    return *word;
  }
  // RFC 6266 does not percent-decode filename=, but many servers send
  // percent-encoded UTF-8 there. Only that case is decoded; ASCII escapes
  // such as "%20" are kept as the server wrote them.
  if (value.find('%') != std::string::npos) {
    auto decoded = text::percentDecode(value);
    const bool multibyte =
        std::any_of(decoded.begin(), decoded.end(), [](char c) {
          return static_cast<unsigned char>(c) >= 0x80;
        });
    if (multibyte && text::isUtf8(decoded)) {
      return decoded;
    }
  }
  return asUtf8(value);
}

std::string baseName(std::string name)
{
  const auto slash = name.find_last_of("/\\");
  if (slash != std::string::npos) {
    name.erase(0, slash + 1);
  }
  return name == "." || name == ".." ? std::string() : name;
}

} // namespace

std::string contentDispositionFileName(std::string_view header)
{
  std::string plain;
  for (const auto& parameter : parameters(header)) {
    if (parameter.name == "filename*") {
      if (auto value = extendedValue(parameter.value);
          value && !value->empty()) {
        return baseName(*value);
      }
    }
    else if (parameter.name == "filename" && plain.empty()) {
      plain = plainValue(parameter.value);
    }
  }
  return baseName(plain);
}

} // namespace aria2
