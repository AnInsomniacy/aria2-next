#include "Signature.h"

#include <fstream>

#include "a2doctest.h"

#include "Exception.h"
#include "File.h"

namespace aria2 {

class SignatureTest {


public:
  void setUp() {}

  void tearDown() {}

  void testSave();
};

A2_TEST(SignatureTest, testSave)

void SignatureTest::testSave()
{
  Signature sig;
  sig.setBody("SIGNATURE");
  std::string filepath = A2_TEST_OUT_DIR "/aria2_SignatureTest_testSave";
  File outfile(filepath);
  if (outfile.exists()) {
    outfile.remove();
  }
  REQUIRE(sig.save(filepath));
  {
    std::ifstream in(filepath.c_str(), std::ios::binary);
    std::string fileContent;
    in >> fileContent;
    REQUIRE_EQ(sig.getBody(), fileContent);
  }
  // second attempt to save will fail because file already exists.
  REQUIRE(!sig.save(filepath));
}

} // namespace aria2
