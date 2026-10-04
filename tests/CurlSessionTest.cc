#include "GroupId.h"
#include "a2netcompat.h"
#include "error_code.h"
#include "stream/CurlHandle.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <thread>
#include "CurlSession.h"
#include "transport/CurlOptions.h"
#include "support/OutputName.h"

#include <algorithm>

#include "CurlDownload.h"
#include "CurlDownloadImpl.h"
#include "Command.h"
#include "ByteArrayDiskWriter.h"
#include "DownloadContext.h"
#include "DiskWriter.h"
#include "DownloadEngine.h"
#include "DownloadFailureException.h"
#include "Option.h"
#include "OptionParser.h"
#include "RequestGroup.h"
#include "SelectEventPoll.h"
#include "SocketCore.h"
#include "a2doctest.h"
#include "a2functional.h"
#include "prefs.h"
#include "wallclock.h"

namespace aria2 {

TEST_CASE("Output names preserve text and follow source precedence")
{
  Option option;
  CHECK_EQ(std::string(236, 'a') + ".zip",
           output::safeName(std::string(300, 'a') + ".zip"));
  const std::string url = "https://example.test/a%2520b.zip";
  CHECK_EQ(
      "report name.pdf",
      output::suggestedName(
          option, "https://example.test/redirection?filename=report+name.pdf"));
  CHECK_EQ("server.pdf",
           output::suggestedName(
               option, "https://example.test/redirection?filename=hint.pdf",
               "attachment; filename=server.pdf"));
  CHECK_EQ("report.pdf", output::suggestedName(
                             option, "https://cdn.example.test/opaque", {},
                             "https://example.test/get?filename=report.pdf"));
  CHECK_EQ("a%20b.zip", output::suggestedName(option, url));
  CHECK_EQ("report%20.zip",
           output::suggestedName(option, url,
                                 "attachment; filename=\"report%20.zip\""));
  CHECK_EQ(
      "report%20.zip",
      output::suggestedName(
          option, url,
          "attachment; filename=plain.zip; filename*=UTF-8''report%2520.zip"));
  CHECK_EQ("résumé.pdf",
           output::suggestedName(
               option, url,
               "attachment; filename=\"=?UTF-8?Q?r=C3=A9sum=C3=A9.pdf?=\""));
  CHECK_EQ(
      "Итоги_2026.docx",
      output::suggestedName(
          option, url,
          "attachment; filename=\"=?UTF-8?B?0JjRgtC+0LPQuF8yMDI2LmRvY3g=?=\""));
  CHECK_EQ(
      "%3D%3FUTF-8%3FQ%3Freport.pdf%3F%3D",
      output::suggestedName(
          option, url,
          "attachment; "
          "filename*=UTF-8''%253D%253FUTF-8%253FQ%253Freport.pdf%253F%253D"));
  option.put(PREF_FILENAME_HINT, "browser%20.zip");
  CHECK_EQ("server.zip", output::suggestedName(
                             option, url, "attachment; filename=server.zip"));
  option.put(PREF_FILENAME_HINT_SOURCE, "browser");
  CHECK_EQ(
      "browser%20.zip",
      output::suggestedName(option, url, "attachment; filename=server.zip"));
  option.put(PREF_MEDIA_FORMAT, "mkv");
  CHECK_EQ("browser%20.mkv", output::mediaName(option, url));
  option.put(PREF_OUT, "chosen.mp4");
  CHECK_EQ("chosen.mkv", output::mediaName(option, url));
  option.remove(PREF_OUT);
  option.put(PREF_FILENAME_HINT, "Episode 1.5");
  option.put(PREF_FILENAME_HINT_SOURCE, "title");
  CHECK_EQ("Episode 1.5.mkv", output::mediaName(option, url));
}

namespace {

class FailingDiskWriter final : public DiskWriter {
public:
  void initAndOpenFile(int64_t) override {}
  void openFile(int64_t) override {}
  void closeFile() override {}
  void openExistingFile(int64_t) override {}
  int64_t size() override { return 0; }

  void writeData(const unsigned char*, size_t, int64_t) override
  {
    throw DOWNLOAD_FAILURE_EXCEPTION2("Disk is full",
                                      error_code::NOT_ENOUGH_DISK_SPACE);
  }

