/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_ED2K_HASH_H
#define D_ED2K_HASH_H

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

namespace ed2k {

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

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_HASH_H
