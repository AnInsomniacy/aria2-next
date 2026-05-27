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
#include "CheckIntegrityMan.h"
#include "CheckIntegrityEntry.h"
#include "util.h"
#ifdef ENABLE_BITTORRENT
#  include "LibtorrentAttribute.h"
#  include "LibtorrentCommand.h"
#  include "LibtorrentProgressInfoFile.h"
#  include "LibtorrentSession.h"
#  include <libtorrent/bdecode.hpp>
#  include <libtorrent/load_torrent.hpp>
#  include <libtorrent/magnet_uri.hpp>
#  include <libtorrent/read_resume_data.hpp>
#  include <libtorrent/torrent_flags.hpp>
#  include <libtorrent/write_resume_data.hpp>
#endif // ENABLE_BITTORRENT

namespace aria2 {

#ifdef ENABLE_BITTORRENT
namespace {
std::string loadTorrentInfoMetadata(const std::string& path)
{
  auto data = readFile(path);
  lt::error_code ec;
  auto root = lt::bdecode(
      lt::span<char const>(data.data(), static_cast<int>(data.size())), ec);
  CPPUNIT_ASSERT(!ec);
  auto info = root.dict_find_dict("info");
  CPPUNIT_ASSERT(info);
  auto section = info.data_section();
  return std::string(section.data(), section.size());
}
} // namespace
#endif // ENABLE_BITTORRENT

class RequestGroupTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RequestGroupTest);
  CPPUNIT_TEST(testGetFirstFilePath);
  CPPUNIT_TEST(testTryAutoFileRenaming);
  CPPUNIT_TEST(testCreateDownloadResult);
  CPPUNIT_TEST(testLoadAndOpenFileRestartFromScratch);
  CPPUNIT_TEST(testCompletedLengthReportsVerifiedStorageOnly);
  CPPUNIT_TEST(testCurlTlsTrustOptions);
  CPPUNIT_TEST(testCurlProxyModeControlsEnvironmentProxy);
  CPPUNIT_TEST(testCurlHttpRetryableErrors);
  CPPUNIT_TEST(testCurlMetadataHeadFailureFallbackPolicy);
  CPPUNIT_TEST(testFinishedHttpDownloadQueuesWholeFileChecksum);
  CPPUNIT_TEST(testInitiateConnectionFactoryUsesCurlForHttp);
  CPPUNIT_TEST(testInitiateConnectionFactoryUsesCurlForFtpFamily);
  CPPUNIT_TEST(testHttpAdaptiveCommandLimit);
  CPPUNIT_TEST(testHttpAdaptiveInitialHandoffFillsWindow);
  CPPUNIT_TEST(testHttpAdaptiveSuccessFillsExpandedWindow);
  CPPUNIT_TEST(testHttpRateLimitKeepsRangeAndCapsConcurrency);
  CPPUNIT_TEST(testHttpRangeDowngradeDisablesRangedConcurrency);
  CPPUNIT_TEST(testHttpRangeDowngradeRejectsExtraActiveStreamRetries);
  CPPUNIT_TEST(testFtpDoesNotUseHttpAdaptiveCommandLimit);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testCreateInitialCommandUsesLibtorrentRuntime);
  CPPUNIT_TEST(testLibtorrentCommandLoadsTorrentMetadata);
  CPPUNIT_TEST(testLibtorrentVerifiedProgressOverridesPieceStorage);
  CPPUNIT_TEST(testLibtorrentResumeProgressUsedWhileLiveStatusIsEmpty);
  CPPUNIT_TEST(testLibtorrentAllDownloadFinishedUsesSeedingStatus);
  CPPUNIT_TEST(testLibtorrentActiveSeedingIsStillInProgress);
  CPPUNIT_TEST(testLibtorrentPartialSharingIsStillInProgress);
  CPPUNIT_TEST(testLibtorrentFinishedRemovesControlFileEvenWithForceSave);
  CPPUNIT_TEST(testLibtorrentResumeDataRoundTrip);
  CPPUNIT_TEST(testLibtorrentEmptyResumeDataDoesNotOverwriteControlFile);
  CPPUNIT_TEST(testLibtorrentControlFileLoadRestoresResumeStatus);
  CPPUNIT_TEST(testLibtorrentControlFileLoadRestoresMetadataPauseApplied);
  CPPUNIT_TEST(testLibtorrentSessionTracksActiveTorrent);
  CPPUNIT_TEST(testLibtorrentDuplicateTorrentFailsOnlySecondGroup);
  CPPUNIT_TEST(testLibtorrentMagnetPauseMetadataStopsAfterMetadata);
  CPPUNIT_TEST(testLibtorrentSelectedMagnetRestartsAfterMetadata);
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
  void testCurlTlsTrustOptions();
  void testCurlProxyModeControlsEnvironmentProxy();
  void testCurlHttpRetryableErrors();
  void testCurlMetadataHeadFailureFallbackPolicy();
  void testFinishedHttpDownloadQueuesWholeFileChecksum();
  void testInitiateConnectionFactoryUsesCurlForHttp();
  void testInitiateConnectionFactoryUsesCurlForFtpFamily();
  void testHttpAdaptiveCommandLimit();
  void testHttpAdaptiveInitialHandoffFillsWindow();
  void testHttpAdaptiveSuccessFillsExpandedWindow();
  void testHttpRateLimitKeepsRangeAndCapsConcurrency();
  void testHttpRangeDowngradeDisablesRangedConcurrency();
  void testHttpRangeDowngradeRejectsExtraActiveStreamRetries();
  void testFtpDoesNotUseHttpAdaptiveCommandLimit();