  ssize_t readData(unsigned char*, size_t, int64_t) override { return 0; }
};

} // namespace

// Friendship is not inherited by doctest's generated fixtures. Keep private
// state access in this adapter rather than exposing engine internals publicly.
class CurlSessionTest {
public:
  void testBoundedInitialRequest();
  void testAbandonedNativeRequest();
  void testWriteErrorBoundary();
  void testEncodedRangeBoundary();
  void testDiskFailureDuringPause();
  void testOutputFilename();
  void testResponseIdentity();
  void testRangeOwnershipAndResponseBoundaries();
  void testNonzeroRangeRejectsCompleteResponse();
  void testUnsatisfiedRangeResponseForms();
  void testRetryableFailureClassification();
  void testFailureMessageUsesTheFailureLayer();
  void testShutdownWithLiveSocket();
  void testTailRecovery();
  void testConnectionRecovery();
  void testEndpointOrigin();
  void testValidatedEndpoint();
  void testNativeTimerPreservesEarlierWakeup();
  void respond(CurlHandle& handle, long code, const std::string& range = {},
               const std::string& etag = {}, const std::string& modified = {},
               const std::string& date = {});
};

TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testBoundedInitialRequest")
{
  testBoundedInitialRequest();
}

void CurlSessionTest::testBoundedInitialRequest()
{
  auto option = std::make_shared<Option>();
  OptionParser::getInstance()->parseDefaultValues(*option);
  option->put(PREF_STATE_DIR, A2_TEST_OUT_DIR "/curl-bounded-state");
  option->put(PREF_DIR, A2_TEST_OUT_DIR);
  option->put(PREF_OUT, "bounded.bin");
  option->put(PREF_STREAM_MAX_CONNECTIONS, "1");
  option->put(PREF_STREAM_MAX_RANGE_SIZE, std::to_string(1_m));
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  CurlSession session(option.get());
  auto download = std::make_shared<CurlDownload>(
      std::vector<std::string>{"http://127.0.0.1:9/bounded"});
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  group->setDownloadContext(std::make_shared<DownloadContext>(1_m, 0));
  group->setCurlDownload(download);
  auto command = session.start(download, group.get(), &engine);
  REQUIRE_EQ(1, download->impl_->handles.size());
  auto& handle = *download->impl_->handles.front();
  CHECK(handle.ranged);
  CHECK_LE(handle.lease.length(), 1_m);
  respond(handle, 200);
  CHECK_EQ(CurlResponseFailure::RangeUnsupported, handle.responseFailure);
  session.stop(download, false);
  session.discardRecovery(download);
}

TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testAbandonedNativeRequest")
{
  testAbandonedNativeRequest();
}

void CurlSessionTest::testAbandonedNativeRequest()
{
  // Preparation can abandon a request before registration with the session.
  // The task and its handle must not both release the same native resources.
  CurlDownload download({"https://example.test/abandoned"});
  auto handle = std::make_unique<CurlHandle>();
  handle->value = curl_easy_init();
  REQUIRE(handle->value);
  handle->headers = curl_slist_append(nullptr, "Accept: */*");
  REQUIRE(handle->headers);
  download.impl_->handles.push_back(std::move(handle));
}

TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testWriteErrorBoundary")
{
  testWriteErrorBoundary();
}
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testOutputFilename")
{
  testOutputFilename();
}
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testResponseIdentity")
{
  testResponseIdentity();
}
TEST_CASE_FIXTURE(CurlSessionTest,
                  "CurlSessionTest.testRangeOwnershipAndResponseBoundaries")
{
  testRangeOwnershipAndResponseBoundaries();
}
TEST_CASE_FIXTURE(CurlSessionTest,
                  "CurlSessionTest.testNonzeroRangeRejectsCompleteResponse")
{
  testNonzeroRangeRejectsCompleteResponse();
}
TEST_CASE_FIXTURE(CurlSessionTest,
                  "CurlSessionTest.testUnsatisfiedRangeResponseForms")
{
  testUnsatisfiedRangeResponseForms();
}
TEST_CASE_FIXTURE(CurlSessionTest,
                  "CurlSessionTest.testRetryableFailureClassification")
{
  testRetryableFailureClassification();
}
TEST_CASE_FIXTURE(CurlSessionTest,
                  "CurlSessionTest.testFailureMessageUsesTheFailureLayer")
{
  testFailureMessageUsesTheFailureLayer();
}
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testShutdownWithLiveSocket")
{
  testShutdownWithLiveSocket();
}
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testTailRecovery")
{
  testTailRecovery();
}
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testConnectionRecovery")
{
  testConnectionRecovery();
}
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testEndpointOrigin")
{
  testEndpointOrigin();
}
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testValidatedEndpoint")
{
  testValidatedEndpoint();
}
TEST_CASE_FIXTURE(CurlSessionTest,
                  "CurlSessionTest.testNativeTimerPreservesEarlierWakeup")
{
  testNativeTimerPreservesEarlierWakeup();
}

void CurlSessionTest::testNativeTimerPreservesEarlierWakeup()
{
  struct ObservedPoll final : EventPoll {
    timeval timeout{};
    void poll(const timeval& value) override { timeout = value; }
    bool addEvents(sock_t, Command*, EventType) override { return true; }
    bool deleteEvents(sock_t, Command*, EventType) override { return true; }
  };
  struct FinishCommand final : Command {
    FinishCommand() : Command(1) {}
    bool execute() override { return true; }
  };
  auto poll = make_unique<ObservedPoll>();
  auto* observed = poll.get();
  DownloadEngine engine(std::move(poll));
  Option option;
  option.put(PREF_STATE_DIR, A2_TEST_OUT_DIR "/curl-timer");
  CurlSession session(&option);
  session.engine_ = &engine;
  engine.setRefreshInterval(std::chrono::milliseconds(50));
  session.transport_.updateTimeout(10000);
  session.armTimeout();
  engine.setNoWait(false);
  engine.addCommand(make_unique<FinishCommand>());
  engine.run(true);
  CHECK_EQ(0, observed->timeout.tv_sec);
  CHECK_EQ(50000, observed->timeout.tv_usec);
}

void CurlSessionTest::testEndpointOrigin()
{
  CHECK(http::sameOrigin("https://origin.test/a",
                         "https://ORIGIN.test:443/b?token=next"));
  CHECK(http::sameOrigin("http://origin.test/a", "http://origin.test:80/b"));
  CHECK(!http::sameOrigin("https://origin.test/a", "http://origin.test/a"));
  CHECK(
      !http::sameOrigin("https://origin.test/a", "https://origin.test:444/a"));
  CHECK(!http::sameOrigin("https://origin.test/a", "https://cdn.test/a"));
  CHECK(!http::sameOrigin("invalid", "https://origin.test/a"));
}

void CurlSessionTest::testValidatedEndpoint()
{
  CurlDownload download({"https://origin.test/file"});
  auto& impl = *download.impl_;
  impl.endpoints.resize(1);
  auto& endpoint = impl.endpoints.front();
  endpoint.resolving = true;
  auto easy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
      curl_easy_init(), curl_easy_cleanup);
  REQUIRE(easy);
  REQUIRE_EQ(CURLE_OK, curl_easy_setopt(easy.get(), CURLOPT_URL,
                                        "https://cdn.test/file?token=one"));
  CurlHandle handle;
  handle.value = easy.release();
  handle.download = &download;
  handle.ranged = true;
  handle.lease = {0, 4096};
  respond(handle, 206, "bytes 1-4095/8192", "\"one\"");
  CurlHandle::rememberEndpoint(handle);
  CHECK(endpoint.uri.empty());
  CHECK(endpoint.resolving);
  respond(handle, 206, "bytes 0-4095/8192", "\"one\"");
  CurlHandle::rememberEndpoint(handle);
  CHECK_EQ("https://cdn.test/file?token=one", endpoint.uri);
  CHECK(!endpoint.resolving);
  CHECK_EQ("https://origin.test/file", impl.uris.front());
  ++endpoint.generation;
  endpoint.uri.clear();
  endpoint.resolving = true;
  CurlHandle::rememberEndpoint(handle);
  CHECK(endpoint.uri.empty());
  CHECK(endpoint.resolving);
}

