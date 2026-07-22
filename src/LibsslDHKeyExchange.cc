/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2010 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "LibsslDHKeyExchange.h"

#include <openssl/err.h>
#include <openssl/rand.h>

#include "DlAbortEx.h"
#include "fmt.h"

namespace aria2 {

namespace {
constexpr char MSE_PRIME_HEX[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B"
    "139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485"
    "B576625E7EC6F44C42E9A63A36210000000000090563";

void handleError(const std::string& funName)
{
  throw DL_ABORT_EX(
      fmt("Exception in OpenSSL MSE DH routine %s: %s",
          funName.c_str(), ERR_error_string(ERR_get_error(), nullptr)));
}
} // namespace

LibsslDHKeyExchange::LibsslDHKeyExchange()
    : bnCtx_(nullptr),
      prime_(nullptr),
      generator_(nullptr),
      privateKey_(nullptr),
      publicKey_(nullptr)
{
  try {
    initialize(nullptr);
  }
  catch (...) {
    clear();
    throw;
  }
}

LibsslDHKeyExchange::LibsslDHKeyExchange(
    const MSEDHPrivateKey& privateKey)
    : bnCtx_(nullptr),
      prime_(nullptr),
      generator_(nullptr),
      privateKey_(nullptr),
      publicKey_(nullptr)
{
  try {
    initialize(&privateKey);
  }
  catch (...) {
    clear();
    throw;
  }
}

LibsslDHKeyExchange::~LibsslDHKeyExchange()
{
  clear();
}

void LibsslDHKeyExchange::clear() noexcept
{
  BN_CTX_free(bnCtx_);
  BN_free(prime_);
  BN_free(generator_);
  BN_free(privateKey_);
  BN_free(publicKey_);
  bnCtx_ = nullptr;
  prime_ = nullptr;
  generator_ = nullptr;
  privateKey_ = nullptr;
  publicKey_ = nullptr;
}

void LibsslDHKeyExchange::initialize(const MSEDHPrivateKey* privateKey)
{
  bnCtx_ = BN_CTX_new();
  if (!bnCtx_) {
    handleError("BN_CTX_new");
  }

  if (BN_hex2bn(&prime_, MSE_PRIME_HEX) == 0) {
    handleError("BN_hex2bn");
  }
  generator_ = BN_new();
  if (!generator_ || BN_set_word(generator_, 2) != 1) {
    handleError("BN_set_word");
  }

  if (privateKey) {
    privateKey_ =
        BN_bin2bn(privateKey->data(), privateKey->size(), nullptr);
    if (!privateKey_ || BN_is_zero(privateKey_)) {
      throw DL_ABORT_EX("MSE DH private key must be nonzero");
    }
  }
  else {
    privateKey_ = BN_new();
    if (!privateKey_ ||
        BN_rand(privateKey_, MSE_DH_PRIVATE_KEY_LENGTH * 8,
                BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) != 1) {
      handleError("BN_rand");
    }
  }

  publicKey_ = BN_new();
  if (!publicKey_ ||
      BN_mod_exp(publicKey_, generator_, privateKey_, prime_, bnCtx_) != 1) {
    handleError("BN_mod_exp");
  }
  publicKeyBytes_ = exportNumber(publicKey_);
}

MSEDHPublicKey LibsslDHKeyExchange::exportNumber(const BIGNUM* number) const
{
  MSEDHPublicKey result{};
  if (BN_bn2binpad(number, result.data(), result.size()) !=
      static_cast<int>(result.size())) {
    handleError("BN_bn2binpad");
  }
  return result;
}

MSEDHPublicKey LibsslDHKeyExchange::computeSecret(
    const MSEDHPublicKey& peerPublicKey) const
{
  BIGNUM* peerNumber =
      BN_bin2bn(peerPublicKey.data(), peerPublicKey.size(), nullptr);
  BIGNUM* upperBound = BN_dup(prime_);
  if (!peerNumber || !upperBound || BN_sub_word(upperBound, 1) != 1) {
    BN_free(peerNumber);
    BN_free(upperBound);
    handleError("peer key import");
  }
  if (BN_cmp(peerNumber, BN_value_one()) <= 0 ||
      BN_cmp(peerNumber, upperBound) >= 0) {
    BN_free(peerNumber);
    BN_free(upperBound);
    throw DL_ABORT_EX("Invalid MSE DH peer public key");
  }
  BN_free(upperBound);

  BIGNUM* secret = BN_new();
  if (!secret ||
      BN_mod_exp(secret, peerNumber, privateKey_, prime_, bnCtx_) != 1) {
    BN_free(peerNumber);
    BN_free(secret);
    handleError("BN_mod_exp");
  }
  BN_free(peerNumber);

  const auto result = exportNumber(secret);
  BN_free(secret);
  return result;
}

} // namespace aria2