#ifdef ENABLE_BITTORRENT
  void testCreateInitialCommandUsesLibtorrentRuntime();
  void testLibtorrentCommandLoadsTorrentMetadata();
  void testLibtorrentVerifiedProgressOverridesPieceStorage();
  void testLibtorrentResumeProgressUsedWhileLiveStatusIsEmpty();
  void testLibtorrentAllDownloadFinishedUsesSeedingStatus();
  void testLibtorrentActiveSeedingIsStillInProgress();
  void testLibtorrentPartialSharingIsStillInProgress();
  void testLibtorrentFinishedRemovesControlFileEvenWithForceSave();
  void testLibtorrentResumeDataRoundTrip();
  void testLibtorrentEmptyResumeDataDoesNotOverwriteControlFile();
  void testLibtorrentControlFileLoadRestoresResumeStatus();
  void testLibtorrentControlFileLoadRestoresMetadataPauseApplied();
  void testLibtorrentSessionTracksActiveTorrent();
  void testLibtorrentDuplicateTorrentFailsOnlySecondGroup();
  void testLibtorrentMagnetPauseMetadataStopsAfterMetadata();
  void testLibtorrentSelectedMagnetRestartsAfterMetadata();
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

void RequestGroupTest::testCurlTlsTrustOptions()
{
  const auto options = CurlDownloadCommand::platformSslTrustOptions();
#if (defined(_WIN32) || defined(__APPLE__)) && defined(CURLSSLOPT_NATIVE_CA)
  CPPUNIT_ASSERT(options & CURLSSLOPT_NATIVE_CA);
#else
  CPPUNIT_ASSERT_EQUAL(0L, options);
#endif
}

void RequestGroupTest::testCurlProxyModeControlsEnvironmentProxy()
{
  auto parent = std::make_shared<Option>();
  parent->put(PREF_PROXY_MODE, V_AUTO);
  parent->put(PREF_ALL_PROXY, "http://proxy.example:8080/");

  Option op;
  op.setParent(parent);
  CPPUNIT_ASSERT(!CurlDownloadCommand::shouldDisableCurlProxy(&op));

  op.put(PREF_PROXY_MODE, V_DIRECT);
  CPPUNIT_ASSERT(CurlDownloadCommand::shouldDisableCurlProxy(&op));

  op.put(PREF_PROXY_MODE, V_MANUAL);
  CPPUNIT_ASSERT(CurlDownloadCommand::shouldDisableCurlProxy(&op));

  op.put(PREF_HTTPS_PROXY, "http://manual.example:8080/");
  CPPUNIT_ASSERT(CurlDownloadCommand::shouldDisableCurlProxy(&op));
}

