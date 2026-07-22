#include "DHKeyExchange.h"
#include "InternalDHKeyExchange.h"
#ifdef HAVE_OPENSSL
#  include "LibsslDHKeyExchange.h"
#endif

#include <algorithm>
#include <string>

#include "a2doctest.h"
#include "util.h"

namespace aria2 {

namespace {

constexpr char PRIVATE_A_HEX[] =
    "0102030405060708090A0B0C0D0E0F1011121314";
constexpr char PRIVATE_B_HEX[] =
    "15161718191A1B1C1D1E1F202122232425262728";
constexpr char PUBLIC_A_HEX[] =
    "96e112dab29e8c5272accb9b17b26887ce54a144a4e3b697c7d159b7a817e556b0918db2"
    "b4c658e02a87f7e5fb14b18a553e084cbf3dad2d30f16596ccb982d406258c61b30c5c1d"
    "ae2ddc60bdbd48d79896312aad63238c39e1a633821eb693";
constexpr char SHARED_SECRET_HEX[] =
    "994aac6c359990cf4f678a1742b587eb1a5248ec7fcc0d0bcfcb12d2461bc1fe25417b708"
    "69697d9ca884832f1c5f2a2fd3318c22a5a6ba170d36aac91405457c1e8137b1534a77686"
    "5ed353f12422ff6afc58435f8bd443f61dd051a37bcdeb";

MSEDHPrivateKey privateKey(const char* hex)
{
  const std::string encoded(hex);
  const auto bytes = util::fromHex(encoded.begin(), encoded.end());
  REQUIRE_EQ(bytes.size(), MSE_DH_PRIVATE_KEY_LENGTH);
  MSEDHPrivateKey result;
  std::copy(bytes.begin(), bytes.end(), result.begin());
  return result;
}

template <typename Left, typename Right>
void requireSharedSecret(Left& left, Right& right)
{
  const auto leftSecret = left.computeSecret(right.getPublicKey());
  const auto rightSecret = right.computeSecret(left.getPublicKey());
  REQUIRE_EQ(util::toHex(leftSecret.data(), leftSecret.size()),
             SHARED_SECRET_HEX);
  REQUIRE_EQ(util::toHex(rightSecret.data(), rightSecret.size()),
             SHARED_SECRET_HEX);
}

} // namespace

class DHKeyExchangeTest {
public:
  void setUp() {}
  void tearDown() {}
  void testKnownAnswer();
  void testSelectedBackendHandshake();
#ifdef HAVE_OPENSSL
  void testInternalToOpenSSL();
  void testOpenSSLToInternal();
#endif
};

A2_TEST(DHKeyExchangeTest, testKnownAnswer)

void DHKeyExchangeTest::testKnownAnswer()
{
  DHKeyExchange exchange(privateKey(PRIVATE_A_HEX));
  const auto& publicKey = exchange.getPublicKey();
  REQUIRE_EQ(util::toHex(publicKey.data(), publicKey.size()), PUBLIC_A_HEX);
}

A2_TEST(DHKeyExchangeTest, testSelectedBackendHandshake)

void DHKeyExchangeTest::testSelectedBackendHandshake()
{
  DHKeyExchange left(privateKey(PRIVATE_A_HEX));
  DHKeyExchange right(privateKey(PRIVATE_B_HEX));
  requireSharedSecret(left, right);
}

#ifdef HAVE_OPENSSL
A2_TEST(DHKeyExchangeTest, testInternalToOpenSSL)

void DHKeyExchangeTest::testInternalToOpenSSL()
{
  InternalDHKeyExchange internal(privateKey(PRIVATE_A_HEX));
  LibsslDHKeyExchange openssl(privateKey(PRIVATE_B_HEX));
  requireSharedSecret(internal, openssl);
}

A2_TEST(DHKeyExchangeTest, testOpenSSLToInternal)

void DHKeyExchangeTest::testOpenSSLToInternal()
{
  LibsslDHKeyExchange openssl(privateKey(PRIVATE_A_HEX));
  InternalDHKeyExchange internal(privateKey(PRIVATE_B_HEX));
  requireSharedSecret(openssl, internal);
}
#endif

} // namespace aria2