void CurlSessionTest::testConnectionRecovery()
{
  Option option;
  option.put(PREF_STATE_DIR, A2_TEST_OUT_DIR "/curl-recovery");
  CurlSession session(&option);
  auto download = std::make_shared<CurlDownload>(
      std::vector<std::string>{"http://example.test/payload"});
  auto& impl = *download->impl_;
  impl.maxConnections = impl.connectionLimit = 8;
  for (int i = 0; i < 7; ++i) {
    auto handle = make_unique<CurlHandle>();
    handle->lease = {0, 1_m};
    handle->rangeAccepted = i == 0;
    handle->writeOffset = i == 0 ? 1 : 0;
    impl.handles.push_back(std::move(handle));
  }
  // Pending responses cannot turn one overload response into a capacity of 1.
  session.penalizeConnectionLimit(download, 0);
  CHECK_EQ(4, impl.connectionLimit);
  session.penalizeConnectionLimit(download, 0);
  CHECK_EQ(4, impl.connectionLimit);
  for (auto& handle : impl.handles) {
    handle->rangeAccepted = true;
  }
  session.rewardConnectionLimit(download);
  CHECK_EQ(4, impl.connectionLimit);
  for (auto& handle : impl.handles) {
    handle->writeOffset = 1;
  }
  download->snapshot_.sessionDownloadLength = 7;
  session.rewardConnectionLimit(download);
  CHECK_EQ(7, impl.connectionLimit);
  impl.recoverConnectionsAt = {};
  session.rewardConnectionLimit(download);
  CHECK_EQ(7, impl.connectionLimit);
  ++download->snapshot_.sessionDownloadLength;
  session.rewardConnectionLimit(download);
  CHECK_EQ(8, impl.connectionLimit);
  // Recovery does not make a stale rejection eligible for another reduction.
  session.penalizeConnectionLimit(download, 0);
  CHECK_EQ(8, impl.connectionLimit);
  impl.handles.clear();
  session.penalizeConnectionLimit(download, 1);
  CHECK_EQ(4, impl.connectionLimit);
}

