#include "ShareRatioSeedCriteria.h"

#include "a2doctest.h"

#include "DownloadContext.h"
#include "BtRuntime.h"
#include "MockPieceStorage.h"
#include "FileEntry.h"

namespace aria2 {

class ShareRatioSeedCriteriaTest {


public:
  void testEvaluate();
  void testEvaluateWithoutBtRuntime();
};

A2_TEST(ShareRatioSeedCriteriaTest, testEvaluate)
A2_TEST(ShareRatioSeedCriteriaTest, testEvaluateWithoutBtRuntime)

void ShareRatioSeedCriteriaTest::testEvaluate()
{
  std::shared_ptr<DownloadContext> dctx(new DownloadContext(1_m, 1000000));
  std::shared_ptr<BtRuntime> btRuntime(new BtRuntime());
  btRuntime->setUploadLengthAtStartup(1000000);

  std::shared_ptr<MockPieceStorage> pieceStorage(new MockPieceStorage());
  pieceStorage->setCompletedLength(1000000);

  ShareRatioSeedCriteria cri(1.0, dctx);
  cri.setBtRuntime(btRuntime);
  cri.setPieceStorage(pieceStorage);

  REQUIRE(cri.evaluate());

  cri.setRatio(2.0);
  REQUIRE(!cri.evaluate());
  // check div by zero
  dctx->getFirstFileEntry()->setLength(0);
  REQUIRE(!cri.evaluate());
}

void ShareRatioSeedCriteriaTest::testEvaluateWithoutBtRuntime()
{
  std::shared_ptr<DownloadContext> dctx(new DownloadContext(1_m, 1000000));
  dctx->getNetStat().updateUpload(1000000);

  std::shared_ptr<MockPieceStorage> pieceStorage(new MockPieceStorage());
  pieceStorage->setCompletedLength(1000000);

  ShareRatioSeedCriteria cri(1.0, dctx);
  REQUIRE(!cri.getPieceStorage());
  cri.setPieceStorage(pieceStorage);
  REQUIRE(pieceStorage.get() == cri.getPieceStorage());

  REQUIRE(cri.evaluate());

  cri.setRatio(2.0);
  REQUIRE(!cri.evaluate());
}

} // namespace aria2
