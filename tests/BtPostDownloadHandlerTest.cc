#include "BtPostDownloadHandler.h"

#include "a2doctest.h"

#include "DownloadContext.h"
#include "RequestGroup.h"
#include "Option.h"
#include "FileEntry.h"
#include "DefaultPieceStorage.h"
#include "bittorrent_helper.h"
#include "PieceStorage.h"
#include "DiskAdaptor.h"
#include "BtFileAllocationEntry.h"
#include "RequestGroupCriteria.h"
#include "BtCheckIntegrityEntry.h"
#include "ChecksumCheckIntegrityEntry.h"
#include "FileAllocationEntry.h"
#include "FileAllocationMan.h"
#include "CheckIntegrityMan.h"
#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "prefs.h"
#include "util.h"

namespace aria2 {

class BtPostDownloadHandlerTest {


private:
  std::shared_ptr<Option> option_;

public:
  void setUp() { option_.reset(new Option()); }

  void testCanHandle_extension();
  void testCanHandle_contentType();
  void testBtIncompleteHashCheckReportPolicy();
  void testBtSchedulesChecksumAfterPieceHash();
  void testGetNextRequestGroups();
};

A2_TEST(BtPostDownloadHandlerTest, testCanHandle_extension)
A2_TEST(BtPostDownloadHandlerTest, testCanHandle_contentType)
A2_TEST(BtPostDownloadHandlerTest, testBtIncompleteHashCheckReportPolicy)
A2_TEST(BtPostDownloadHandlerTest, testBtSchedulesChecksumAfterPieceHash)
A2_TEST(BtPostDownloadHandlerTest, testGetNextRequestGroups)

void BtPostDownloadHandlerTest::testCanHandle_extension()
{
  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(0, 0, A2_TEST_DIR "/test.torrent"));
  RequestGroup rg(GroupId::create(), option_);
  rg.setDownloadContext(dctx);

  BtPostDownloadHandler handler;

  REQUIRE(handler.canHandle(&rg));

  dctx->getFirstFileEntry()->setPath(A2_TEST_DIR "/test.torrent2");
  REQUIRE(!handler.canHandle(&rg));
}

void BtPostDownloadHandlerTest::testCanHandle_contentType()
{
  std::shared_ptr<DownloadContext> dctx(new DownloadContext(0, 0, "test"));
  dctx->getFirstFileEntry()->setContentType("application/x-bittorrent");
  RequestGroup rg(GroupId::create(), option_);
  rg.setDownloadContext(dctx);

  BtPostDownloadHandler handler;

  REQUIRE(handler.canHandle(&rg));

  dctx->getFirstFileEntry()->setContentType("application/octet-stream");
  REQUIRE(!handler.canHandle(&rg));
}

void BtPostDownloadHandlerTest::testBtIncompleteHashCheckReportPolicy()
{
  RequestGroup rg(GroupId::create(), option_);
  BtCheckIntegrityEntry entry(&rg);

  REQUIRE(!entry.shouldReportIncompleteAsError());

  option_->put(PREF_HASH_CHECK_ONLY, A2_V_TRUE);
  REQUIRE(entry.shouldReportIncompleteAsError());
}

void BtPostDownloadHandlerTest::testBtSchedulesChecksumAfterPieceHash()
{
  RequestGroup rg(GroupId::create(), option_);
  auto dctx = std::make_shared<DownloadContext>(1_k, 1_k, "/tmp/file");
  dctx->setDigest("sha-1",
                  util::fromHex(std::begin("0123456789abcdef0123456789abcdef01234567"),
                                std::end("0123456789abcdef0123456789abcdef01234567") - 1));
  rg.setDownloadContext(dctx);
  rg.setPieceStorage(std::make_shared<DefaultPieceStorage>(dctx, option_.get()));
  rg.getPieceStorage()->initStorage();

  DownloadEngine e(make_unique<SelectEventPoll>());
  e.setCheckIntegrityMan(make_unique<CheckIntegrityMan>());
  e.setFileAllocationMan(make_unique<FileAllocationMan>());
  option_->put(PREF_BT_HASH_CHECK_SEED, A2_V_TRUE);
  option_->put(PREF_FILE_ALLOCATION, V_PREALLOC);
  option_->put(PREF_NO_FILE_ALLOCATION_LIMIT, "0");
  std::vector<std::unique_ptr<Command>> commands;
  BtCheckIntegrityEntry entry(&rg);
  entry.onDownloadFinished(commands, &e);

  REQUIRE(e.getCheckIntegrityMan()->hasNext());
  auto checksumEntry = e.getCheckIntegrityMan()->pickNext();
  REQUIRE(dynamic_cast<ChecksumCheckIntegrityEntry*>(checksumEntry));

  std::vector<std::unique_ptr<Command>> nextCommands;
  checksumEntry->onDownloadFinished(nextCommands, &e);

  auto btFileAllocationEntry = e.getFileAllocationMan()->pickNext();
  REQUIRE(dynamic_cast<BtFileAllocationEntry*>(btFileAllocationEntry));
}

void BtPostDownloadHandlerTest::testGetNextRequestGroups()
{
  std::shared_ptr<DownloadContext> dctx(
      new DownloadContext(1_k, 0, A2_TEST_DIR "/test.torrent"));
  RequestGroup rg(GroupId::create(), option_);
  rg.setDownloadContext(dctx);
  rg.initPieceStorage();
  rg.getPieceStorage()->getDiskAdaptor()->enableReadOnly();

  BtPostDownloadHandler handler;
  std::vector<std::shared_ptr<RequestGroup>> groups;
  handler.getNextRequestGroups(groups, &rg);
  REQUIRE_EQ((size_t)1, groups.size());
  REQUIRE_EQ(
      std::string("248d0a1cd08284299de78d5c1ed359bb46717d8c"),
      bittorrent::getInfoHashString(groups.front()->getDownloadContext()));
  REQUIRE(std::find(rg.followedBy().begin(), rg.followedBy().end(),
                           groups.front()->getGID()) != rg.followedBy().end());
  for (auto& nrg : groups) {
    REQUIRE_EQ(rg.getGID(), nrg->following());
  }
}

} // namespace aria2