void CurlSessionTest::testOutputFilename()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_STATE_DIR, A2_TEST_OUT_DIR "/curl-filename-state");
  option->put(PREF_DIR, A2_TEST_OUT_DIR "/curl-filename");
  CurlSession session(option.get());
  const std::pair<std::string, std::string> cases[] = {
      {"%E4%B8%AD%E6%96%87%20file.bin", "中文 file.bin"},
      {"a%2520b.bin", "a%20b.bin"},
      {"a+b.bin?filename=query.bin", "query.bin"},
      {"bad%ZZ.bin", "bad%ZZ.bin"},
      {"bad%FF.bin", "bad%FF.bin"},
      {"dir%2Fchild.bin", "dir%2Fchild.bin"},
      {"zero%00byte.bin", "zero%00byte.bin"},
      {"line%0Abreak.bin", "line%0Abreak.bin"},
      {"%2E%2E", "%2E%2E"},
      {"%2E", "%2E"},
      {"", "index.html"},
  };
  for (const auto& entry : cases) {
    RequestGroup group(GroupId::create(), option);
    group.setDownloadContext(std::make_shared<DownloadContext>(1_m, 0, ""));
    auto download = std::make_shared<CurlDownload>(
        std::vector<std::string>{"https://example.test/" + entry.first});
    session.restorePaused(download, &group);
    const auto expected = option->get(PREF_DIR) + "/" + entry.second;
    CHECK_EQ(expected, group.getFirstFilePath());
    // A fresh snapshot must not decode an already selected output path again.
    download = std::make_shared<CurlDownload>(
        std::vector<std::string>{"https://example.test/other"});
    session.restorePaused(download, &group);
    CHECK_EQ(expected, group.getFirstFilePath());
  }
  option->put(PREF_OUT, "literal%20name.bin");
  RequestGroup group(GroupId::create(), option);
  group.setDownloadContext(std::make_shared<DownloadContext>(1_m, 0, ""));
  auto download = std::make_shared<CurlDownload>(
      std::vector<std::string>{"https://example.test/%E4%B8%AD.bin"});
  session.restorePaused(download, &group);
  CHECK_EQ(option->get(PREF_DIR) + "/literal%20name.bin",
           group.getFirstFilePath());
}

