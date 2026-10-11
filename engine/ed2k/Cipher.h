/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace aria2::ed2k {

// RC4 as used by eMule protocol obfuscation. OpenSSL 3 confines RC4 and MD4
// to its legacy provider, so both are implemented here.
class Rc4 {
public:
  void init(const unsigned char* key, size_t length);
  void encrypt(size_t length, unsigned char* out, const unsigned char* in);
  void apply(std::string& data)
  {
    auto* bytes = reinterpret_cast<unsigned char*>(data.data());
    encrypt(data.size(), bytes, bytes);
  }

private:
  std::array<uint8_t, 256> state_{};
  uint8_t i_ = 0;
  uint8_t j_ = 0;
};

std::string md5Digest(std::string_view data);
std::string sha1Digest(std::string_view data);

} // namespace aria2::ed2k
