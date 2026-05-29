#include "RequestGroupMan.h"

#include <fstream>

#include <cppunit/extensions/HelperMacros.h>

#include "TestUtil.h"
#include "Ed2kAttribute.h"
#include "Ed2kSharedStore.h"
#include "DefaultProgressInfoFile.h"
#include "DiskAdaptor.h"
#include "prefs.h"
#include "DownloadContext.h"
#include "RequestGroup.h"
#include "Option.h"
#include "DownloadResult.h"
#include "FileEntry.h"
#include "PieceStorage.h"
#include "ServerStatMan.h"
#include "ServerStat.h"
#include "File.h"
#include "TorrentMetadataFollow.h"
#include "array_fun.h"
#include "RecoverableException.h"
#include "util.h"
#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "UriListParser.h"
#include "Command.h"
#ifdef ENABLE_BITTORRENT
#  include "LibtorrentAttribute.h"
#endif // ENABLE_BITTORRENT

namespace aria2 {

namespace {
class ActiveDownloadCommand : public Command {
private:
  RequestGroup* requestGroup_;

public:
  ActiveDownloadCommand(cuid_t cuid, RequestGroup* requestGroup)
      : Command(cuid), requestGroup_(requestGroup)
  {
    setStatusActive();
    requestGroup_->increaseNumCommand();
  }

  ~ActiveDownloadCommand() { requestGroup_->decreaseNumCommand(); }

  bool execute() CXX11_OVERRIDE
  {
    return requestGroup_->isHaltRequested();
  }
};
} // namespace

class RequestGroupManTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RequestGroupManTest);
  CPPUNIT_TEST(testIsSameFileBeingDownloaded);
  CPPUNIT_TEST(testGetInitialCommands);
  CPPUNIT_TEST(testLoadServerStat);
  CPPUNIT_TEST(testSaveServerStat);
  CPPUNIT_TEST(testChangeReservedGroupPosition);
  CPPUNIT_TEST(testFillRequestGroupFromReserver);
  CPPUNIT_TEST(testFillRequestGroupFromReserver_uriParser);
  CPPUNIT_TEST(testFillRequestGroupFromReserverSkipsDuplicateBtInfoHash);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testTorrentMetadataResponseDetection);
  CPPUNIT_TEST(testCompletedTorrentMetadataStartsLibtorrentTask);
#endif // ENABLE_BITTORRENT
  CPPUNIT_TEST(testReduceMaxConcurrentDownloads);
  CPPUNIT_TEST(testUploadDeltaFeedsGlobalStat);
  CPPUNIT_TEST(testUserRemoveDoesNotKeepControlFile);
  CPPUNIT_TEST(testInsertReservedGroup);
  CPPUNIT_TEST(testAddDownloadResult);
  CPPUNIT_TEST(testAddDownloadResultSharesCompletedEd2kFile);
  CPPUNIT_TEST_SUITE_END();

private:
  std::unique_ptr<DownloadEngine> e_;
  std::shared_ptr<Option> option_;
  RequestGroupMan* rgman_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    option_->put(PREF_PIECE_LENGTH, "1048576");
    // To enable paused RequestGroup
    option_->put(PREF_ENABLE_RPC, A2_V_TRUE);
    File(option_->get(PREF_DIR)).mkdirs();
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
    auto rgman = make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 3, option_.get());
    rgman_ = rgman.get();
    e_->setRequestGroupMan(std::move(rgman));
  }

  void testIsSameFileBeingDownloaded();
  void testGetInitialCommands();
  void testLoadServerStat();
  void testSaveServerStat();
  void testChangeReservedGroupPosition();
  void testFillRequestGroupFromReserver();
  void testFillRequestGroupFromReserver_uriParser();
  void testFillRequestGroupFromReserverSkipsDuplicateBtInfoHash();
#ifdef ENABLE_BITTORRENT
  void testTorrentMetadataResponseDetection();
  void testCompletedTorrentMetadataStartsLibtorrentTask();
#endif // ENABLE_BITTORRENT
  void testReduceMaxConcurrentDownloads();
  void testUploadDeltaFeedsGlobalStat();
  void testUserRemoveDoesNotKeepControlFile();
  void testInsertReservedGroup();
  void testAddDownloadResult();
  void testAddDownloadResultSharesCompletedEd2kFile();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RequestGroupManTest);

