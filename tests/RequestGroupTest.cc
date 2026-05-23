#include "RequestGroup.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Command.h"
#include "DownloadEngine.h"
#include "Option.h"
#include "DownloadContext.h"
#include "FileEntry.h"
#include "Piece.h"
#include "PieceStorage.h"
#include "DefaultProgressInfoFile.h"
#include "File.h"
#include "TestUtil.h"
#include "DownloadResult.h"
#include "RequestGroupMan.h"
#include "SelectEventPoll.h"
#include "CurlDownloadCommand.h"
#include "InitiateConnectionCommandFactory.h"
#ifdef ENABLE_BITTORRENT
#  include "LibtorrentAttribute.h"
#  include "LibtorrentCommand.h"
#  include "LibtorrentSession.h"
#endif // ENABLE_BITTORRENT

namespace aria2 {

class RequestGroupTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RequestGroupTest);
  CPPUNIT_TEST(testGetFirstFilePath);
  CPPUNIT_TEST(testTryAutoFileRenaming);
  CPPUNIT_TEST(testCreateDownloadResult);
  CPPUNIT_TEST(testLoadAndOpenFileRestartFromScratch);
  CPPUNIT_TEST(testCompletedLengthReportsVerifiedStorageOnly);
  CPPUNIT_TEST(testInitiateConnectionFactoryUsesCurlForHttp);
  CPPUNIT_TEST(testInitiateConnectionFactoryUsesCurlForFtpFamily);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testCreateInitialCommandUsesLibtorrentRuntime);
  CPPUNIT_TEST(testLibtorrentCommandLoadsTorrentMetadata);
  CPPUNIT_TEST(testLibtorrentVerifiedProgressOverridesPieceStorage);
  CPPUNIT_TEST(testLibtorrentAllDownloadFinishedUsesSeedingStatus);
  CPPUNIT_TEST(testLibtorrentResumeDataRoundTrip);
  CPPUNIT_TEST(testLibtorrentSessionTracksActiveTorrent);
#endif // ENABLE_BITTORRENT
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<Option> option_;

public:
  void setUp() { option_.reset(new Option()); }

  void testGetFirstFilePath();
  void testTryAutoFileRenaming();
  void testCreateDownloadResult();
  void testLoadAndOpenFileRestartFromScratch();
  void testCompletedLengthReportsVerifiedStorageOnly();
  void testInitiateConnectionFactoryUsesCurlForHttp();
  void testInitiateConnectionFactoryUsesCurlForFtpFamily();
#ifdef ENABLE_BITTORRENT
  void testCreateInitialCommandUsesLibtorrentRuntime();
  void testLibtorrentCommandLoadsTorrentMetadata();
  void testLibtorrentVerifiedProgressOverridesPieceStorage();
  void testLibtorrentAllDownloadFinishedUsesSeedingStatus();
  void testLibtorrentResumeDataRoundTrip();
  void testLibtorrentSessionTracksActiveTorrent();
#endif // ENABLE_BITTORRENT
};

CPPUNIT_TEST_SUITE_REGISTRATION(RequestGroupTest);

void RequestGroupTest::testGetFirstFilePath()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_k, "/tmp/myfile"));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile"), group.getFirstFilePath());

  group.markInMemoryDownload();

  CPPUNIT_ASSERT_EQUAL(std::string("[MEMORY]myfile"), group.getFirstFilePath());
}

void RequestGroupTest::testTryAutoFileRenaming()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_k, "/tmp/myfile"));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  option_->put(PREF_AUTO_FILE_RENAMING, "false");
  try {
    group.tryAutoFileRenaming();
  }
  catch (const Exception& ex) {
    CPPUNIT_ASSERT_EQUAL(error_code::FILE_ALREADY_EXISTS, ex.getErrorCode());
  }

  option_->put(PREF_AUTO_FILE_RENAMING, "true");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile.1"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp/myfile.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile.1.txt"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/myfile");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/myfile.1"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/myfile.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/myfile.1.txt"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath(".bashrc");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string(".bashrc.1"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath(".bashrc.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string(".bashrc.1.txt"), group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/.bashrc");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/.bashrc.1"),
                       group.getFirstFilePath());

  ctx->getFirstFileEntry()->setPath("/tmp.txt/.bashrc.txt");
  group.tryAutoFileRenaming();
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp.txt/.bashrc.1.txt"),
                       group.getFirstFilePath());
}

