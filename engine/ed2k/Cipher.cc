/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Cipher.h"

#include <openssl/evp.h>

#include <numeric>
#include <utility>

namespace aria2::ed2k {

void Rc4::init(const unsigned char* key, size_t length)
{
  std::iota(state_.begin(), state_.end(), 0);
  uint8_t j = 0;
  for (size_t i = 0; i < state_.size(); ++i) {
    j = static_cast<uint8_t>(j + state_[i] + key[i % length]);
    std::swap(state_[i], state_[j]);
  }
  i_ = j_ = 0;
}

void Rc4::encrypt(size_t length, unsigned char* out, const unsigned char* in)
{
  for (size_t k = 0; k < length; ++k) {
    ++i_;
    j_ = static_cast<uint8_t>(j_ + state_[i_]);
    std::swap(state_[i_], state_[j_]);
    out[k] = in[k] ^ state_[static_cast<uint8_t>(state_[i_] + state_[j_])];
  }
}

namespace {
std::string digest(const EVP_MD* type, std::string_view data)
{
  unsigned char value[EVP_MAX_MD_SIZE];
  unsigned int size = 0;
  EVP_Digest(data.data(), data.size(), value, &size, type, nullptr);
  return std::string(reinterpret_cast<const char*>(value), size);
}
} // namespace

std::string md5Digest(std::string_view data) { return digest(EVP_md5(), data); }

std::string sha1Digest(std::string_view data)
{
  return digest(EVP_sha1(), data);
}

} // namespace aria2::ed2k