void RequestGroupManTest::testIsSameFileBeingDownloaded()
{
  std::shared_ptr<RequestGroup> rg1(
      new RequestGroup(GroupId::create(), util::copy(option_)));
  std::shared_ptr<RequestGroup> rg2(
      new RequestGroup(GroupId::create(), util::copy(option_)));

  std::shared_ptr<DownloadContext> dctx1(
      new DownloadContext(0, 0, "aria2.tar.bz2"));
  std::shared_ptr<DownloadContext> dctx2(
      new DownloadContext(0, 0, "aria2.tar.bz2"));

  rg1->setDownloadContext(dctx1);
  rg2->setDownloadContext(dctx2);

  RequestGroupMan gm(std::vector<std::shared_ptr<RequestGroup>>(), 1,
                     option_.get());

  gm.addRequestGroup(rg1);
  gm.addRequestGroup(rg2);

  CPPUNIT_ASSERT(gm.isSameFileBeingDownloaded(rg1.get()));

  dctx2->getFirstFileEntry()->setPath("aria2.tar.gz");

  CPPUNIT_ASSERT(!gm.isSameFileBeingDownloaded(rg1.get()));
}

void RequestGroupManTest::testGetInitialCommands()
{
  // TODO implement later
}

void RequestGroupManTest::testSaveServerStat()
{
  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 0,
                     option_.get());
  std::shared_ptr<ServerStat> ss_localhost(new ServerStat("localhost", "http"));
  rm.addServerStat(ss_localhost);
  File f(A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_testSaveServerStat");
  if (f.exists()) {
    f.remove();
  }
  CPPUNIT_ASSERT(rm.saveServerStat(f.getPath()));
  CPPUNIT_ASSERT(f.isFile());

  f.remove();
  CPPUNIT_ASSERT(f.mkdirs());
  CPPUNIT_ASSERT(!rm.saveServerStat(f.getPath()));
}

void RequestGroupManTest::testLoadServerStat()
{
  File f(A2_TEST_OUT_DIR "/aria2_RequestGroupManTest_testLoadServerStat");
  std::ofstream o(f.getPath().c_str(), std::ios::binary);
  o << "host=localhost, protocol=http, dl_speed=0, last_updated=1219505257,"
    << "status=OK";
  o.close();

  RequestGroupMan rm(std::vector<std::shared_ptr<RequestGroup>>(), 0,
                     option_.get());
  std::cerr << "testLoadServerStat" << std::endl;
  CPPUNIT_ASSERT(rm.loadServerStat(f.getPath()));
  std::shared_ptr<ServerStat> ss_localhost =
      rm.findServerStat("localhost", "http");
  CPPUNIT_ASSERT(ss_localhost);
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), ss_localhost->getHostname());
}

void RequestGroupManTest::testChangeReservedGroupPosition()
{
  std::vector<std::shared_ptr<RequestGroup>> gs{
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)),
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_))};
  RequestGroupMan rm(gs, 0, option_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 0, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 1, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), 10, OFFSET_MODE_SET));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[0]->getGID(), -10, OFFSET_MODE_SET));

  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 0, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -10, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 1, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), 10, OFFSET_MODE_CUR));
  CPPUNIT_ASSERT_EQUAL((size_t)1, rm.changeReservedGroupPosition(
                                      gs[1]->getGID(), -2, OFFSET_MODE_CUR));

  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), 0, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)2, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), -1, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)0, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), -10, OFFSET_MODE_END));
  CPPUNIT_ASSERT_EQUAL((size_t)3, rm.changeReservedGroupPosition(
                                      gs[3]->getGID(), 10, OFFSET_MODE_END));

  CPPUNIT_ASSERT_EQUAL((size_t)4, rm.getReservedGroups().size());

  try {
    rm.changeReservedGroupPosition(GroupId::create()->getNumericId(), 0,
                                   OFFSET_MODE_CUR);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // success
  }
}