void RequestGroupTest::testCurlHttpRetryableErrors()
{
  CPPUNIT_ASSERT(
      CurlDownloadCommand::isRetryableHttpCurlError(CURLE_OPERATION_TIMEDOUT));
  CPPUNIT_ASSERT(
      CurlDownloadCommand::isRetryableHttpCurlError(CURLE_COULDNT_CONNECT));
  CPPUNIT_ASSERT(CurlDownloadCommand::isRetryableHttpCurlError(CURLE_RECV_ERROR));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::isRetryableHttpCurlError(CURLE_SSL_CACERT_BADFILE));
  CPPUNIT_ASSERT(!CurlDownloadCommand::isRetryableHttpCurlError(CURLE_OK));
}

void RequestGroupTest::testCurlMetadataHeadFailureFallbackPolicy()
{
  CPPUNIT_ASSERT(CurlDownloadCommand::shouldFallbackMetadataHeadErrorToRangeProbe(
      true, false, false, true, CURLE_GOT_NOTHING));
  CPPUNIT_ASSERT(CurlDownloadCommand::shouldFallbackMetadataHeadErrorToRangeProbe(
      true, false, false, true, CURLE_RECV_ERROR));

  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataHeadErrorToRangeProbe(
          true, false, true, true, CURLE_GOT_NOTHING));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataHeadErrorToRangeProbe(
          true, true, false, true, CURLE_GOT_NOTHING));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataHeadErrorToRangeProbe(
          true, false, false, true, CURLE_SSL_CACERT_BADFILE));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataHeadErrorToRangeProbe(
          true, false, false, false, CURLE_GOT_NOTHING));

  CPPUNIT_ASSERT(CurlDownloadCommand::shouldFallbackMetadataHeadStatusToRangeProbe(
      true, false, false, true, 405));
  CPPUNIT_ASSERT(CurlDownloadCommand::shouldFallbackMetadataHeadStatusToRangeProbe(
      true, false, false, true, 501));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataHeadStatusToRangeProbe(
          true, false, true, true, 405));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataHeadStatusToRangeProbe(
          true, false, false, true, 404));

  CPPUNIT_ASSERT(CurlDownloadCommand::shouldFallbackMetadataRangeProbeToFullDownload(
      true, true, 200));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataRangeProbeToFullDownload(
          true, true, 206));
  CPPUNIT_ASSERT(
      !CurlDownloadCommand::shouldFallbackMetadataRangeProbeToFullDownload(
          true, false, 200));
}

void RequestGroupTest::testFinishedHttpDownloadQueuesWholeFileChecksum()
{
  auto path = std::string(A2_TEST_OUT_DIR) +
              "/aria2_RequestGroupTest_finished_http_checksum";
  File(path).remove();
  File(path + DefaultProgressInfoFile::getSuffix()).remove();
  createFile(path, 1_k);

  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);

  auto ctx = std::make_shared<DownloadContext>(1_k, 1_k, path);
  auto digest = std::string(40, '0');
  ctx->setDigest("sha-1", util::fromHex(digest.begin(), digest.end()));
  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  group.loadAndOpenFile(std::make_shared<DefaultProgressInfoFile>(
      ctx, group.getPieceStorage(), option_.get()));
  group.getPieceStorage()->markAllPiecesDone();

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setCheckIntegrityMan(make_unique<CheckIntegrityMan>());

  CPPUNIT_ASSERT(group.queueChecksumValidationIfNeeded(&engine));
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       engine.getCheckIntegrityMan()->countEntryInQueue());
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

void RequestGroupTest::testHttpAdaptiveCommandLimit()
{
  option_->put(PREF_SPLIT, "64");
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);

  auto group = createRequestGroup(
      1_k, 64_k,
      std::string(A2_TEST_OUT_DIR) +
          "/aria2_RequestGroupTest_http_adaptive_limit",
      "https://example.test/file", option_);
  group->setNumConcurrentCommand(64);
  group->initPieceStorage();

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());

  std::vector<std::unique_ptr<Command>> commands;
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)4, commands.size());

  commands.clear();
  group->noteHttpSegmentSuccess();
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)8, commands.size());

  commands.clear();
  group->noteHttpSegmentFailure();
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)4, commands.size());
}