void CurlSessionTest::testTailRecovery()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_STATE_DIR, A2_TEST_OUT_DIR "/curl-tail");
  option->put(PREF_RETRY_WAIT, "10");
  option->put(PREF_MAX_TRIES, "4");
  RequestGroup group(GroupId::create(), option);
  group.setDownloadContext(
      std::make_shared<DownloadContext>(1_m, 1_m, "payload"));
  auto engine = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
  engine->setOption(option.get());
  auto* session = engine->getCurlSession();
  session->engine_ = engine.get();
  session->transport_.bind(engine.get());
  auto download = std::make_shared<CurlDownload>(
      std::vector<std::string>{"http://example.test/payload"});
  auto& impl = *download->impl_;
  impl.group = &group;
  impl.writer = make_unique<ByteArrayDiskWriter>();
  impl.connectionLimit = 64;
  auto handle = make_unique<CurlHandle>();
  handle->value = curl_easy_init();
  REQUIRE(handle->value);
  handle->rangeAccepted = true;
  handle->download = download.get();
  handle->lease = {0, 1_m};
  handle->responseRangeEnd = 1_m;
  handle->writeOffset = 80_k;
  global::wallclock().reset();
  handle->bodySampleStart = global::wallclock();
  handle->lastPayload = global::wallclock();
  handle->payloadSpeed.reset();
  handle->payloadSpeed.update(80_k);
  impl.handles.push_back(std::move(handle));

  for (int i = 0; i < 32; ++i) {
    impl.handles.push_back(make_unique<CurlHandle>());
  }
  group.setMaxDownloadSpeedLimit(1_k);
  CHECK(!session->rebalanceEndgame(download, 1_m));
  group.setMaxDownloadSpeedLimit(0);
  auto* retrying = impl.handles.back().get();
  retrying->lease.attempts = 1;
  CHECK(!session->rebalanceEndgame(download, 1_m));
  retrying->lease.attempts = 0;
  const auto retryAt = std::chrono::steady_clock::now() + 12_s;
  impl.planner.defer({1_m, 2_m}, retryAt);
  CHECK(!session->rebalanceEndgame(download, 1_m));
  REQUIRE(impl.planner.takeReady(retryAt));
  CHECK(session->rebalanceEndgame(download, 1_m));
  REQUIRE_EQ(33, impl.handles.size());
  auto* donor = impl.handles.front().get();
  auto lease = impl.planner.takeReady({});
  REQUIRE(lease);
  CHECK_EQ(donor->lease.end, lease->begin);
  CHECK_EQ(1_m, lease->end);
  CHECK(!impl.planner.takeReady({}));

  std::string body(static_cast<size_t>(1_m - donor->writeOffset), 'x');
  CHECK_EQ(CURL_WRITEFUNC_ERROR,
           CurlHandle::writeData(body.data(), 1, body.size(), donor));
  CHECK_EQ(lease->begin, donor->writeOffset);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (impl.io.busy() && std::chrono::steady_clock::now() < deadline) {
    impl.io.poll(
        [&](int64_t begin, int64_t end) { impl.planner.commit(begin, end); });
    std::this_thread::yield();
  }
  CHECK_EQ(lease->begin, impl.writer->size());
  CHECK_EQ(error_code::UNDEFINED, download->snapshot_.errorCode);
  impl.planner.commit(0, 80_k);
  RangePlanner restored;
  restored.restore(impl.planner.completedRanges());
  restored.configure(1_m, 1_m, {});
  auto missing = restored.takeReady({});
  REQUIRE(missing);
  CHECK_EQ(lease->begin, missing->begin);
  CHECK_EQ(lease->end, missing->end);
  CHECK(!restored.takeReady({}));
  curl_easy_cleanup(donor->value);
  donor->value = nullptr;
  impl.handles.clear();

  auto replacement = make_unique<CurlHandle>();
  replacement->lease = {1_m - 64_k, 1_m};
  replacement->writeOffset = 1_m - 48_k;
  replacement->rangeAccepted = true;
  replacement->bodySampleStart = global::wallclock();
  impl.handles.push_back(std::move(replacement));
  CHECK(!session->rebalanceEndgame(download, 1_m));
  global::wallclock().advance(20_s);
  CHECK(session->rebalanceEndgame(download, 1_m));
  CHECK(impl.handles.empty());
  const auto tail = impl.planner.takeReady({});
  REQUIRE(tail);
  CHECK_EQ(1_m - 48_k, tail->begin);
  CHECK_EQ(1_m, tail->end);
  const auto delay = session->retryRange(download, *lease, 12);
  REQUIRE(delay);
  CHECK(*delay >= 12_s);
  CHECK(*delay <= std::chrono::milliseconds(12100));
  CHECK(!impl.planner.takeReady(std::chrono::steady_clock::now()));
  global::wallclock().reset();
}

void CurlSessionTest::testShutdownWithLiveSocket()
{
  SocketCore listener;
  listener.bind("127.0.0.1", 0, AF_INET);
  listener.beginListen();
  Option option;
  option.put(PREF_STATE_DIR, A2_TEST_OUT_DIR "/curl-shutdown");
  auto engine = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
  engine->setOption(&option);
  auto* session = engine->getCurlSession();
  session->engine_ = engine.get();
  session->transport_.bind(engine.get());
  auto easy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(
      curl_easy_init(), curl_easy_cleanup);
  REQUIRE(easy);
  const auto url =
      "http://127.0.0.1:" + std::to_string(listener.getAddrInfo().port) + "/";
  REQUIRE_EQ(CURLE_OK, curl_easy_setopt(easy.get(), CURLOPT_URL, url.c_str()));
  REQUIRE_EQ(CURLE_OK, curl_easy_setopt(easy.get(), CURLOPT_PROXY, ""));
  REQUIRE_EQ(CURLM_OK,
             curl_multi_add_handle(session->transport_.get(), easy.get()));
  session->transport_.socketAction(CURL_SOCKET_TIMEOUT, 0);
  REQUIRE(session->transport_.socketCount() > 0);
  // Destroying the engine must unregister native callbacks before deleting
  // the commands they reference. AddressSanitizer detects the reversed order.
  engine.reset();
}

void CurlSessionTest::respond(CurlHandle& handle, long code,
                              const std::string& range, const std::string& etag,
                              const std::string& modified,
                              const std::string& date)
{
  handle.responseCode = code;
  handle.responseEtag = etag;
  handle.responseLastModified = modified;
  handle.responseDate = date;
  handle.responseFailure = CurlResponseFailure::None;
  handle.rangeAccepted = false;
  handle.fullResponseAccepted = false;
  handle.unsatisfiedTotalLength = -1;
  CurlHandle::validateResponse(handle, range);
}

