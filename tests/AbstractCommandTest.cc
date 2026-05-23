#include "AbstractCommand.h"

#include <chrono>
#include <iostream>
#include <cppunit/extensions/HelperMacros.h>

#include "DownloadContext.h"
#include "DownloadCommand.h"
#include "DownloadEngine.h"
#include "FileEntry.h"
#include "Request.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SelectEventPoll.h"
#include "SegmentMan.h"
#include "DlRetryEx.h"
#include "Option.h"
#include "prefs.h"
#include "Piece.h"
#include "PieceStorage.h"
#include "SocketCore.h"
#include "SocketRecvBuffer.h"
#include "TestUtil.h"
#include "wallclock.h"

namespace aria2 {

namespace {
std::shared_ptr<Option> createOption()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, A2_TEST_OUT_DIR);
  option->put(PREF_FILE_ALLOCATION, V_NONE);
  option->put(PREF_MAX_DOWNLOAD_RESULT, "5");
  option->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT, "0");
  option->put(PREF_MAX_OVERALL_UPLOAD_LIMIT, "0");
  option->put(PREF_MAX_TRIES, "1");
  option->put(PREF_RETRY_WAIT, "0");
  option->put(PREF_BT_MAX_OPEN_FILES, "100");
  option->put(PREF_ED2K_UPLOAD_SLOTS, "3");
  option->put(PREF_ENABLE_RPC, A2_V_FALSE);
  option->put(PREF_SELECT_LEAST_USED_HOST, A2_V_FALSE);
  option->put(PREF_MIN_SPLIT_SIZE, "1M");
  option->put(PREF_STREAM_PIECE_SELECTOR, A2_V_DEFAULT);
  option->put(PREF_REALTIME_CHUNK_CHECKSUM, A2_V_FALSE);
  option->put(PREF_ALWAYS_RESUME, A2_V_FALSE);
  option->put(PREF_MAX_RESUME_FAILURE_TRIES, "0");
  option->put(PREF_LOWEST_SPEED_LIMIT, "0");
  option->put(PREF_TIMEOUT, "1");
  return option;
}

class ZeroProgressPieceStorage : public PieceStorage {
public:
  ZeroProgressPieceStorage() : piece_(std::make_shared<Piece>(0, 1_k)) {}

  bool hasMissingUnusedPiece() CXX11_OVERRIDE { return true; }

  std::shared_ptr<Piece>
  getMissingPiece(size_t, const unsigned char*, size_t, cuid_t) CXX11_OVERRIDE
  {
    return piece_;
  }

  std::shared_ptr<Piece> getMissingPiece(size_t, cuid_t) CXX11_OVERRIDE
  {
    return piece_;
  }

  std::shared_ptr<Piece> getPiece(size_t) CXX11_OVERRIDE { return piece_; }

  void completePiece(const std::shared_ptr<Piece>&) CXX11_OVERRIDE {}

  void cancelPiece(const std::shared_ptr<Piece>&, cuid_t) CXX11_OVERRIDE {}

  bool hasPiece(size_t) CXX11_OVERRIDE { return false; }

  bool isPieceUsed(size_t) CXX11_OVERRIDE { return false; }

  int64_t getTotalLength() CXX11_OVERRIDE { return 1_k; }

  int64_t getFilteredTotalLength() CXX11_OVERRIDE { return 1_k; }

  int64_t getCompletedLength() CXX11_OVERRIDE { return 0; }

  int64_t getInFlightCompletedLength() CXX11_OVERRIDE { return 0; }

  int64_t getFilteredCompletedLength() CXX11_OVERRIDE { return 0; }

  int64_t getFilteredInFlightCompletedLength() CXX11_OVERRIDE { return 0; }

  void initStorage() CXX11_OVERRIDE {}

  void setupFileFilter() CXX11_OVERRIDE {}

  void clearFileFilter() CXX11_OVERRIDE {}

  bool downloadFinished() CXX11_OVERRIDE { return false; }

  bool allDownloadFinished() CXX11_OVERRIDE { return false; }

  void setBitfield(const unsigned char*, size_t) CXX11_OVERRIDE {}

  size_t getBitfieldLength() CXX11_OVERRIDE { return 0; }

  const unsigned char* getBitfield() CXX11_OVERRIDE { return nullptr; }

  void setEndGamePieceNum(size_t) CXX11_OVERRIDE {}

  bool isSelectiveDownloadingMode() CXX11_OVERRIDE { return false; }

  bool isEndGame() CXX11_OVERRIDE { return false; }

  void enterEndGame() CXX11_OVERRIDE {}

  std::shared_ptr<DiskAdaptor> getDiskAdaptor() CXX11_OVERRIDE
  {
    return nullptr;
  }

  WrDiskCache* getWrDiskCache() CXX11_OVERRIDE { return nullptr; }

  void flushWrDiskCacheEntry(bool) CXX11_OVERRIDE {}

  int32_t getPieceLength(size_t) CXX11_OVERRIDE { return 1_k; }

  void advertisePiece(cuid_t, size_t, Timer) CXX11_OVERRIDE {}

  uint64_t getAdvertisedPieceIndexes(std::vector<size_t>&, cuid_t,
                                     uint64_t) CXX11_OVERRIDE
  {
    return 0;
  }