void RequestGroupTest::testHttpAdaptiveInitialHandoffFillsWindow()
{
  option_->put(PREF_SPLIT, "64");
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);

  auto group = createRequestGroup(
      1_k, 64_k,
      std::string(A2_TEST_OUT_DIR) +
          "/aria2_RequestGroupTest_http_adaptive_initial_handoff",
      "https://example.test/file", option_);
  group->setNumConcurrentCommand(64);
  group->initPieceStorage();
  group->increaseStreamCommand();

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());

  std::vector<std::unique_ptr<Command>> commands;
  group->createNextCommandForCompletedStream(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)4, commands.size());
}

void RequestGroupTest::testHttpAdaptiveSuccessFillsExpandedWindow()
{
  option_->put(PREF_SPLIT, "64");
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);

  auto group = createRequestGroup(
      1_k, 64_k,
      std::string(A2_TEST_OUT_DIR) +
          "/aria2_RequestGroupTest_http_adaptive_success_fill",
      "https://example.test/file", option_);
  group->setNumConcurrentCommand(64);
  group->initPieceStorage();
  group->increaseStreamCommand();
  group->increaseStreamCommand();
  group->increaseStreamCommand();
  group->increaseStreamCommand();
  group->noteHttpSegmentSuccess();

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());

  std::vector<std::unique_ptr<Command>> commands;
  group->createNextCommandForCompletedStream(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)5, commands.size());
}

void RequestGroupTest::testHttpRateLimitKeepsRangeAndCapsConcurrency()
{
  option_->put(PREF_SPLIT, "64");
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);

  auto group = createRequestGroup(
      1_k, 64_k,
      std::string(A2_TEST_OUT_DIR) +
          "/aria2_RequestGroupTest_http_rate_limit",
      "https://example.test/file", option_);
  group->setNumConcurrentCommand(64);
  group->initPieceStorage();

  group->noteHttpRateLimited();

  CPPUNIT_ASSERT(group->shouldUseHttpRange());
  CPPUNIT_ASSERT_EQUAL(1, group->getEffectiveStreamCommandLimit());

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());

  std::vector<std::unique_ptr<Command>> commands;
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)1, commands.size());

  commands.clear();
  group->noteHttpSegmentSuccess();
  group->noteHttpSegmentSuccess();
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)2, commands.size());
}

void RequestGroupTest::testHttpRangeDowngradeDisablesRangedConcurrency()
{
  option_->put(PREF_SPLIT, "64");
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);

  auto group = createRequestGroup(
      1_k, 64_k,
      std::string(A2_TEST_OUT_DIR) +
          "/aria2_RequestGroupTest_http_range_downgrade",
      "https://example.test/file", option_);
  group->setNumConcurrentCommand(64);
  group->initPieceStorage();

  group->disableHttpRangeForDownload();

  CPPUNIT_ASSERT(!group->shouldUseHttpRange());
  CPPUNIT_ASSERT_EQUAL(1, group->getEffectiveStreamCommandLimit());

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());

  std::vector<std::unique_ptr<Command>> commands;
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)1, commands.size());
}

void RequestGroupTest::testHttpRangeDowngradeRejectsExtraActiveStreamRetries()
{
  option_->put(PREF_SPLIT, "64");
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);

  auto group = createRequestGroup(
      1_k, 64_k,
      std::string(A2_TEST_OUT_DIR) +
          "/aria2_RequestGroupTest_http_range_active_downgrade",
      "https://example.test/file", option_);
  group->setNumConcurrentCommand(64);
  group->initPieceStorage();

  group->increaseStreamCommand();
  group->increaseStreamCommand();
  group->increaseStreamCommand();
  group->disableHttpRangeForDownload();
  auto oldGeneration = group->getHttpRangeGeneration() - 1;
  auto currentGeneration = group->getHttpRangeGeneration();

  CPPUNIT_ASSERT(group->claimStreamRetrySlot(oldGeneration));
  group->decreaseStreamCommand();
  CPPUNIT_ASSERT(!group->claimStreamRetrySlot(oldGeneration));
  group->decreaseStreamCommand();
  CPPUNIT_ASSERT(!group->claimStreamRetrySlot(oldGeneration));
  group->decreaseStreamCommand();
  CPPUNIT_ASSERT(group->claimStreamRetrySlot(currentGeneration));

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());

  std::vector<std::unique_ptr<Command>> commands;
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)1, commands.size());
}

