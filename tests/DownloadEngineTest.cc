#include "DownloadEngine.h"

#include <cppunit/extensions/HelperMacros.h>

#include "AbstractCommand.h"
#include "a2functional.h"
#include "CurlSession.h"
#include "DownloadContext.h"
#include "FileEntry.h"
#include "prefs.h"
#include "Request.h"
#include "SelectEventPoll.h"
#include "AsioRuntime.h"
#include "RequestGroupMan.h"
#include "Option.h"

namespace aria2 {

namespace {
std::shared_ptr<Option> createEngineOption()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, A2_TEST_OUT_DIR);
  option->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT, "0");
  option->put(PREF_MAX_OVERALL_UPLOAD_LIMIT, "0");
  option->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option->put(PREF_MAX_DOWNLOAD_RESULT, "5");
  option->put(PREF_BT_MAX_OPEN_FILES, "100");
  option->put(PREF_ED2K_UPLOAD_SLOTS, "3");
  option->put(PREF_ENABLE_RPC, A2_V_FALSE);
  return option;
}

class CurlLimitTestCommand : public AbstractCommand {
public:
  CurlLimitTestCommand(RequestGroup* group, DownloadEngine* engine)
      : AbstractCommand(1, std::make_shared<Request>(),
                        std::make_shared<FileEntry>(), group, engine, nullptr,
                        false, false)
  {
  }

private:
  bool executeInternal() CXX11_OVERRIDE { return false; }
};
} // namespace

class DownloadEngineTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadEngineTest);
  CPPUNIT_TEST(testRuntimeWakeRunsPostedTask);
  CPPUNIT_TEST(testRuntimeTimerWake);
  CPPUNIT_TEST(testHaltWakesRuntime);
  CPPUNIT_TEST(testRefreshRateLimitsAppliesCurlTaskAndGlobalLimit);
  CPPUNIT_TEST(testCurlRateLimitIsPerTaskAcrossSplits);
  CPPUNIT_TEST_SUITE_END();

public:
  void testRuntimeWakeRunsPostedTask();
  void testRuntimeTimerWake();
  void testHaltWakesRuntime();
  void testRefreshRateLimitsAppliesCurlTaskAndGlobalLimit();
  void testCurlRateLimitIsPerTaskAcrossSplits();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadEngineTest);

void DownloadEngineTest::testRuntimeWakeRunsPostedTask()
{
  DownloadEngine e(make_unique<SelectEventPoll>());
  bool ran = false;

  e.getRuntime().post([&ran] { ran = true; });
  e.wakeRuntime();
  e.getRuntime().runReady();

  CPPUNIT_ASSERT(e.getRuntime().wakeRequested());
  CPPUNIT_ASSERT(e.getRuntime().consumeWakeRequest());
  CPPUNIT_ASSERT(!e.getRuntime().wakeRequested());
  CPPUNIT_ASSERT(ran);
}

void DownloadEngineTest::testRuntimeTimerWake()
{
  DownloadEngine e(make_unique<SelectEventPoll>());

  e.scheduleRuntimeWake(std::chrono::milliseconds(0));
  e.getRuntime().runReady();

  CPPUNIT_ASSERT(e.getRuntime().wakeRequested());
}

void DownloadEngineTest::testHaltWakesRuntime()
{
  DownloadEngine e(make_unique<SelectEventPoll>());
  auto option = std::make_shared<Option>();
  std::vector<std::shared_ptr<RequestGroup>> groups;
  e.setRequestGroupMan(make_unique<RequestGroupMan>(groups, 1, option.get()));

  e.requestHalt();

  CPPUNIT_ASSERT(e.isHaltRequested());
  CPPUNIT_ASSERT(e.getRuntime().wakeRequested());
}

void DownloadEngineTest::testRefreshRateLimitsAppliesCurlTaskAndGlobalLimit()
{
  DownloadEngine e(make_unique<SelectEventPoll>());
  auto option = createEngineOption();
  option->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT, "512000");
  e.setOption(option.get());
  e.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option.get()));

  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  group->setDownloadContext(std::make_shared<DownloadContext>(1_k, 0, "test"));
  group->setMaxDownloadSpeedLimit(100_k);
  CurlLimitTestCommand command(group.get(), &e);
  auto easy = curl_easy_init();

  e.getCurlSession().add(easy, &command);
  e.refreshRateLimits();

  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(100_k),
                       e.getCurlSession().testDownloadLimit(easy));

  e.getCurlSession().remove(easy);
  curl_easy_cleanup(easy);
}

void DownloadEngineTest::testCurlRateLimitIsPerTaskAcrossSplits()
{
  DownloadEngine e(make_unique<SelectEventPoll>());
  auto option = createEngineOption();
  option->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT, "6660000");
  e.setOption(option.get());
  e.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option.get()));

  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  group->setDownloadContext(std::make_shared<DownloadContext>(1_k, 0, "test"));
  CurlLimitTestCommand command1(group.get(), &e);
  CurlLimitTestCommand command2(group.get(), &e);
  auto easy1 = curl_easy_init();
  auto easy2 = curl_easy_init();

  e.getCurlSession().add(easy1, &command1);
  e.getCurlSession().add(easy2, &command2);
  e.refreshRateLimits();

  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(3330000),
                       e.getCurlSession().testDownloadLimit(easy1));
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(3330000),
                       e.getCurlSession().testDownloadLimit(easy2));

  e.getCurlSession().remove(easy1);
  e.getCurlSession().remove(easy2);
  curl_easy_cleanup(easy1);
  curl_easy_cleanup(easy2);
}

} // namespace aria2