void RequestGroupManTest::testFillRequestGroupFromReserver()
{
  std::shared_ptr<RequestGroup> rgs[] = {
      createRequestGroup(0, 0, "foo1", "http://host/foo1", util::copy(option_)),
      createRequestGroup(0, 0, "foo2", "http://host/foo2", util::copy(option_)),
      createRequestGroup(0, 0, "foo3", "http://host/foo3", util::copy(option_)),
      // Intentionally same path/URI for first RequestGroup and set
      // length explicitly to do duplicate filename check.
      createRequestGroup(0, 10, "foo1", "http://host/foo1",
                         util::copy(option_)),
      createRequestGroup(0, 0, "foo4", "http://host/foo4", util::copy(option_)),
      createRequestGroup(0, 0, "foo5", "http://host/foo5",
                         util::copy(option_))};
  rgs[1]->setPauseRequested(true);
  for (const auto& i : rgs) {
    rgman_->addReservedGroup(i);
  }
  rgman_->fillRequestGroupFromReserver(e_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman_->getReservedGroups().size());
}

void RequestGroupManTest::testFillRequestGroupFromReserver_uriParser()
{
  std::shared_ptr<RequestGroup> rgs[] = {
      createRequestGroup(0, 0, "mem1", "http://mem1", util::copy(option_)),
      createRequestGroup(0, 0, "mem2", "http://mem2", util::copy(option_)),
  };
  rgs[0]->setPauseRequested(true);
  for (const auto& i : rgs) {
    rgman_->addReservedGroup(i);
  }

  std::shared_ptr<UriListParser> flp(
      new UriListParser(A2_TEST_DIR "/filelist2.txt"));
  rgman_->setUriListParser(flp);

  rgman_->fillRequestGroupFromReserver(e_.get());

  RequestGroupList::const_iterator itr;
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getReservedGroups().size());
  itr = rgman_->getReservedGroups().begin();
  CPPUNIT_ASSERT_EQUAL(rgs[0]->getGID(), (*itr)->getGID());
  CPPUNIT_ASSERT_EQUAL((size_t)3, rgman_->getRequestGroups().size());
}

void RequestGroupManTest::testFillRequestGroupFromReserverSkipsDuplicateBtInfoHash()
{
#ifdef ENABLE_BITTORRENT
  option_->put(PREF_LISTEN_PORT, "0");
  option_->put(PREF_DHT_LISTEN_PORT, "0");
  option_->put(PREF_MAX_CONCURRENT_DOWNLOADS, "2");
  auto createBtGroup = [this]() {
    auto dctx = std::make_shared<DownloadContext>(1_k, 0, "magnet");
    dctx->markTotalLengthIsUnknown();
    dctx->setAttribute(
        CTX_ATTR_LIBTORRENT,
        make_unique<LibtorrentAttribute>(
            LibtorrentAttribute::SourceType::MAGNET,
            "magnet:?xt=urn:btih:0101010101010101010101010101010101010101",
            "", std::vector<std::string>{},
            A2_TEST_OUT_DIR "/0101010101010101010101010101010101010101.aria2",
            std::string(20, '\x11')));
    auto group = std::make_shared<RequestGroup>(GroupId::create(),
                                                util::copy(option_));
    group->setDownloadContext(dctx);
    return group;
  };

  rgman_->addReservedGroup(createBtGroup());
  rgman_->addReservedGroup(createBtGroup());
  rgman_->fillRequestGroupFromReserver(e_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getRequestGroups().size());
  CPPUNIT_ASSERT_EQUAL((size_t)0, rgman_->getReservedGroups().size());
  CPPUNIT_ASSERT(rgman_->getDownloadResults().empty());
#endif // ENABLE_BITTORRENT
}

#ifdef ENABLE_BITTORRENT
void RequestGroupManTest::testTorrentMetadataResponseDetection()
{
  CPPUNIT_ASSERT(isTorrentMetadataResponse("/tmp/file.torrent", ""));
  CPPUNIT_ASSERT(isTorrentMetadataResponse(
      "/tmp/file.bin", "application/x-bittorrent; charset=binary"));
  CPPUNIT_ASSERT(!isTorrentMetadataResponse("/tmp/file.bin",
                                            "application/octet-stream"));
}

