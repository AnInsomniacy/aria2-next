/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aria2::http {

// One <file> of a Metalink 4 document (RFC 5854).
struct MetalinkFile {
  std::string name; // relative path
  int64_t size = -1;
  std::string checksum; // strongest whole-file hash as "type=hex", or empty
  std::vector<std::string> urls; // by priority, most preferred first
};

// Throws Error(MetalinkParse) for malformed documents, Metalink 3 and
// documents without a usable file.
std::vector<MetalinkFile> parseMetalink(std::string_view document);

// Whether a download is a Metalink document by name or Content-Type.
bool isMetalink(std::string_view path, std::string_view contentType);

} // namespace aria2::http
