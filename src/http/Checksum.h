/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <functional>
#include <optional>
#include <string>

namespace aria2 {
class DiskPool;
}

namespace aria2::http {

// Checks `path` against "TYPE=DIGEST" (sha-1, sha-224, sha-256, sha-384,
// sha-512, md5; hex or base64 digest). `done` receives an error description
// on mismatch or failure.
void verifyChecksum(DiskPool& pool, const std::string& path,
                    const std::string& expected,
                    std::function<void(std::optional<std::string>)> done);

// Hex digest of a file; empty when the type is unknown or reading fails.
std::string fileDigest(const std::string& path, const std::string& type);

} // namespace aria2::http
