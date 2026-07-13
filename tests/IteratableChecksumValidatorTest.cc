#include "IteratableChecksumValidator.h"

#include "a2doctest.h"

#include "TestUtil.h"
#include "DownloadContext.h"
#include "DefaultPieceStorage.h"
#include "Option.h"
#include "DiskAdaptor.h"
#include "FileEntry.h"
#include "PieceSelector.h"

namespace aria2 {

class IteratableChecksumValidatorTest {


private:
public:
  void setUp() {}

  void testValidate();
  void testValidate_fail();
};

A2_TEST(IteratableChecksumValidatorTest, testValidate)
A2_TEST(IteratableChecksumValidatorTest, testValidate_fail)

void IteratableChecksumValidatorTest::testValidate()
{
  Option option;
  std::shared_ptr<DownloadContext> dctx(new DownloadContext(
      100, 250, A2_TEST_DIR "/chunkChecksumTestFile250.txt"));
  dctx->setDigest("sha-1", fromHex("898a81b8e0181280ae2ee1b81e269196d91e869a"));
  std::shared_ptr<DefaultPieceStorage> ps(
      new DefaultPieceStorage(dctx, &option));
  ps->initStorage();
  ps->getDiskAdaptor()->enableReadOnly();
  ps->getDiskAdaptor()->openFile();

  IteratableChecksumValidator validator(dctx, ps);
  validator.init();
  while (!validator.finished()) {
    validator.validateChunk();
  }

  REQUIRE(ps->downloadFinished());
}

void IteratableChecksumValidatorTest::testValidate_fail()
{
  Option option;
  std::shared_ptr<DownloadContext> dctx(new DownloadContext(
      100, 250, A2_TEST_DIR "/chunkChecksumTestFile250.txt"));
  dctx->setDigest("sha-1", fromHex(std::string(40, '0'))); // set wrong checksum
  std::shared_ptr<DefaultPieceStorage> ps(
      new DefaultPieceStorage(dctx, &option));
  ps->initStorage();
  ps->getDiskAdaptor()->enableReadOnly();
  ps->getDiskAdaptor()->openFile();

  IteratableChecksumValidator validator(dctx, ps);
  validator.init();

  while (!validator.finished()) {
    validator.validateChunk();
  }

  REQUIRE(!ps->downloadFinished());
}

} // namespace aria2
