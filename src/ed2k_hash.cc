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
#include "ed2k_hash.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "DlAbortEx.h"
#include "MessageDigest.h"
#include "message_digest_helper.h"

namespace aria2 {

namespace ed2k {

namespace {

constexpr uint32_t MD4_INIT_A = 0x67452301u;
constexpr uint32_t MD4_INIT_B = 0xefcdab89u;
constexpr uint32_t MD4_INIT_C = 0x98badcfeu;
constexpr uint32_t MD4_INIT_D = 0x10325476u;

uint32_t rol(uint32_t n, uint32_t s) { return (n << s) | (n >> (32 - s)); }

uint32_t f(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }

uint32_t g(uint32_t x, uint32_t y, uint32_t z)
{
  return (x & y) | (x & z) | (y & z);
}

uint32_t h(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }

void step1(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
           uint32_t s)
{
  a = rol(a + f(b, c, d) + x, s);
}

void step2(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
           uint32_t s)
{
  a = rol(a + g(b, c, d) + x + 0x5a827999u, s);
}

void step3(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
           uint32_t s)
{
  a = rol(a + h(b, c, d) + x + 0x6ed9eba1u, s);
}

uint32_t loadLe32(const unsigned char* p)
{
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

void storeLe32(unsigned char* p, uint32_t n)
{
  p[0] = static_cast<unsigned char>(n);
  p[1] = static_cast<unsigned char>(n >> 8);
  p[2] = static_cast<unsigned char>(n >> 16);
  p[3] = static_cast<unsigned char>(n >> 24);
}

void transform(uint32_t state[4], const unsigned char block[64])
{
  uint32_t x[16];
  for (size_t i = 0; i < 16; ++i) {
    x[i] = loadLe32(block + i * 4);
  }

  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];

  step1(a, b, c, d, x[0], 3);
  step1(d, a, b, c, x[1], 7);
  step1(c, d, a, b, x[2], 11);
  step1(b, c, d, a, x[3], 19);
  step1(a, b, c, d, x[4], 3);
  step1(d, a, b, c, x[5], 7);
  step1(c, d, a, b, x[6], 11);
  step1(b, c, d, a, x[7], 19);
  step1(a, b, c, d, x[8], 3);
  step1(d, a, b, c, x[9], 7);
  step1(c, d, a, b, x[10], 11);
  step1(b, c, d, a, x[11], 19);
  step1(a, b, c, d, x[12], 3);
  step1(d, a, b, c, x[13], 7);
  step1(c, d, a, b, x[14], 11);
  step1(b, c, d, a, x[15], 19);

  step2(a, b, c, d, x[0], 3);
  step2(d, a, b, c, x[4], 5);
  step2(c, d, a, b, x[8], 9);
  step2(b, c, d, a, x[12], 13);
  step2(a, b, c, d, x[1], 3);
  step2(d, a, b, c, x[5], 5);
  step2(c, d, a, b, x[9], 9);
  step2(b, c, d, a, x[13], 13);
  step2(a, b, c, d, x[2], 3);
  step2(d, a, b, c, x[6], 5);
  step2(c, d, a, b, x[10], 9);
  step2(b, c, d, a, x[14], 13);
  step2(a, b, c, d, x[3], 3);
  step2(d, a, b, c, x[7], 5);
  step2(c, d, a, b, x[11], 9);
  step2(b, c, d, a, x[15], 13);

  step3(a, b, c, d, x[0], 3);
  step3(d, a, b, c, x[8], 9);
  step3(c, d, a, b, x[4], 11);
  step3(b, c, d, a, x[12], 15);
  step3(a, b, c, d, x[2], 3);
  step3(d, a, b, c, x[10], 9);
  step3(c, d, a, b, x[6], 11);
  step3(b, c, d, a, x[14], 15);
  step3(a, b, c, d, x[1], 3);
  step3(d, a, b, c, x[9], 9);
  step3(c, d, a, b, x[5], 11);
  step3(b, c, d, a, x[13], 15);
  step3(a, b, c, d, x[3], 3);
  step3(d, a, b, c, x[11], 9);
  step3(c, d, a, b, x[7], 11);
  step3(b, c, d, a, x[15], 15);

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

void validateAichHashLength(const std::string& hash)
{
  if (hash.size() != AICH_HASH_LENGTH) {
    throw DL_ABORT_EX("Bad ED2K AICH hash length.");
  }
}

std::string aichRootHashForRange(const char* data, size_t length,
                                 size_t baseSize, bool leftBranch)
{
  if (length <= baseSize) {
    return aichHash(data, length);
  }
  const auto blockCount = (length + baseSize - 1) / baseSize;
  const auto leftBlocks = (leftBranch ? blockCount + 1 : blockCount) / 2;
  const auto leftLength = std::min(length, leftBlocks * baseSize);
  const auto rightLength = length - leftLength;
  const auto leftBase =
      leftLength <= PIECE_LENGTH ? EMBLOCK_LENGTH : PIECE_LENGTH;
  const auto rightBase =
      rightLength <= PIECE_LENGTH ? EMBLOCK_LENGTH : PIECE_LENGTH;
  return aichHash(aichRootHashForRange(data, leftLength, leftBase, true) +
                  aichRootHashForRange(data + leftLength, rightLength,
                                       rightBase, false));
}

} // namespace

std::string md4Digest(const void* data, size_t length)
{
  uint32_t state[4] = {MD4_INIT_A, MD4_INIT_B, MD4_INIT_C, MD4_INIT_D};
  auto bytes = static_cast<const unsigned char*>(data);
  const uint64_t totalLength = length;

  while (length >= 64) {
    transform(state, bytes);
    bytes += 64;
    length -= 64;
  }

  std::array<unsigned char, 128> tail;
  tail.fill(0);
  if (length) {
    std::memcpy(tail.data(), bytes, length);
  }
  tail[length] = 0x80;
  const uint64_t bitLength = totalLength * 8;
  const size_t lengthOffset = length < 56 ? 56 : 120;
  for (size_t i = 0; i < 8; ++i) {
    tail[lengthOffset + i] = static_cast<unsigned char>(bitLength >> (i * 8));
  }

  transform(state, tail.data());
  if (length >= 56) {
    transform(state, tail.data() + 64);
  }

  unsigned char digest[HASH_LENGTH];
  for (size_t i = 0; i < 4; ++i) {
    storeLe32(digest + i * 4, state[i]);
  }
  return std::string(&digest[0], &digest[HASH_LENGTH]);
}

std::string md4Digest(const std::string& data)
{
  return md4Digest(data.data(), data.size());
}

std::string rootHash(const std::vector<std::string>& pieceHashes)
{
  if (pieceHashes.empty()) {
    return md4Digest("");
  }
  if (pieceHashes.size() == 1) {
    return pieceHashes[0];
  }
  std::string concat;
  concat.reserve(pieceHashes.size() * HASH_LENGTH);
  for (const auto& hash : pieceHashes) {
    if (hash.size() != HASH_LENGTH) {
      throw DL_ABORT_EX("Bad ED2K piece hash length.");
    }
    concat += hash;
  }
  return md4Digest(concat);
}

std::string aichHash(const void* data, size_t length)
{
  unsigned char digest[AICH_HASH_LENGTH];
  auto sha1 = MessageDigest::sha1();
  message_digest::digest(digest, sizeof(digest), sha1.get(), data, length);
  return std::string(&digest[0], &digest[AICH_HASH_LENGTH]);
}

std::string aichHash(const std::string& data)
{
  return aichHash(data.data(), data.size());
}

std::string aichRootHash(const void* data, size_t length)
{
  if (length == 0) {
    return aichHash("", 0);
  }
  const auto baseSize = length <= PIECE_LENGTH ? EMBLOCK_LENGTH : PIECE_LENGTH;
  return aichRootHashForRange(static_cast<const char*>(data), length, baseSize,
                              true);
}

std::string aichRootHash(const std::vector<std::string>& hashes)
{
  if (hashes.empty()) {
    return aichHash("", 0);
  }
  if (hashes.size() == 1) {
    validateAichHashLength(hashes[0]);
    return hashes[0];
  }
  std::vector<std::string> level = hashes;
  for (const auto& hash : level) {
    validateAichHashLength(hash);
  }
  while (level.size() > 1) {
    std::vector<std::string> next;
    next.reserve((level.size() + 1) / 2);
    for (size_t i = 0; i < level.size(); i += 2) {
      if (i + 1 == level.size()) {
        next.push_back(level[i]);
      }
      else {
        next.push_back(aichHash(level[i] + level[i + 1]));
      }
    }
    level.swap(next);
  }
  return level[0];
}

} // namespace ed2k

} // namespace aria2
