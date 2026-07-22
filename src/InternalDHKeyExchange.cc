/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2013 Nils Maier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link this program with the OpenSSL library under the
 * conditions described in the individual source files.
 */
/* copyright --> */

#include "InternalDHKeyExchange.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "DlAbortEx.h"
#include "util.h"

namespace aria2 {

namespace {

constexpr size_t WORD_BITS = 32;
constexpr size_t WORD_COUNT = MSE_DH_PUBLIC_KEY_LENGTH * 8 / WORD_BITS;
constexpr size_t PRIVATE_KEY_BITS = MSE_DH_PRIVATE_KEY_LENGTH * 8;

constexpr char MSE_PRIME_HEX[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B"
    "139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485"
    "B576625E7EC6F44C42E9A63A36210000000000090563";

class UInt768 {
public:
  UInt768() = default;

  explicit UInt768(uint32_t value) { words_[0] = value; }

  static UInt768 fromBigEndian(const unsigned char* data, size_t length)
  {
    if (length > MSE_DH_PUBLIC_KEY_LENGTH) {
      throw DL_ABORT_EX("MSE DH integer exceeds 768 bits");
    }

    UInt768 result;
    for (size_t i = 0; i < length; ++i) {
      const size_t source = length - 1 - i;
      result.words_[i / 4] |=
          static_cast<uint32_t>(data[source]) << ((i % 4) * 8);
    }
    return result;
  }

  static UInt768 fromHex(const char* hex)
  {
    const std::string encoded(hex);
    const auto bytes = util::fromHex(encoded.begin(), encoded.end());
    if (bytes.size() != MSE_DH_PUBLIC_KEY_LENGTH) {
      throw DL_ABORT_EX("Invalid MSE DH prime");
    }
    return fromBigEndian(
        reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
  }

  MSEDHPublicKey toBigEndian() const
  {
    MSEDHPublicKey result{};
    for (size_t i = 0; i < result.size(); ++i) {
      result[result.size() - 1 - i] =
          static_cast<unsigned char>(words_[i / 4] >> ((i % 4) * 8));
    }
    return result;
  }

  bool bit(size_t index) const
  {
    return (words_[index / WORD_BITS] >> (index % WORD_BITS)) & 1U;
  }

  bool isZero() const
  {
    for (const auto word : words_) {
      if (word != 0) {
        return false;
      }
    }
    return true;
  }

  int compare(const UInt768& other) const
  {
    for (size_t i = WORD_COUNT; i-- > 0;) {
      if (words_[i] < other.words_[i]) {
        return -1;
      }
      if (words_[i] > other.words_[i]) {
        return 1;
      }
    }
    return 0;
  }

  UInt768 subtractOne() const
  {
    UInt768 result = *this;
    for (auto& word : result.words_) {
      if (word != 0) {
        --word;
        break;
      }
      word = UINT32_MAX;
    }
    return result;
  }

  static UInt768 modExp(UInt768 base, const UInt768& exponent,
                        size_t exponentBits, const UInt768& modulus)
  {
    UInt768 result(1);
    for (size_t bit = 0; bit < exponentBits; ++bit) {
      if (exponent.bit(bit)) {
        result = modMultiply(result, base, modulus);
      }
      if (bit + 1 < exponentBits) {
        base = modMultiply(base, base, modulus);
      }
    }
    return result;
  }

private:
  static UInt768 addMod(const UInt768& left, const UInt768& right,
                        const UInt768& modulus)
  {
    std::array<uint32_t, WORD_COUNT + 1> sum{};
    uint64_t carry = 0;
    for (size_t i = 0; i < WORD_COUNT; ++i) {
      const uint64_t value = static_cast<uint64_t>(left.words_[i]) +
                             right.words_[i] + carry;
      sum[i] = static_cast<uint32_t>(value);
      carry = value >> WORD_BITS;
    }
    sum[WORD_COUNT] = static_cast<uint32_t>(carry);

    bool reduce = sum[WORD_COUNT] != 0;
    if (!reduce) {
      UInt768 low;
      std::copy_n(sum.begin(), WORD_COUNT, low.words_.begin());
      reduce = low.compare(modulus) >= 0;
    }

    if (reduce) {
      uint64_t borrow = 0;
      for (size_t i = 0; i < WORD_COUNT; ++i) {
        const uint64_t subtrahend =
            static_cast<uint64_t>(modulus.words_[i]) + borrow;
        const uint64_t minuend = sum[i];
        sum[i] = static_cast<uint32_t>(minuend - subtrahend);
        borrow = minuend < subtrahend;
      }
      sum[WORD_COUNT] -= static_cast<uint32_t>(borrow);
    }

    UInt768 result;
    std::copy_n(sum.begin(), WORD_COUNT, result.words_.begin());
    return result;
  }

  static UInt768 modMultiply(const UInt768& left, const UInt768& right,
                             const UInt768& modulus)
  {
    UInt768 result;
    UInt768 addend = left;
    for (size_t bit = 0; bit < MSE_DH_PUBLIC_KEY_LENGTH * 8; ++bit) {
      if (right.bit(bit)) {
        result = addMod(result, addend, modulus);
      }
      if (bit + 1 < MSE_DH_PUBLIC_KEY_LENGTH * 8) {
        addend = addMod(addend, addend, modulus);
      }
    }
    return result;
  }

  std::array<uint32_t, WORD_COUNT> words_{};
};

const UInt768& msePrime()
{
  static const UInt768 prime = UInt768::fromHex(MSE_PRIME_HEX);
  return prime;
}

UInt768 privateNumber(const MSEDHPrivateKey& privateKey)
{
  const auto number = UInt768::fromBigEndian(privateKey.data(), privateKey.size());
  if (number.isZero()) {
    throw DL_ABORT_EX("MSE DH private key must be nonzero");
  }
  return number;
}

MSEDHPublicKey makePublicKey(const MSEDHPrivateKey& privateKey)
{
  return UInt768::modExp(UInt768(2), privateNumber(privateKey),
                         PRIVATE_KEY_BITS, msePrime())
      .toBigEndian();
}

} // namespace

InternalDHKeyExchange::InternalDHKeyExchange()
{
  do {
    util::generateRandomData(privateKey_.data(), privateKey_.size());
  } while (std::all_of(privateKey_.begin(), privateKey_.end(),
                       [](unsigned char byte) { return byte == 0; }));
  publicKey_ = makePublicKey(privateKey_);
}

InternalDHKeyExchange::InternalDHKeyExchange(
    const MSEDHPrivateKey& privateKey)
    : privateKey_(privateKey), publicKey_(makePublicKey(privateKey))
{
}

MSEDHPublicKey InternalDHKeyExchange::computeSecret(
    const MSEDHPublicKey& peerPublicKey) const
{
  const auto peer =
      UInt768::fromBigEndian(peerPublicKey.data(), peerPublicKey.size());
  const auto upperBound = msePrime().subtractOne();
  if (peer.compare(UInt768(1)) <= 0 || peer.compare(upperBound) >= 0) {
    throw DL_ABORT_EX("Invalid MSE DH peer public key");
  }

  return UInt768::modExp(peer, privateNumber(privateKey_), PRIVATE_KEY_BITS,
                         msePrime())
      .toBigEndian();
}

} // namespace aria2