void RequestGroupManTest::testCompletedTorrentMetadataStartsLibtorrentTask()
{
  option_->put(PREF_TORRENT_METADATA, "start");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  const std::string path = A2_TEST_OUT_DIR "/remote-metadata.torrent";
  File(path).remove();
  auto data = readFile(A2_TEST_DIR "/single.torrent");

  auto group = createRequestGroup(data.size(), data.size(), path,
                                  "http://example.test/file.torrent",
                                  util::copy(option_));
  group->getDownloadContext()->getFirstFileEntry()->setContentType(
      "application/octet-stream");
  group->setRequestGroupMan(rgman_);
  group->setState(RequestGroup::STATE_ACTIVE);
  group->initPieceStorage();
  group->getPieceStorage()->getDiskAdaptor()->openFile();
  group->getPieceStorage()->getDiskAdaptor()->writeData(
      reinterpret_cast<const unsigned char*>(data.data()), data.size(), 0);
  group->getPieceStorage()->markAllPiecesDone();

  rgman_->addRequestGroup(group);
  while (e_->run(true) != 0)
    ;

  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getReservedGroups().size());
  auto child = *rgman_->getReservedGroups().begin();
  CPPUNIT_ASSERT(child->getDownloadContext()->hasAttribute(CTX_ATTR_LIBTORRENT));
  CPPUNIT_ASSERT_EQUAL(group->getGID(), child->following());
  CPPUNIT_ASSERT_EQUAL((size_t)1, group->followedBy().size());
  CPPUNIT_ASSERT_EQUAL(child->getGID(), group->followedBy()[0]);
}
#endif // ENABLE_BITTORRENT

void RequestGroupManTest::testReduceMaxConcurrentDownloads()
{
  std::vector<std::shared_ptr<RequestGroup>> rgs{
      createRequestGroup(0, 0, "active1", "http://host/active1",
                         util::copy(option_)),
      createRequestGroup(0, 0, "active2", "http://host/active2",
                         util::copy(option_)),
      createRequestGroup(0, 0, "active3", "http://host/active3",
                         util::copy(option_))};
  for (const auto& rg : rgs) {
    rg->setRequestGroupMan(rgman_);
    rg->setState(RequestGroup::STATE_ACTIVE);
    rgman_->addRequestGroup(rg);
    e_->addCommand(make_unique<ActiveDownloadCommand>(e_->newCUID(), rg.get()));
  }

  rgman_->setMaxConcurrentDownloads(1);
  rgman_->reduceActiveDownloadsToLimit(e_.get());
  CPPUNIT_ASSERT(!rgs[0]->isHaltRequested());
  CPPUNIT_ASSERT(rgs[1]->isHaltRequested());
  CPPUNIT_ASSERT(rgs[2]->isHaltRequested());

  while (e_->run(true) != 0)
    ;

  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman_->getRequestGroups().size());
  auto active = rgman_->getRequestGroups().begin();
  CPPUNIT_ASSERT_EQUAL(rgs[0]->getGID(), (*active)->getGID());
  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman_->getReservedGroups().size());
  CPPUNIT_ASSERT(findReservedGroup(rgman_, rgs[1]->getGID()));
  CPPUNIT_ASSERT(findReservedGroup(rgman_, rgs[2]->getGID()));
  CPPUNIT_ASSERT(!rgs[1]->isPauseRequested());
  CPPUNIT_ASSERT(!rgs[2]->isPauseRequested());
}

void RequestGroupManTest::testUploadDeltaFeedsGlobalStat()
{
  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  auto dctx = std::make_shared<DownloadContext>(1_k, 0, "bt-upload.bin");
  group->setRequestGroupMan(rgman_);
  group->setDownloadContext(dctx);

  dctx->updateUpload(8048);

  auto taskStat = group->calculateStat();
  auto globalStat = rgman_->calculateStat();
  CPPUNIT_ASSERT(taskStat.uploadSpeed > 0);
  CPPUNIT_ASSERT(globalStat.uploadSpeed > 0);
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(8048),
                       taskStat.sessionUploadLength);
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(8048),
                       globalStat.sessionUploadLength);
}