void CurlSessionTest::testWriteErrorBoundary()
{
  CurlDownload download({"https://example.invalid/file"});
  download.impl_->writer = make_unique<FailingDiskWriter>();
  CurlHandle handle;
  handle.download = &download;
  char data[] = "data";
  CHECK_EQ(sizeof(data) - 1,
           CurlHandle::writeData(data, 1, sizeof(data) - 1, &handle));
  bool failed = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (download.impl_->io.busy() &&
         std::chrono::steady_clock::now() < deadline) {
    try {
      download.impl_->io.poll([&](int64_t begin, int64_t end) {
        download.impl_->planner.commit(begin, end);
      });
    }
    catch (const Exception& error) {
      CHECK_EQ(error_code::NOT_ENOUGH_DISK_SPACE, error.getErrorCode());
      failed = true;
    }
    std::this_thread::yield();
  }
  CHECK(failed);
  CHECK_EQ(0, download.impl_->planner.completedLength());
}

void CurlSessionTest::testResponseIdentity()
{
  const std::string modified = "Tue, 25 Aug 2026 00:00:00 GMT";
  const std::string date = "Tue, 25 Aug 2026 00:01:00 GMT";
  for (const auto& tag : {"\"revision-one\"", "\"\"", "W/\"weak\"", "bare",
                          "\"bad space\"", "\"bad\"quote\"", "bad space",
                          "W/bare", "\"unterminated", "bad\r\ntag"}) {
    CurlDownload download({"https://example.test/file"});
    CurlHandle handle;
    handle.download = &download;
    handle.lease = {0, 4096};
    handle.ranged = true;
    respond(handle, 206, "bytes 0-4095/8192", tag, modified, date);
    CHECK(handle.rangeAccepted);
    CHECK_EQ(tag == std::string("\"revision-one\"") ||
                 tag == std::string("\"\"") || tag == std::string("bare"),
             !download.impl_->etag.empty());
    CHECK_EQ(modified, download.impl_->lastModified);
  }

  for (int code : {200, 206, 412, 503}) {
    CurlDownload download({"https://example.test/file"});
    download.impl_->etag = "\"revision-one\"";
    CurlHandle handle;
    handle.download = &download;
    handle.lease = {0, 4096};
    handle.ranged = true;
    respond(handle, code, "bytes 0-4095/8192", "\"revision-two\"");
    const auto expected = code == 503 ? CurlResponseFailure::None
                          : code == 412
                              ? CurlResponseFailure::PreconditionFailed
                              : CurlResponseFailure::EtagChanged;
    CHECK_EQ(expected, handle.responseFailure);
    CHECK(!handle.rangeAccepted);
    CHECK(!handle.fullResponseAccepted);
    CHECK_EQ(std::string("\"revision-one\""), download.impl_->etag);
  }

  for (const auto& initial : {"revision-one", "\"revision-one\""}) {
    for (const auto& next : {"revision-one", "\"revision-one\"", "revision-two",
                             "W/\"revision-one\""}) {
      CurlDownload download({"https://example.test/file"});
      CurlHandle handle;
      handle.download = &download;
      handle.lease = {0, 4096};
      handle.ranged = true;
      respond(handle, 206, "bytes 0-4095/8192", initial, modified, date);
      CHECK_EQ(std::string("\"revision-one\""), download.impl_->etag);
      handle.lease = {4096, 8192};
      respond(handle, 206, "bytes 4096-8191/8192", next,
              "Fri, 24 Apr 2026 06:48:00 GMT", date);
      const auto expected = next == std::string("revision-two")
                                ? CurlResponseFailure::EtagChanged
                            : next == std::string("W/\"revision-one\"")
                                ? CurlResponseFailure::ValidatorUnavailable
                                : CurlResponseFailure::None;
      CHECK_EQ(expected, handle.responseFailure);
      CHECK_EQ(expected == CurlResponseFailure::None, handle.rangeAccepted);
    }
  }

  for (const auto& responseDate :
       {"", "invalid", "Tue, 25 Aug 2026 00:00:00 GMT",
        "Tue, 25 Aug 2026 00:00:59 GMT", "Tue, 25 Aug 2026 00:01:00 GMT"}) {
    CurlDownload download({"https://example.test/file"});
    CurlHandle handle;
    handle.download = &download;
    handle.lease = {0, 4096};
    handle.ranged = true;
    respond(handle, 206, "bytes 0-4095/8192", "W/\"weak\"", modified,
            responseDate);
    const bool qualified = responseDate == date;
    CHECK_EQ(qualified, !download.impl_->lastModified.empty());
    handle.lease = {4096, 8192};
    respond(handle, 206, "bytes 4096-8191/8192", "W/\"weak\"",
            "Tue, 25 Aug 2026 00:00:01 GMT", responseDate);
    CHECK_EQ(qualified ? CurlResponseFailure::ModifiedChanged
                       : CurlResponseFailure::None,
             handle.responseFailure);
  }
}

