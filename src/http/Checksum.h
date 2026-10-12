/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace aria2 {
class DiskPool;
}

namespace aria2::http {

struct PieceHashes;

// Checks `path` against "TYPE=DIGEST" (sha-1, sha-224, sha-256, sha-384,
// sha-512, md5; hex or base64 digest). `done` receives an error description
// on mismatch or failure.
void verifyChecksum(DiskPool& pool, const std::string& path,
                    const std::string& expected,
                    std::function<void(std::optional<std::string>)> done);

// Hex digest of a file; empty when the type is unknown or reading fails.
std::string fileDigest(const std::string& path, const std::string& type);

// Indexes of the pieces whose hash does not match; nullopt when the file
// cannot be read or the type is unknown. Blocking: run it on the disk pool.
std::optional<std::vector<size_t>> badPieces(const std::string& path,
                                             const PieceHashes& pieces);

} // namespace aria2::http
