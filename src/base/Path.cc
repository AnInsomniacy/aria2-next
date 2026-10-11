/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Path.h"

#include "base/Text.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace aria2 {

fs::path toPath(std::string_view utf8)
{
  return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string toUtf8(const fs::path& path)
{
  const auto text = path.generic_u8string();
  return std::string(text.begin(), text.end());
}

std::string joinPath(std::string_view directory, std::string_view name)
{
  const auto path = toPath(name);
  if (path.is_absolute() || path.has_root_name() || directory.empty()) {
    return toUtf8(path);
  }
  return toUtf8(toPath(directory) / path);
}

std::string safeFileName(std::string_view input)
{
  std::string name;
  for (const char ch : input) {
    const auto c = static_cast<unsigned char>(ch);
    const bool reserved = c < 0x20 || c == 0x7f || ch == '/' || ch == '\\' ||
                          ch == ':' || ch == '*' || ch == '?' || ch == '"' ||
                          ch == '<' || ch == '>' || ch == '|';
    name += reserved ? '_' : ch;
  }
  if (!text::isUtf8(name)) {
    name = text::latin1ToUtf8(name);
  }
  while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
    name.pop_back();
  }
  while (!name.empty() && name.front() == ' ') {
    name.erase(0, 1);
  }
  if (name.empty()) {
    return {};
  }
  const auto stem = text::lower(name.substr(0, name.find('.')));
  const bool device = stem == "con" || stem == "prn" || stem == "aux" ||
                      stem == "nul" ||
                      (stem.size() == 4 &&
                       (stem.starts_with("com") || stem.starts_with("lpt")) &&
                       stem[3] >= '1' && stem[3] <= '9');
  if (device) {
    name.insert(0, "_");
  }
  constexpr size_t limit = 240;
  if (name.size() > limit) {
    const auto dot = name.rfind('.');
    std::string extension = dot != std::string::npos && name.size() - dot < 32
                                ? name.substr(dot)
                                : "";
    name.resize(limit - extension.size());
    while (!name.empty() && !text::isUtf8(name)) {
      name.pop_back();
    }
    name += extension;
  }
  return name;
}

bool samePath(std::string_view a, std::string_view b)
{
  auto key = [](std::string_view path) {
    auto text = toUtf8(toPath(path).lexically_normal());
#ifdef _WIN32
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
#endif
    return text;
  };
  return key(a) == key(b);
}

bool isContainedRelativePath(std::string_view value)
{
  const auto path = toPath(value);
  if (value.empty() || path.is_absolute() || path.has_root_name() ||
      path.has_root_directory()) {
    return false;
  }
  for (const auto& part : path) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

std::string homeDirectory()
{
#ifdef _WIN32
  for (const auto* name : {L"HOME", L"USERPROFILE"}) {
    if (const auto* value = _wgetenv(name); value && *value) {
      return toUtf8(fs::path(value));
    }
  }
  return {};
#else
  const auto* value = std::getenv("HOME");
  return value ? value : "";
#endif
}

std::string defaultConfigPath()
{
  const auto* xdg = std::getenv("XDG_CONFIG_HOME");
  const std::string base = xdg && fs::path(xdg).is_absolute()
                               ? std::string(xdg)
                               : homeDirectory() + "/.config";
  return base + "/aria2/aria2.conf";
}

bool writeFileAtomically(const std::string& path, std::string_view data)
{
  const auto target = toPath(path);
  auto temporary = target;
  temporary += ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out.write(data.data(), static_cast<std::streamsize>(data.size()))) {
      return false;
    }
  }
  std::error_code error;
  fs::rename(temporary, target, error);
  return !error;
}

std::optional<std::string> readFile(const std::string& path)
{
  std::ifstream in(toPath(path), std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  return std::string(std::istreambuf_iterator<char>(in), {});
}

} // namespace aria2