void RequestGroupManTest::testUserRemoveDoesNotKeepControlFile()
{
  const std::string path =
      A2_TEST_OUT_DIR "/request-group-man-user-remove.bin";
  const std::string ctrlPath = path + DefaultProgressInfoFile::getSuffix();
  File(path).remove();
  File(ctrlPath).remove();
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  rgman_->setMaxDownloadResult(1);

  auto group = createRequestGroup(1_k, 4_k, path, "http://host/file",
                                  util::copy(option_));
  group->setRequestGroupMan(rgman_);
  group->setState(RequestGroup::STATE_ACTIVE);
  group->initPieceStorage();
  group->getPieceStorage()->getDiskAdaptor()->openFile();
  group->setProgressInfoFile(std::make_shared<DefaultProgressInfoFile>(
      group->getDownloadContext(), group->getPieceStorage(), option_.get()));
  group->saveControlFile();
  CPPUNIT_ASSERT(File(ctrlPath).isFile());

  rgman_->addRequestGroup(group);
  e_->addCommand(make_unique<ActiveDownloadCommand>(e_->newCUID(),
                                                    group.get()));

  group->setForceHaltRequested(true, RequestGroup::USER_REQUEST);
  while (e_->run(true) != 0)
    ;

  CPPUNIT_ASSERT(!File(ctrlPath).exists());
  CPPUNIT_ASSERT_EQUAL((size_t)0, rgman_->getRequestGroups().size());
  auto result = rgman_->findDownloadResult(group->getGID());
  CPPUNIT_ASSERT(result);
  CPPUNIT_ASSERT_EQUAL(error_code::REMOVED, result->result);
}

void RequestGroupManTest::testInsertReservedGroup()
{
  std::vector<std::shared_ptr<RequestGroup>> rgs1{
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_))),
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_)))};
  std::vector<std::shared_ptr<RequestGroup>> rgs2{
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_))),
      std::shared_ptr<RequestGroup>(
          new RequestGroup(GroupId::create(), util::copy(option_)))};
  rgman_->insertReservedGroup(0, rgs1);
  CPPUNIT_ASSERT_EQUAL((size_t)2, rgman_->getReservedGroups().size());
  RequestGroupList::const_iterator itr;
  itr = rgman_->getReservedGroups().begin();
  CPPUNIT_ASSERT_EQUAL(rgs1[0]->getGID(), (*itr++)->getGID());
  CPPUNIT_ASSERT_EQUAL(rgs1[1]->getGID(), (*itr++)->getGID());

  rgman_->insertReservedGroup(1, rgs2);
  CPPUNIT_ASSERT_EQUAL((size_t)4, rgman_->getReservedGroups().size());
  itr = rgman_->getReservedGroups().begin();
  ++itr;
  CPPUNIT_ASSERT_EQUAL(rgs2[0]->getGID(), (*itr++)->getGID());
  CPPUNIT_ASSERT_EQUAL(rgs2[1]->getGID(), (*itr++)->getGID());
}

void RequestGroupManTest::testAddDownloadResult()
{
  std::string uri = "http://example.org";
  rgman_->setMaxDownloadResult(3);
  rgman_->addDownloadResult(createDownloadResult(error_code::TIME_OUT, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  rgman_->addDownloadResult(createDownloadResult(error_code::FINISHED, uri));
  CPPUNIT_ASSERT_EQUAL(error_code::TIME_OUT,
                       rgman_->getDownloadStat().getLastErrorResult());
}

void RequestGroupManTest::testAddDownloadResultSharesCompletedEd2kFile()
{
  const std::string hash(16, '\x44');
  const std::string path =
      A2_TEST_OUT_DIR "/request-group-man-ed2k-shared.bin";
  createFile(path, 321);
  auto result = createDownloadResult(error_code::FINISHED, "ed2k://test");
  result->totalLength = 321;
  result->fileEntries.clear();
  result->fileEntries.push_back(std::make_shared<FileEntry>(path, 321, 0));
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.name = "request-group-man-ed2k-shared.bin";
  attrs->link.size = 321;
  attrs->link.hash = hash;
  result->attrs.push_back(attrs);

  rgman_->addDownloadResult(result);

  auto file = rgman_->getEd2kSharedStore()->findByHash(hash);
  CPPUNIT_ASSERT(file);
  CPPUNIT_ASSERT_EQUAL(path, file->path);
  CPPUNIT_ASSERT_EQUAL((int64_t)321, file->size);
}

} // namespace aria2