void RequestGroupTest::testFtpDoesNotUseHttpAdaptiveCommandLimit()
{
  option_->put(PREF_SPLIT, "64");
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);

  auto group = createRequestGroup(
      1_k, 64_k,
      std::string(A2_TEST_OUT_DIR) +
          "/aria2_RequestGroupTest_ftp_no_http_adaptive_limit",
      "ftp://example.test/file", option_);
  group->setNumConcurrentCommand(64);
  group->initPieceStorage();

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());

  std::vector<std::unique_ptr<Command>> commands;
  group->createNextCommand(commands, &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)64, commands.size());
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
          std::vector<std::string>{},
          "test_outdir/3d366ed505b977fc61c9a6ee01e96329.aria2"));

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
          A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
          "test_outdir/0101010101010101010101010101010101010101.aria2"));

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
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
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

void RequestGroupTest::testLibtorrentResumeProgressUsedWhileLiveStatusIsEmpty()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  attrsPtr->status.hasStatus = true;
  attrsPtr->status.checking = false;
  attrsPtr->status.totalLength = 0;
  attrsPtr->status.completedLength = 0;
  attrsPtr->resumeStatus.hasStatus = true;
  attrsPtr->resumeStatus.totalLength = 100_k;
  attrsPtr->resumeStatus.completedLength = 99_k;

  CPPUNIT_ASSERT_EQUAL((int64_t)100_k, group.getTotalLength());
  CPPUNIT_ASSERT_EQUAL((int64_t)99_k, group.getCompletedLength());
}

void RequestGroupTest::testLibtorrentAllDownloadFinishedUsesSeedingStatus()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  group.getPieceStorage()->markAllPiecesDone();

  attrsPtr->status.hasStatus = true;
  attrsPtr->status.complete = true;
  attrsPtr->status.seeding = false;
  CPPUNIT_ASSERT(group.allDownloadFinished());

  attrsPtr->status.seeding = true;
  CPPUNIT_ASSERT(!group.allDownloadFinished());
}

void RequestGroupTest::testLibtorrentActiveSeedingIsStillInProgress()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  group.getPieceStorage()->markAllPiecesDone();

  attrsPtr->status.hasStatus = true;
  attrsPtr->status.complete = false;
  attrsPtr->status.seeding = true;
  attrsPtr->status.sharing = true;

  CPPUNIT_ASSERT(!group.downloadFinished());
  CPPUNIT_ASSERT(!group.allDownloadFinished());
  CPPUNIT_ASSERT(group.isSeeder());
}

void RequestGroupTest::testLibtorrentPartialSharingIsStillInProgress()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.initPieceStorage();
  group.getPieceStorage()->markAllPiecesDone();

  attrsPtr->status.hasStatus = true;
  attrsPtr->status.complete = false;
  attrsPtr->status.seeding = false;
  attrsPtr->status.sharing = true;

  CPPUNIT_ASSERT(!group.downloadFinished());
  CPPUNIT_ASSERT(!group.allDownloadFinished());
  CPPUNIT_ASSERT(group.isSeeder());
}

void RequestGroupTest::testLibtorrentFinishedRemovesControlFileEvenWithForceSave()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));
  option_->put(PREF_FORCE_SAVE, A2_V_TRUE);

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  attrsPtr->status.hasStatus = true;
  attrsPtr->status.complete = false;
  attrsPtr->status.sharing = true;
  CPPUNIT_ASSERT(!group.shouldRemoveControlFileOnFinish());

  attrsPtr->status.complete = true;
  attrsPtr->status.sharing = false;
  CPPUNIT_ASSERT(group.shouldRemoveControlFileOnFinish());
}