  void removeAdvertisedPiece(const Timer&) CXX11_OVERRIDE {}

  void markAllPiecesDone() CXX11_OVERRIDE {}

  void markPiecesDone(int64_t) CXX11_OVERRIDE {}

  void markPieceMissing(size_t) CXX11_OVERRIDE {}

  void addInFlightPiece(
      const std::vector<std::shared_ptr<Piece>>&) CXX11_OVERRIDE
  {
  }

  size_t countInFlightPiece() CXX11_OVERRIDE { return 0; }

  void getInFlightPieces(std::vector<std::shared_ptr<Piece>>&) CXX11_OVERRIDE
  {
  }

  void addPieceStats(size_t) CXX11_OVERRIDE {}

  void addPieceStats(const unsigned char*, size_t) CXX11_OVERRIDE {}

  void subtractPieceStats(const unsigned char*, size_t) CXX11_OVERRIDE {}

  void updatePieceStats(const unsigned char*, size_t,
                        const unsigned char*) CXX11_OVERRIDE
  {
  }

  size_t getNextUsedIndex(size_t) CXX11_OVERRIDE { return 0; }

  void onDownloadIncomplete() CXX11_OVERRIDE {}

private:
  std::shared_ptr<Piece> piece_;
};

class ZeroProgressDownloadCommand : public DownloadCommand {
public:
  ZeroProgressDownloadCommand(cuid_t cuid, const std::shared_ptr<Request>& req,
                              const std::shared_ptr<FileEntry>& fileEntry,
                              RequestGroup* requestGroup, DownloadEngine* e)
      : DownloadCommand(cuid, req, fileEntry, requestGroup, e,
                        std::make_shared<SocketCore>(),
                        std::make_shared<SocketRecvBuffer>(
                            std::make_shared<SocketCore>()))
  {
  }

  void runZeroProgressWatchdog()
  {
    checkProgressTimeout();
  }

private:
  bool executeInternal() CXX11_OVERRIDE { return false; }

  int64_t getRequestEndOffset() const CXX11_OVERRIDE { return 1_k; }
};
} // namespace

class AbstractCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(AbstractCommandTest);
  CPPUNIT_TEST(testGetProxyUri);
  CPPUNIT_TEST(testNativeDownloadProgressTimeout);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testGetProxyUri();
  void testNativeDownloadProgressTimeout();
};

CPPUNIT_TEST_SUITE_REGISTRATION(AbstractCommandTest);

void AbstractCommandTest::testGetProxyUri()
{
  Option op;
  CPPUNIT_ASSERT_EQUAL(std::string(), getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY, "http://hu:hp@httpproxy/");
  op.put(PREF_FTP_PROXY, "ftp://fu:fp@ftpproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hu:hp@httpproxy/"),
                       getProxyUri("http", &op));
  CPPUNIT_ASSERT_EQUAL(std::string("ftp://fu:fp@ftpproxy/"),
                       getProxyUri("ftp", &op));

  op.put(PREF_ALL_PROXY, "http://au:ap@allproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://au:ap@allproxy/"),
                       getProxyUri("https", &op));

  op.put(PREF_ALL_PROXY_USER, "aunew");
  op.put(PREF_ALL_PROXY_PASSWD, "apnew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://aunew:apnew@allproxy/"),
                       getProxyUri("https", &op));

  op.put(PREF_HTTPS_PROXY, "http://hsu:hsp@httpsproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hsu:hsp@httpsproxy/"),
                       getProxyUri("https", &op));

  CPPUNIT_ASSERT_EQUAL(std::string(), getProxyUri("unknown", &op));

  op.put(PREF_HTTP_PROXY_USER, "hunew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hunew:hp@httpproxy/"),
                       getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY_PASSWD, "hpnew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hunew:hpnew@httpproxy/"),
                       getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY_USER, "");
  CPPUNIT_ASSERT_EQUAL(std::string("http://httpproxy/"),
                       getProxyUri("http", &op));
}

void AbstractCommandTest::testNativeDownloadProgressTimeout()
{
  auto option = createOption();
  auto dctx = std::make_shared<DownloadContext>(
      1_k, 1_k, std::string(A2_TEST_OUT_DIR) + "/abstract-command-stall.bin");
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  group->setDownloadContext(dctx);
  auto storage = std::make_shared<ZeroProgressPieceStorage>();
  group->setPieceStorage(storage);
  group->setSegmentMan(std::make_shared<SegmentMan>(dctx, storage));
  auto fileEntry = dctx->getFirstFileEntry();

  auto req = std::make_shared<Request>();
  CPPUNIT_ASSERT(req->setUri("http://example.org/abstract-command-stall.bin"));
  fileEntry->addUri(req->getUri());

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());

  ZeroProgressDownloadCommand command(1, req, fileEntry, group.get(), &engine);
  command.runZeroProgressWatchdog();
  global::wallclock().advance(2_s);

  try {
    command.runZeroProgressWatchdog();
    CPPUNIT_FAIL("expected timeout for stalled native download progress");
  }
  catch (DlRetryEx& ex) {
    CPPUNIT_ASSERT_EQUAL(error_code::TIME_OUT, ex.getErrorCode());
  }
}

} // namespace aria2
