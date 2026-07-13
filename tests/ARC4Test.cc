#include "ARC4Encryptor.h"

#include <cstring>
#include "a2doctest.h"

#include "Exception.h"
#include "util.h"

namespace aria2 {

class ARC4Test {


public:
  void setUp() {}

  void tearDown() {}

  void testEncrypt();
};

A2_TEST(ARC4Test, testEncrypt)

void ARC4Test::testEncrypt()
{
  ARC4Encryptor enc;
  ARC4Encryptor dec;
  const size_t LEN = 20;
  unsigned char key[LEN];
  memset(key, 0, LEN);
  util::generateRandomData(key, sizeof(key));
  enc.init(key, sizeof(key));
  dec.init(key, sizeof(key));

  unsigned char encrypted[LEN];
  unsigned char decrypted[LEN];

  enc.encrypt(LEN, encrypted, key);
  REQUIRE(memcmp(key, encrypted, LEN) != 0);
  dec.encrypt(LEN, decrypted, encrypted);
  REQUIRE(memcmp(key, decrypted, LEN) == 0);

  // once more
  enc.encrypt(LEN, encrypted, key);
  REQUIRE(memcmp(key, encrypted, LEN) != 0);
  dec.encrypt(LEN, decrypted, encrypted);
  REQUIRE(memcmp(key, decrypted, LEN) == 0);
}

} // namespace aria2