void RequestGroupTest::testLibtorrentResumeDataRoundTrip()
{
  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET,
      "magnet:?xt=urn:btih:0101010101010101010101010101010101010101", "",
      std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
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

void RequestGroupTest::testLibtorrentEmptyResumeDataDoesNotOverwriteControlFile()
{
  const std::string controlPath =
      std::string(A2_TEST_OUT_DIR) +
      "/empty-resume-data.aria2";
  File(controlPath).remove();

  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET,
      "magnet:?xt=urn:btih:0101010101010101010101010101010101010101", "",
      std::vector<std::string>{}, controlPath);
  auto attrsPtr = attrs.get();
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto torrentParams = lt::load_torrent_file(A2_TEST_DIR "/single.torrent");
  auto resumeBuf = lt::write_resume_data_buf(torrentParams);
  const std::string resumeData(resumeBuf.begin(), resumeBuf.end());
  attrsPtr->setResumeData(resumeData);
  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.setProgressInfoFile(
      std::make_shared<LibtorrentProgressInfoFile>(ctx));
  group.saveControlFile();
  auto savedSize = File(controlPath).size();

  attrsPtr->setResumeData("");
  group.saveControlFile();

  CPPUNIT_ASSERT_EQUAL(savedSize, File(controlPath).size());
  LibtorrentProgressInfoFile controlFile(ctx);
  controlFile.load();
  CPPUNIT_ASSERT_EQUAL(resumeData, attrsPtr->getResumeData());
  File(controlPath).remove();
}

void RequestGroupTest::testLibtorrentControlFileLoadRestoresResumeStatus()
{
  const std::string controlPath =
      std::string(A2_TEST_OUT_DIR) +
      "/0101010101010101010101010101010101010101.aria2";
  File(controlPath).remove();

  auto torrentParams = lt::load_torrent_file(A2_TEST_DIR "/single.torrent");
  torrentParams.have_pieces.resize(torrentParams.ti->num_pieces(), true);
  auto resumeData = lt::write_resume_data_buf(torrentParams);

  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      controlPath, torrentParams.ti->info_hashes().v1.to_string());
  auto attrsPtr = attrs.get();
  attrsPtr->setResumeData(std::string(resumeData.begin(), resumeData.end()));
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  LibtorrentProgressInfoFile controlFile(ctx);
  controlFile.save();
  attrsPtr->setResumeData("");
  attrsPtr->resumeStatus = LibtorrentAttribute::Status();
  controlFile.load();

  CPPUNIT_ASSERT(attrsPtr->resumeStatus.hasStatus);
  CPPUNIT_ASSERT(attrsPtr->resumeStatus.totalLength > 0);
  CPPUNIT_ASSERT_EQUAL(attrsPtr->resumeStatus.totalLength,
                       attrsPtr->resumeStatus.completedLength);
  File(controlPath).remove();
}

void RequestGroupTest::testLibtorrentControlFileLoadRestoresMetadataPauseApplied()
{
  const std::string controlPath =
      std::string(A2_TEST_OUT_DIR) +
      "/0101010101010101010101010101010101010101.aria2";
  File(controlPath).remove();

  auto torrentParams = lt::load_torrent_file(A2_TEST_DIR "/single.torrent");
  auto resumeData = lt::write_resume_data_buf(torrentParams);

  auto ctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET,
      "magnet:?xt=urn:btih:" +
          util::toHex(torrentParams.ti->info_hashes().v1.to_string()),
      "", std::vector<std::string>{}, controlPath,
      torrentParams.ti->info_hashes().v1.to_string());
  auto attrsPtr = attrs.get();
  attrsPtr->pauseAfterMetadata = true;
  attrsPtr->setResumeData(std::string(resumeData.begin(), resumeData.end()));
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  LibtorrentProgressInfoFile controlFile(ctx);
  controlFile.save();
  attrsPtr->setResumeData("");
  attrsPtr->resumeStatus = LibtorrentAttribute::Status();
  attrsPtr->metadataPauseApplied = false;
  controlFile.load();

  CPPUNIT_ASSERT(attrsPtr->resumeStatus.hasMetadata);
  CPPUNIT_ASSERT(attrsPtr->metadataPauseApplied);
  File(controlPath).remove();
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
          std::vector<std::string>{},
          "test_outdir/0101010101010101010101010101010101010101.aria2"));

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

