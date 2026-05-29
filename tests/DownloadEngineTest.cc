#include "DownloadEngine.h"

#include <cppunit/extensions/HelperMacros.h>

#include "SelectEventPoll.h"
#include "AsioRuntime.h"
#include "RequestGroupMan.h"
#include "Option.h"

namespace aria2 {

class DownloadEngineTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadEngineTest);
  CPPUNIT_TEST(testRuntimeWakeRunsPostedTask);
  CPPUNIT_TEST(testRuntimeTimerWake);
  CPPUNIT_TEST(testHaltWakesRuntime);
  CPPUNIT_TEST_SUITE_END();

public:
  void testRuntimeWakeRunsPostedTask();
  void testRuntimeTimerWake();
  void testHaltWakesRuntime();
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

} // namespace aria2
