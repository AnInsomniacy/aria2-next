/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "disk/FileName.h"

#include "options/Options.h"

#include <algorithm>
#include <cstdint>

namespace aria2 {

FileNamePolicy fileNamePolicy(const Options& options)
{
  FileNamePolicy policy;
  const auto invalid = options.get("filename-invalid-chars");
  if (invalid == "delete") {
    policy.invalidCharacters = InvalidFileNameCharacters::Delete;
  }
  else if (invalid == "replace") {
    policy.invalidCharacters = InvalidFileNameCharacters::Replace;
  }
  if (options.get("filename-truncate-style") == "head-tail") {
    policy.truncation = FileNameTruncation::HeadTail;
  }
  policy.maxBytes = static_cast<size_t>(
      std::max<int64_t>(0, options.integer("filename-max-length")));
  policy.headCharacters = static_cast<size_t>(
      std::max<int64_t>(0, options.integer("filename-truncate-head")));
  policy.tailCharacters = static_cast<size_t>(
      std::max<int64_t>(0, options.integer("filename-truncate-tail")));
  return policy;
}

std::string sanitizeFileName(std::string_view name, const Options& options)
{
  return sanitizeFileName(name, fileNamePolicy(options));
}

} // namespace aria2