void CurlSessionTest::testRangeOwnershipAndResponseBoundaries()
{
  struct Case {
    int64_t begin, end, responseEnd, total;
  };
  for (const auto& item :
       {Case{0, 4 * 1024 * 1024, 60651, 60651},
        Case{0, 4 * 1024 * 1024, 60651, 8 * 1024 * 1024},
        Case{1024, 2048, 1536, 4096}, Case{1024, 2048, 1536, 1536}}) {
    CurlDownload download({"https://example.test/file"});
    CurlHandle handle;
    handle.download = &download;
    handle.lease = {item.begin, item.end};
    handle.writeOffset = item.begin;
    handle.ranged = true;
    respond(handle, 206,
            "bytes " + std::to_string(item.begin) + "-" +
                std::to_string(item.responseEnd - 1) + "/" +
                std::to_string(item.total));
    CHECK(handle.rangeAccepted);
    CHECK_EQ(CurlResponseFailure::None, handle.responseFailure);
    CHECK_EQ(item.responseEnd, handle.responseRangeEnd);
    CHECK_EQ(std::min(item.end, item.total), handle.lease.end);
    CHECK_EQ(item.total, download.snapshot().totalLength);
    const auto remainder = handle.lease.remainder(item.responseEnd);
    CHECK_EQ(item.responseEnd, remainder.begin);
    CHECK_EQ(std::min(item.end, item.total), remainder.end);
    // A syntactically valid response for the wrong offset must never be
    // accepted.
    respond(handle, 206, "bytes 1-1023/8192");
    CHECK_EQ(CurlResponseFailure::InvalidRange, handle.responseFailure);
    CHECK(!handle.rangeAccepted);
  }
}

void CurlSessionTest::testNonzeroRangeRejectsCompleteResponse()
{
  CurlDownload download({"https://example.test/file"});
  CurlHandle handle;
  handle.download = &download;
  handle.lease = {1024, 2048};
  handle.writeOffset = 1024;
  handle.ranged = true;
  handle.responseContentLength = 4096;
  respond(handle, 200);
  char data[] = "data";
  CHECK(!handle.fullResponseAccepted);
  CHECK_EQ(CURL_WRITEFUNC_ERROR,
           CurlHandle::writeData(data, 1, sizeof(data) - 1, &handle));
  download.impl_->etag = "\"same\"";
  handle.rangeValidator = "\"same\"";
  respond(handle, 200);
  CHECK_EQ(CurlResponseFailure::ValidatorUnavailable, handle.responseFailure);
  respond(handle, 200, {}, "\"same\"");
  CHECK_EQ(CurlResponseFailure::None, handle.responseFailure);
  CHECK(!handle.fullResponseAccepted);
}

void CurlSessionTest::testUnsatisfiedRangeResponseForms()
{
  CurlDownload download({"https://example.test/file"});
  CurlHandle handle;
  handle.download = &download;
  respond(handle, 416, "bytes */4096");
  CHECK_EQ(4096, handle.unsatisfiedTotalLength);
  respond(handle, 416, "*/8192");
  CHECK_EQ(8192, handle.unsatisfiedTotalLength);
  respond(handle, 416);
  CHECK_EQ(-1, handle.unsatisfiedTotalLength);
}

void CurlSessionTest::testRetryableFailureClassification()
{
  CHECK(CurlSession::retryableFailure(CURLE_SSL_CONNECT_ERROR, 0, 0, 0, false,
                                      false));
  CHECK(!CurlSession::retryableFailure(CURLE_PEER_FAILED_VERIFICATION, 0, 0, 0,
                                       false, true));
  CHECK(!CurlSession::retryableFailure(CURLE_SSL_CERTPROBLEM, 0, 0, 0, false,
                                       true));
  CHECK(!CurlSession::retryableFailure(CURLE_SSL_CACERT_BADFILE, 0, 0, 0, false,
                                       true));
  CHECK(CurlSession::retryableFailure(CURLE_HTTP_RETURNED_ERROR, 403, 0, 0,
                                      true, false));
  CHECK(!CurlSession::retryableFailure(CURLE_HTTP_RETURNED_ERROR, 403, 0, 0,
                                       false, false));
  CHECK(!CurlSession::retryableFailure(CURLE_SSH, 0, 0, 0, false, false));
  CHECK(CurlSession::retryableFailure(CURLE_SSH, 0, 0, 0, false, true));
}

void CurlSessionTest::testFailureMessageUsesTheFailureLayer()
{
  CurlHandle handle;
  const std::string detail = "native TLS failure";
  std::copy(detail.begin(), detail.end(), handle.errorBuffer.begin());

  CHECK_EQ(detail,
           CurlHandle::failureMessage(handle, CURLE_SSL_CONNECT_ERROR, 302));
  CHECK_EQ(std::string("HTTP 503: ") + detail,
           CurlHandle::failureMessage(handle, CURLE_HTTP_RETURNED_ERROR, 503));
}

} // namespace aria2

