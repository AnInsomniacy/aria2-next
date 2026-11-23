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

namespace {

bool isInvalidFileNameCharacter(unsigned char c)
{
  return c < 0x20 || c == 0x7f || c == '"' || c == '*' || c == ':' ||
         c == '<' || c == '>' || c == '?' || c == '|';
}

size_t codePointLength(unsigned char lead)
{
  if ((lead & 0x80) == 0) {
    return 1;
  }
  if ((lead & 0xe0) == 0xc0) {
    return 2;
  }
  if ((lead & 0xf0) == 0xe0) {
    return 3;
  }
  if ((lead & 0xf8) == 0xf0) {
    return 4;
  }
  return 1;
}

size_t prefixBytes(std::string_view text, size_t characters)
{
  size_t bytes = 0;
  while (bytes < text.size() && characters > 0) {
    bytes += std::min(codePointLength(static_cast<unsigned char>(text[bytes])),
                      text.size() - bytes);
    --characters;
  }
  return bytes;
}

size_t suffixBytes(std::string_view text, size_t characters)
{
  size_t bytes = text.size();
  while (bytes > 0 && characters > 0) {
    --bytes;
    while (bytes > 0 &&
           (static_cast<unsigned char>(text[bytes]) & 0xc0) == 0x80) {
      --bytes;
    }
    --characters;
  }
  return text.size() - bytes;
}

size_t characterCount(std::string_view text)
{
  size_t count = 0;
  for (size_t bytes = 0; bytes < text.size();) {
    bytes += codePointLength(static_cast<unsigned char>(text[bytes]));
    ++count;
  }
  return count;
}

// The pre-policy shortening: keep the beginning up to the byte limit and
// append the extension when it fits, so an established name changes as little
// as possible. A limit too small for the extension cuts the name instead.
std::string shortenKeepingExtension(std::string_view name, size_t limit)
{
  const auto dot = name.rfind('.');
  const bool hasExtension =
      dot != std::string_view::npos && name.size() - dot < 32;
  if (!hasExtension || limit <= name.size() - dot) {
    auto shortened = std::string(name.substr(0, limit));
    while (!shortened.empty() && !text::isUtf8(shortened)) {
      shortened.pop_back();
    }
    return shortened;
  }
  const auto extension = name.substr(dot);
  auto shortened = std::string(name.substr(0, limit - extension.size()));
  while (!shortened.empty() && !text::isUtf8(shortened)) {
    shortened.pop_back();
  }
  shortened += extension;
  return shortened;
}

// Keeps the first and last characters of an over-long name so the start stays
// readable and the extension survives. The two ends never overlap, and the
// tail gives way only after the head, which keeps the extension.
std::string shortenToHeadAndTail(std::string_view name,
                                 const FileNamePolicy& policy)
{
  const auto characters = characterCount(name);
  auto head = std::min(policy.headCharacters, characters);
  auto tail = std::min(policy.tailCharacters, characters);
  if (head + tail >= characters) {
    // The ends would overlap; keep at least one character out. The tail is
    // what carries the extension, so the head gives way first.
    tail = std::min(tail, characters - 1);
    head = std::min(head, characters - tail - 1);
  }
  std::string headText(name.substr(0, prefixBytes(name, head)));
  std::string tailText(name.substr(name.size() - suffixBytes(name, tail)));
  std::string result = headText + '_' + tailText;
  while (result.size() > policy.maxBytes) {
    if (!headText.empty()) {
      headText.resize(headText.size() - suffixBytes(headText, 1));
    }
    else if (!tailText.empty()) {
      tailText.erase(0, prefixBytes(tailText, 1));
    }
    else {
      result = "_";
      break;
    }
    result = headText + '_' + tailText;
  }
  return result;
}

} // namespace

std::string sanitizeFileName(std::string_view input,
                             const FileNamePolicy& policy)
{
  std::string name;
  name.reserve(input.size());
  for (const char ch : input) {
    const auto byte = static_cast<unsigned char>(ch);
    if (ch == '/' || ch == '\\' || byte == 0) {
      // A derived name is one component and must never leave its directory.
      name += '_';
    }
    else if (isInvalidFileNameCharacter(byte)) {
      switch (policy.invalidCharacters) {
      case InvalidFileNameCharacters::Keep:
        name += ch;
        break;
      case InvalidFileNameCharacters::Delete:
        break;
      case InvalidFileNameCharacters::Replace:
        name += '_';
        break;
      }
    }
    else {
      name += ch;
    }
  }
  if (!text::isUtf8(name)) {
    name = text::latin1ToUtf8(name);
  }
  // Windows drops trailing dots and spaces, so a name made of them names no
  // file; a name that only shrinks to nothing is unusable.
  while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
    name.pop_back();
  }
  while (!name.empty() && name.front() == ' ') {
    name.erase(0, 1);
  }
  if (name.empty()) {
    return {};
  }
  // A root name such as "C:" would make joinPath treat the name as a drive
  // path. Prefixing keeps it a component even when ':' is kept.
  if (toPath(name).has_root_name()) {
    name.insert(0, 1, '_');
  }
  const auto stem = text::lower(name.substr(0, name.find('.')));
  const bool device = stem == "con" || stem == "prn" || stem == "aux" ||
                      stem == "nul" ||
                      (stem.size() == 4 &&
                       (stem.starts_with("com") || stem.starts_with("lpt")) &&
                       stem[3] >= '1' && stem[3] <= '9');
  if (device) {
    name.insert(0, 1, '_');
  }
  if (policy.maxBytes > 0 && name.size() > policy.maxBytes) {
    name = policy.truncation == FileNameTruncation::HeadTail
               ? shortenToHeadAndTail(name, policy)
               : shortenKeepingExtension(name, policy.maxBytes);
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

bool volumeAvailable(std::string_view value)
{
#ifdef _WIN32
  auto wide = toPath(value).make_preferred().wstring();
  if (wide.rfind(L"\\\\?\\UNC\\", 0) == 0)
    wide = L"\\\\" + wide.substr(8);
  else if (wide.rfind(L"\\\\?\\", 0) == 0)
    wide = wide.substr(4);
  std::wstring root;
  if (wide.size() >= 2 && wide[1] == L':') {
    root = wide.substr(0, 2) + L"\\";
  }
  else if (wide.rfind(L"\\\\", 0) == 0) {
    // \\server\share\: the share, not the server, is what can be missing.
    const auto server = wide.find(L'\\', 2);
    if (server == std::wstring::npos)
      return true;
    const auto share = wide.find(L'\\', server + 1);
    root = (share == std::wstring::npos ? wide : wide.substr(0, share)) + L"\\";
  }
  else {
    return true;
  }
  return GetFileAttributesW(root.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
  (void)value;
  return true;
#endif
}

std::string missingVolumeMessage(std::string_view path)
{
  if (volumeAvailable(path))
    return {};
  return "its drive or network share is not available; a drive mapped in "
         "another session, such as one mapped without administrator rights "
         "while the engine runs with them, cannot be seen, so use its "
         "network path (\\\\server\\share) instead";
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