void RequestGroupTest::testLibtorrentDuplicateTorrentFailsOnlySecondGroup()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);
  option_->put(PREF_LISTEN_PORT, "0");
  option_->put(PREF_DHT_LISTEN_PORT, "0");

  auto createGroup = [this]() {
    auto ctx = std::make_shared<DownloadContext>(1_k, 0, "magnet");
    ctx->markTotalLengthIsUnknown();
    ctx->setAttribute(
        CTX_ATTR_LIBTORRENT,
        make_unique<LibtorrentAttribute>(
            LibtorrentAttribute::SourceType::MAGNET,
            "magnet:?xt=urn:btih:0101010101010101010101010101010101010101",
            "", std::vector<std::string>{},
            "test_outdir/0101010101010101010101010101010101010101.aria2"));
    auto group = make_unique<RequestGroup>(GroupId::create(), option_);
    group->setDownloadContext(ctx);
    return group;
  };

  auto eventPoll = make_unique<SelectEventPoll>();
  auto engine = make_unique<DownloadEngine>(std::move(eventPoll));
  engine->setOption(option_.get());

  auto firstGroup = createGroup();
  auto first = make_unique<LibtorrentCommand>(engine->newCUID(),
                                              firstGroup.get(), engine.get());
  first->preProcess();

  auto secondGroup = createGroup();
  auto second = make_unique<LibtorrentCommand>(engine->newCUID(),
                                               secondGroup.get(), engine.get());
  second->preProcess();

  CPPUNIT_ASSERT(engine->getLibtorrentSession().hasTorrent(firstGroup->getGID()));
  CPPUNIT_ASSERT(!engine->getLibtorrentSession().hasTorrent(secondGroup->getGID()));
  CPPUNIT_ASSERT(secondGroup->isHaltRequested());
  auto result = secondGroup->createDownloadResult();
  CPPUNIT_ASSERT_EQUAL(error_code::DUPLICATE_INFO_HASH, result->result);
}

void RequestGroupTest::testLibtorrentMagnetPauseMetadataStopsAfterMetadata()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);
  option_->put(PREF_LISTEN_PORT, "0");
  option_->put(PREF_DHT_LISTEN_PORT, "0");

  const std::string torrentPath = A2_TEST_DIR "/single.torrent";
  auto torrentParams = lt::load_torrent_file(torrentPath);
  auto metadata = loadTorrentInfoMetadata(torrentPath);
  auto magnet = "magnet:?xt=urn:btih:" +
                util::toHex(torrentParams.ti->info_hashes().v1.to_string());
  auto ctx = std::make_shared<DownloadContext>(0, 0, "magnet");
  ctx->markTotalLengthIsUnknown();
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET, magnet, "",
      std::vector<std::string>{},
      A2_TEST_OUT_DIR "/0101010101010101010101010101010101010101.aria2",
      torrentParams.ti->info_hashes().v1.to_string());
  auto attrsPtr = attrs.get();
  attrsPtr->pauseAfterMetadata = true;
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);

  auto eventPoll = make_unique<SelectEventPoll>();
  auto engine = make_unique<DownloadEngine>(std::move(eventPoll));
  engine->setOption(option_.get());
  engine->setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
  group.setRequestGroupMan(engine->getRequestGroupMan().get());
  group.setState(RequestGroup::STATE_ACTIVE);

  LibtorrentCommand command(engine->newCUID(), &group, engine.get());
  command.preProcess();
  auto handle = engine->getLibtorrentSession().nativeSession().find_torrent(
      torrentParams.ti->info_hashes().v1);
  CPPUNIT_ASSERT(handle.is_valid());
  handle.set_metadata(lt::span<char const>(
      metadata.data(), static_cast<int>(metadata.size())));
  command.preProcess();

  CPPUNIT_ASSERT(ctx->knowsTotalLength());
  CPPUNIT_ASSERT(!ctx->getFileEntries().empty());
  CPPUNIT_ASSERT_EQUAL((int64_t)0, attrsPtr->status.completedLength);
  CPPUNIT_ASSERT(attrsPtr->metadataPauseApplied);
  CPPUNIT_ASSERT(group.isPauseRequested());
  CPPUNIT_ASSERT(command.execute());

  lt::error_code ec;
  auto params = lt::read_resume_data(
      lt::span<char const>(attrsPtr->getResumeData().data(),
                           static_cast<int>(attrsPtr->getResumeData().size())),
      ec);
  CPPUNIT_ASSERT(!ec);
  CPPUNIT_ASSERT(params.ti);
}