TEST_CASE(
    "Proxy bypass expands host globs without changing native network rules")
{
  CHECK_EQ("api.example.com,10.0.0.0/8",
           aria2::http::noProxyFor("*.example.com,10.0.0.0/8",
                                   "https://api.example.com/file"));
  CHECK_EQ("10.0.0.0/8",
           aria2::http::noProxyFor("*.example.com,10.0.0.0/8",
                                   "https://example.com.attacker.test/file"));
  CHECK_EQ("dlinks.net",
           aria2::http::noProxyFor("dlinks.*", "https://dlinks.net/file"));
}

TEST_CASE("Stream I/O preserves order without blocking the engine thread")
{
  aria2::stream::IoQueue queue;
  std::promise<void> release;
  auto ready = release.get_future().share();
  queue.submit([ready] { ready.wait(); }, 0, 4);
  queue.submit([] {}, 4, 4);
  std::vector<std::pair<int64_t, int64_t>> committed;
  const auto commit = [&](int64_t first, int64_t last) {
    committed.emplace_back(first, last);
  };
  queue.poll(commit);
  const auto pendingBytes = queue.bytes();
  const auto premature = committed.size();
  release.set_value();
  CHECK_EQ(8, pendingBytes);
  CHECK_EQ(0, premature);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (queue.busy() && std::chrono::steady_clock::now() < deadline) {
    queue.poll(commit);
    std::this_thread::yield();
  }
  REQUIRE_FALSE(queue.busy());
  CHECK_EQ(0, queue.bytes());
  CHECK_EQ((std::vector<std::pair<int64_t, int64_t>>{{0, 4}, {4, 8}}),
           committed);
}

namespace aria2 {
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testEncodedRangeBoundary")
{
  testEncodedRangeBoundary();
}
void CurlSessionTest::testEncodedRangeBoundary()
{
  CurlDownload download({"https://example.test/archive.exe"});
  download.impl_->writer = std::make_shared<ByteArrayDiskWriter>();
  CurlHandle handle;
  handle.download = &download;
  handle.lease = {0, 4096};
  handle.ranged = true;
  handle.encoded = true;
  respond(handle, 206, "bytes 0-4095/8192");
  CHECK_EQ(CurlResponseFailure::EncodedRange, handle.responseFailure);
  char data[] = "encoded";
  CHECK_EQ(CURL_WRITEFUNC_ERROR,
           CurlHandle::writeData(data, 1, sizeof(data) - 1, &handle));
  CHECK_EQ(0, download.impl_->writer->size());
  CHECK_FALSE(download.impl_->io.busy());
  CHECK_EQ(0, download.impl_->planner.completedLength());
  handle.ranged = false;
  handle.responseContentLength = 7;
  respond(handle, 200);
  CHECK(handle.fullResponseAccepted);
  CHECK_EQ(0, download.snapshot_.totalLength);
  CHECK_EQ(0, CurlHandle::updateProgress(&handle, 7, 7, 0, 0));
  CHECK_EQ(0, download.snapshot_.totalLength);
}
} // namespace aria2

namespace aria2 {
TEST_CASE_FIXTURE(CurlSessionTest, "CurlSessionTest.testDiskFailureDuringPause")
{
  testDiskFailureDuringPause();
}
void CurlSessionTest::testDiskFailureDuringPause()
{
  auto option = std::make_shared<Option>();
  OptionParser::getInstance()->parseDefaultValues(*option);
  option->put(PREF_STATE_DIR, A2_TEST_OUT_DIR "/curl-pause-error");
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto* session = engine.getCurlSession();
  session->engine_ = &engine;
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  auto download = std::make_shared<CurlDownload>(
      std::vector<std::string>{"https://example.test/file"});
  auto& impl = *download->impl_;
  impl.group = group.get();
  impl.pendingStop = true;
  impl.writer = std::make_shared<FailingDiskWriter>();
  download->snapshot_.state = CurlSnapshot::State::Active;
  impl.io.submit(
      [writer = impl.writer] {
        const unsigned char data = 0;
        writer->writeData(&data, 1, 0);
      },
      0, 1);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!download->stopped() && std::chrono::steady_clock::now() < deadline) {
    session->advance(download);
    std::this_thread::yield();
  }
  CHECK(download->failed());
  CHECK_EQ(error_code::NOT_ENOUGH_DISK_SPACE, download->snapshot_.errorCode);
  CHECK_EQ(0, impl.planner.completedLength());
  CHECK_FALSE(impl.io.busy());
  CHECK_FALSE(impl.pendingStop);
}
} // namespace aria2