void RequestGroupTest::testCreateDownloadResult()
{
  std::shared_ptr<DownloadContext> ctx(
      new DownloadContext(1_k, 1_m, "/tmp/myfile"));
  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  {
    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/myfile"),
                         result->fileEntries[0]->getPath());
    CPPUNIT_ASSERT_EQUAL((int64_t)1_m,
                         result->fileEntries.back()->getLastOffset());
    CPPUNIT_ASSERT_EQUAL((uint64_t)0, result->sessionDownloadLength);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, result->sessionTime.count());
    // result is UNKNOWN_ERROR if download has not completed and no specific
    // error has been reported
    CPPUNIT_ASSERT_EQUAL(error_code::UNKNOWN_ERROR, result->result);

    // if haltReason is set to RequestGroup::USER_REQUEST, download
    // result will become REMOVED.
    group.setHaltRequested(true, RequestGroup::USER_REQUEST);
    result = group.createDownloadResult();
    CPPUNIT_ASSERT_EQUAL(error_code::REMOVED, result->result);
    // if haltReason is set to RequestGroup::SHUTDOWN_SIGNAL, download
    // result will become IN_PROGRESS.
    group.setHaltRequested(true, RequestGroup::SHUTDOWN_SIGNAL);
    result = group.createDownloadResult();
    CPPUNIT_ASSERT_EQUAL(error_code::IN_PROGRESS, result->result);
  }
  {
    group.setLastErrorCode(error_code::RESOURCE_NOT_FOUND);

    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(error_code::RESOURCE_NOT_FOUND, result->result);
  }
  {
    group.getPieceStorage()->markAllPiecesDone();

    std::shared_ptr<DownloadResult> result = group.createDownloadResult();

    CPPUNIT_ASSERT_EQUAL(error_code::FINISHED, result->result);
  }
}

void RequestGroupTest::testLoadAndOpenFileRestartFromScratch()
{
  auto path = std::string(A2_TEST_OUT_DIR) +
              "/aria2_RequestGroupTest_testLoadAndOpenFileRestartFromScratch";
  File(path).remove();
  File(path + DefaultProgressInfoFile::getSuffix()).remove();
  createFile(path, 1_k);

  option_->put(PREF_CONTINUE, A2_V_TRUE);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);

  auto ctx = std::make_shared<DownloadContext>(1_k, 2_k, path);
  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();

  auto infoFile =
      std::make_shared<DefaultProgressInfoFile>(ctx, group.getPieceStorage(),
                                                  option_.get());
  group.loadAndOpenFile(infoFile, RequestGroup::RESTART_FROM_SCRATCH);

  CPPUNIT_ASSERT_EQUAL((int64_t)0, group.getCompletedLength());
  CPPUNIT_ASSERT_EQUAL((int64_t)0, File(path).size());
}

void RequestGroupTest::testCompletedLengthReportsVerifiedStorageOnly()
{
  auto ctx = std::make_shared<DownloadContext>(1_m, 256_m, "file.bin");
  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  group.getPieceStorage()->markPiecesDone(250_m);

  std::vector<std::shared_ptr<Piece>> inFlightPieces;
  for (int i = 0; i < 2; ++i) {
    auto p = std::make_shared<Piece>(250 + i, 1_m);
    for (int j = 0; j < 32; ++j) {
      p->completeBlock(j);
    }
    inFlightPieces.push_back(p);
  }
  group.getPieceStorage()->addInFlightPiece(inFlightPieces);

  CPPUNIT_ASSERT_EQUAL((int64_t)250_m, group.getCompletedLength());
  CPPUNIT_ASSERT_EQUAL((int64_t)1_m,
                       group.getPieceStorage()->getInFlightCompletedLength());
}