void RequestGroupTest::testLibtorrentSelectedMagnetRestartsAfterMetadata()
{
  option_->put(PREF_DIR, A2_TEST_OUT_DIR);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);
  option_->put(PREF_LISTEN_PORT, "0");
  option_->put(PREF_DHT_LISTEN_PORT, "0");

  const std::string torrentPath = A2_TEST_DIR "/test.torrent";
  auto torrentParams = lt::load_torrent_file(torrentPath);
  auto metadata = loadTorrentInfoMetadata(torrentPath);
  auto magnet = "magnet:?xt=urn:btih:" +
                util::toHex(torrentParams.ti->info_hashes().v1.to_string());
  auto metadataOnly = lt::parse_magnet_uri(magnet);
  metadataOnly.flags |= lt::torrent_flags::upload_mode;
  metadataOnly.flags |= lt::torrent_flags::default_dont_download;
  auto resumeData = lt::write_resume_data_buf(metadataOnly);

  auto ctx = std::make_shared<DownloadContext>(0, 0, "magnet");
  ctx->markTotalLengthIsUnknown();
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET, magnet, "",
      std::vector<std::string>{},
      A2_TEST_OUT_DIR "/0101010101010101010101010101010101010101.aria2",
      torrentParams.ti->info_hashes().v1.to_string());
  auto attrsPtr = attrs.get();
  attrsPtr->pauseAfterMetadata = true;
  attrsPtr->metadataPauseApplied = true;
  attrsPtr->selectedFiles = "2";
  attrsPtr->setResumeData(std::string(resumeData.begin(), resumeData.end()));
  ctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  RequestGroup group(GroupId::create(), option_);
  group.setDownloadContext(ctx);
  group.setState(RequestGroup::STATE_ACTIVE);

  auto engine = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
  engine->setOption(option_.get());

  LibtorrentCommand command(engine->newCUID(), &group, engine.get());
  command.preProcess();
  auto handle = engine->getLibtorrentSession().nativeSession().find_torrent(
      torrentParams.ti->info_hashes().v1);
  CPPUNIT_ASSERT(handle.is_valid());
  handle.set_metadata(lt::span<char const>(
      metadata.data(), static_cast<int>(metadata.size())));
  command.preProcess();
  for (int i = 0; i < 10 && attrsPtr->filePrioritiesPending; ++i) {
    command.preProcess();
  }
  command.preProcess();

  CPPUNIT_ASSERT(!group.isPauseRequested());
  CPPUNIT_ASSERT(!group.isRestartRequested());

  lt::error_code ec;
  auto params = lt::read_resume_data(
      lt::span<char const>(attrsPtr->getResumeData().data(),
                           static_cast<int>(attrsPtr->getResumeData().size())),
      ec);
  CPPUNIT_ASSERT(!ec);
  CPPUNIT_ASSERT(params.ti);
  CPPUNIT_ASSERT(params.piece_priorities.empty());
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrsPtr->filePriorities.size());
  CPPUNIT_ASSERT_EQUAL(0, attrsPtr->filePriorities[0]);
  CPPUNIT_ASSERT_EQUAL(4, attrsPtr->filePriorities[1]);
  CPPUNIT_ASSERT(attrsPtr->contentStarted);
  CPPUNIT_ASSERT(attrsPtr->status.totalLength > 0);
  CPPUNIT_ASSERT_EQUAL((int64_t)0, attrsPtr->status.completedLength);
}
#endif // ENABLE_BITTORRENT

} // namespace aria2
