/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace aria2 {

namespace fs = std::filesystem;

// The engine stores paths as UTF-8 strings with '/' separators; these convert
// at the operating-system boundary.
fs::path toPath(std::string_view utf8);
std::string toUtf8(const fs::path& path);

// Joins a download directory and a relative output name. Absolute names are
// kept as given.
std::string joinPath(std::string_view directory, std::string_view name);

// Turns untrusted text into one safe path component: no separators, control
// characters, reserved Windows names or trailing dots; at most 240 bytes with
// the extension preserved. Returns empty for names that cannot be used.
std::string safeFileName(std::string_view name);

// Compares two paths for the same file: normalized, and case-insensitive on
// Windows.
bool samePath(std::string_view a, std::string_view b);

// Rejects relative paths that escape their base ("..", absolute, drive).
bool isContainedRelativePath(std::string_view path);

// Whether the drive or network share holding an absolute path is there. A
// drive mapped in another logon session, such as a non-elevated one when the
// engine runs elevated, is not; nor is a disconnected share.
bool volumeAvailable(std::string_view path);

// Explains why `path` could not be created or opened when its drive or share
// is missing, which the system reports only as an invalid argument; empty
// otherwise.
std::string missingVolumeMessage(std::string_view path);

std::string homeDirectory();
// $XDG_CONFIG_HOME/aria2/aria2.conf, or ~/.config/aria2/aria2.conf.
std::string defaultConfigPath();

// Writes through a temporary file and renames it over the target.
bool writeFileAtomically(const std::string& path, std::string_view data);
std::optional<std::string> readFile(const std::string& path);

} // namespace aria2