void RequestGroupTest::testInitiateConnectionFactoryUsesCurlForHttp()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_SPLIT, "1");

  auto group = createRequestGroup(1_k, 1_k,
                                  std::string(A2_TEST_OUT_DIR) +
                                      "/aria2_RequestGroupTest_curl_http",
                                  "http://example.test/file", option_);
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
  group->setRequestGroupMan(engine.getRequestGroupMan().get());
  group->initPieceStorage();
  auto request = std::make_shared<Request>();
  CPPUNIT_ASSERT(request->setUri("http://example.test/file"));

  auto command = InitiateConnectionCommandFactory::createInitiateConnectionCommand(
      engine.newCUID(), request, group->getDownloadContext()->getFirstFileEntry(),
      group.get(), &engine);

  CPPUNIT_ASSERT(dynamic_cast<CurlDownloadCommand*>(command.get()));
}

void RequestGroupTest::testInitiateConnectionFactoryUsesCurlForFtpFamily()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_SPLIT, "1");

  const std::string uris[] = {"ftp://example.test/file",
                              "ftps://example.test/file",
                              "sftp://example.test/file",
                              "scp://example.test/file"};
  for (const auto& uri : uris) {
    auto group = createRequestGroup(
        1_k, 1_k,
        std::string(A2_TEST_OUT_DIR) +
            "/aria2_RequestGroupTest_curl_ftp_family",
        uri, option_);
    DownloadEngine engine(make_unique<SelectEventPoll>());
    engine.setOption(option_.get());
    engine.setRequestGroupMan(make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
    group->setRequestGroupMan(engine.getRequestGroupMan().get());
    group->initPieceStorage();
    auto request = std::make_shared<Request>();
    CPPUNIT_ASSERT(request->setUri(uri));

    auto command =
        InitiateConnectionCommandFactory::createInitiateConnectionCommand(
            engine.newCUID(), request,
            group->getDownloadContext()->getFirstFileEntry(), group.get(),
            &engine);

    CPPUNIT_ASSERT(dynamic_cast<CurlDownloadCommand*>(command.get()));
  }
}

#ifdef ENABLE_BITTORRENT
void RequestGroupTest::testCreateInitialCommandUsesLibtorrentRuntime()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));

  auto ctx = std::make_shared<DownloadContext>(0, 0, "magnet");
  ctx->setAttribute(
      CTX_ATTR_LIBTORRENT,
      make_unique<LibtorrentAttribute>(
          LibtorrentAttribute::SourceType::MAGNET,
          "magnet:?xt=urn:btih:3D366ED505B977FC61C9A6EE01E96329", "",
          std::vector<std::string>{}));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  std::vector<std::unique_ptr<Command>> commands;
  group.createInitialCommand(commands, &engine);

  CPPUNIT_ASSERT_EQUAL((size_t)1, commands.size());
  CPPUNIT_ASSERT(dynamic_cast<LibtorrentCommand*>(commands[0].get()));
}

void RequestGroupTest::testLibtorrentCommandLoadsTorrentMetadata()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);
  option_->put(PREF_LISTEN_PORT, "0");
  option_->put(PREF_DHT_LISTEN_PORT, "0");

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  auto rgman = make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get());
  auto rgmanPtr = rgman.get();
  engine.setRequestGroupMan(std::move(rgman));

  auto ctx = std::make_shared<DownloadContext>(0, 0, "torrent");
  ctx->setAttribute(
      CTX_ATTR_LIBTORRENT,
      make_unique<LibtorrentAttribute>(
          LibtorrentAttribute::SourceType::TORRENT_FILE,
          A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{}));

  RequestGroup group(GroupId::create(), option_);
  group.setRequestGroupMan(rgmanPtr);
  group.setDownloadContext(ctx);

  LibtorrentCommand command(engine.newCUID(), &group, &engine);
  command.preProcess();

  CPPUNIT_ASSERT(ctx->knowsTotalLength());
  CPPUNIT_ASSERT(ctx->getTotalLength() > 0);
  CPPUNIT_ASSERT(ctx->getFirstFileEntry()->getPath().find(A2_TEST_OUT_DIR) ==
                 0);
}

