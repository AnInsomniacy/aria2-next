#include "DefaultPieceStorage.h"

#include <cppunit/extensions/HelperMacros.h>

#include "DownloadContext.h"
#include "Option.h"
#include "Piece.h"
#include "prefs.h"

namespace aria2 {

class StorageTruthTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(StorageTruthTest);
  CPPUNIT_TEST(testCompletedLengthExcludesInFlightBytes);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp()
  {
    option_.reset(new Option());
    option_->put(PREF_DIR, ".");
  }

  void testCompletedLengthExcludesInFlightBytes();

private:
  std::shared_ptr<Option> option_;
};

CPPUNIT_TEST_SUITE_REGISTRATION(StorageTruthTest);

void StorageTruthTest::testCompletedLengthExcludesInFlightBytes()
{
  auto dctx = std::make_shared<DownloadContext>(1_m, 256_m);
  DefaultPieceStorage ps(dctx, option_.get());
  ps.markPiecesDone(250_m);

  std::vector<std::shared_ptr<Piece>> inFlightPieces;
  for (int i = 0; i < 2; ++i) {
    auto p = std::make_shared<Piece>(250 + i, 1_m);
    for (int j = 0; j < 32; ++j) {
      p->completeBlock(j);
    }
    inFlightPieces.push_back(p);
  }
  ps.addInFlightPiece(inFlightPieces);

  CPPUNIT_ASSERT_EQUAL((int64_t)250_m, ps.getCompletedLength());
  CPPUNIT_ASSERT_EQUAL((int64_t)1_m, ps.getInFlightCompletedLength());
}

} // namespace aria2
