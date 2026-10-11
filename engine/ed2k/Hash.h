/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aria2::ed2k {

constexpr int32_t PIECE_LENGTH = 9728000;
constexpr int32_t BLOCK_LENGTH = 184320;
constexpr int32_t EMBLOCK_LENGTH = BLOCK_LENGTH;
constexpr size_t HASH_LENGTH = 16;
constexpr size_t AICH_HASH_LENGTH = 20;

std::string md4Digest(const void* data, size_t length);
std::string md4Digest(const std::string& data);
size_t hashSetPartCount(int64_t fileSize);
std::string rootHash(const std::vector<std::string>& pieceHashes);
std::string aichHash(const void* data, size_t length);
std::string aichHash(const std::string& data);
std::string aichRootHash(const void* data, size_t length);
std::string aichRootHash(const std::vector<std::string>& hashes);

} // namespace aria2::ed2k