void RequestGroupTest::testLibtorrentVerifiedProgressOverridesPieceStorage()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{});
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  group.getPieceStorage()->markAllPiecesDone();

  attrsPtr->status.totalLength = 100_k;
  attrsPtr->status.completedLength = 99_k;
  attrsPtr->status.complete = false;

  CPPUNIT_ASSERT(!group.downloadFinished());
  CPPUNIT_ASSERT_EQUAL((int64_t)100_k, group.getTotalLength());
  CPPUNIT_ASSERT_EQUAL((int64_t)99_k, group.getCompletedLength());
}

void RequestGroupTest::testLibtorrentAllDownloadFinishedUsesSeedingStatus()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{});
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  group.getPieceStorage()->markAllPiecesDone();

  attrsPtr->status.hasStatus = true;
  attrsPtr->status.complete = true;
  attrsPtr->status.seeding = false;
  CPPUNIT_ASSERT(!group.allDownloadFinished());

  attrsPtr->status.seeding = true;
  CPPUNIT_ASSERT(group.allDownloadFinished());
}

void RequestGroupTest::testLibtorrentResumeDataRoundTrip()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET,
      "magnet:?xt=urn:btih:0101010101010101010101010101010101010101", "",
      std::vector<std::string>{});
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  const std::string resumeData("libtorrent fast resume");
  attrsPtr->setResumeData(resumeData);
  CPPUNIT_ASSERT(attrsPtr->hasResumeData());
  CPPUNIT_ASSERT_EQUAL(resumeData, attrsPtr->getResumeData());

  auto saved = attrsPtr->takeResumeData();
  CPPUNIT_ASSERT_EQUAL(resumeData, saved);
  CPPUNIT_ASSERT(!attrsPtr->hasResumeData());

  attrsPtr->setResumeData(saved);
  CPPUNIT_ASSERT(attrsPtr->hasResumeData());
  CPPUNIT_ASSERT_EQUAL(resumeData, attrsPtr->getResumeData());
}

void RequestGroupTest::testLibtorrentSessionTracksActiveTorrent()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);
  option_->put(PREF_LISTEN_PORT, "0");
  option_->put(PREF_DHT_LISTEN_PORT, "0");

  auto ctx = std::make_shared<DownloadContext>(1_k, 0, "magnet");
  ctx->markTotalLengthIsUnknown();
  ctx->setAttribute(
      CTX_ATTR_LIBTORRENT,
      make_unique<LibtorrentAttribute>(
          LibtorrentAttribute::SourceType::MAGNET,
          "magnet:?xt=urn:btih:0101010101010101010101010101010101010101", "",
          std::vector<std::string>{}));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  auto eventPoll = make_unique<SelectEventPoll>();
  auto engine = make_unique<DownloadEngine>(std::move(eventPoll));
  engine->setOption(option_.get());

  auto command = make_unique<LibtorrentCommand>(engine->newCUID(), &group,
                                                engine.get());
  command->preProcess();
  auto& session = engine->getLibtorrentSession();
  CPPUNIT_ASSERT(session.hasTorrent(group.getGID()));

  session.setTorrentDownloadLimit(group.getGID(), 100_k);
  session.setTorrentUploadLimit(group.getGID(), 50_k);
  session.setTorrentMaxConnections(group.getGID(), 40);
  command.reset();
  CPPUNIT_ASSERT(!session.hasTorrent(group.getGID()));
}
#endif // ENABLE_BITTORRENT

} // namespace aria2
