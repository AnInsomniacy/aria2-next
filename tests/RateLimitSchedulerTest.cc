#include "RateLimitScheduler.h"

#include "TestUtil.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class RateLimitSchedulerTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(RateLimitSchedulerTest);
  CPPUNIT_TEST(testUnlimitedLeavesBackendsUnlimited);
  CPPUNIT_TEST(testSingleActiveBackendGetsGlobalLimit);
  CPPUNIT_TEST(testTaskLimitCapsBackendLimit);
  CPPUNIT_TEST(testUnusedShareIsBorrowedByBusyBackend);
  CPPUNIT_TEST(testSlowBackendKeepsFloorWhileBusyBackendBorrowsRest);
  CPPUNIT_TEST(testUploadAndDownloadAreIndependent);
  CPPUNIT_TEST_SUITE_END();

public:
  void testUnlimitedLeavesBackendsUnlimited();
  void testSingleActiveBackendGetsGlobalLimit();
  void testTaskLimitCapsBackendLimit();
  void testUnusedShareIsBorrowedByBusyBackend();
  void testSlowBackendKeepsFloorWhileBusyBackendBorrowsRest();
  void testUploadAndDownloadAreIndependent();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RateLimitSchedulerTest);

void RateLimitSchedulerTest::testUnlimitedLeavesBackendsUnlimited()
{
  RateLimitScheduler scheduler;
  scheduler.setGlobalLimit(RateLimitDirection::Download, 0);
  scheduler.setActive(RateLimitBackend::Curl, RateLimitDirection::Download,
                      true);

  scheduler.recalculate();

  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(0),
      scheduler.backendLimit(RateLimitBackend::Curl,
                             RateLimitDirection::Download));
}

void RateLimitSchedulerTest::testSingleActiveBackendGetsGlobalLimit()
{
  RateLimitScheduler scheduler;
  scheduler.setGlobalLimit(RateLimitDirection::Download, 500_k);
  scheduler.setActive(RateLimitBackend::Curl, RateLimitDirection::Download,
                      true);

  scheduler.recalculate();

  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(500_k),
      scheduler.backendLimit(RateLimitBackend::Curl,
                             RateLimitDirection::Download));
}

void RateLimitSchedulerTest::testTaskLimitCapsBackendLimit()
{
  RateLimitScheduler scheduler;
  scheduler.setGlobalLimit(RateLimitDirection::Download, 500_k);
  scheduler.setActive(RateLimitBackend::Curl, RateLimitDirection::Download,
                      true);
  scheduler.setBackendCap(RateLimitBackend::Curl, RateLimitDirection::Download,
                          100_k);

  scheduler.recalculate();

  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(100_k),
      scheduler.backendLimit(RateLimitBackend::Curl,
                             RateLimitDirection::Download));
}

void RateLimitSchedulerTest::testUnusedShareIsBorrowedByBusyBackend()
{
  RateLimitScheduler scheduler;
  scheduler.setGlobalLimit(RateLimitDirection::Download, 500_k);
  scheduler.setActive(RateLimitBackend::Curl, RateLimitDirection::Download,
                      true);
  scheduler.setActive(RateLimitBackend::Libtorrent,
                      RateLimitDirection::Download, true);

  scheduler.recalculate();
  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(250_k),
      scheduler.backendLimit(RateLimitBackend::Curl,
                             RateLimitDirection::Download));

  scheduler.setObservedSpeed(RateLimitBackend::Curl,
                             RateLimitDirection::Download, 250_k);
  scheduler.setObservedSpeed(RateLimitBackend::Libtorrent,
                             RateLimitDirection::Download, 0);
  scheduler.recalculate();

  CPPUNIT_ASSERT(
      scheduler.backendLimit(RateLimitBackend::Curl,
                             RateLimitDirection::Download) > 400_k);
  CPPUNIT_ASSERT(
      scheduler.backendLimit(RateLimitBackend::Libtorrent,
                             RateLimitDirection::Download) < 100_k);
}

void RateLimitSchedulerTest::testSlowBackendKeepsFloorWhileBusyBackendBorrowsRest()
{
  RateLimitScheduler scheduler;
  scheduler.setGlobalLimit(RateLimitDirection::Download, 6660000);
  scheduler.setActive(RateLimitBackend::Curl, RateLimitDirection::Download,
                      true);
  scheduler.setObservedSpeed(RateLimitBackend::Curl,
                             RateLimitDirection::Download, 3300000);
  scheduler.setActive(RateLimitBackend::Libtorrent,
                      RateLimitDirection::Download, true);
  scheduler.setObservedSpeed(RateLimitBackend::Libtorrent,
                             RateLimitDirection::Download, 4096);

  scheduler.recalculate();

  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(5994000),
      scheduler.backendLimit(RateLimitBackend::Curl,
                             RateLimitDirection::Download));
  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(666000),
      scheduler.backendLimit(RateLimitBackend::Libtorrent,
                             RateLimitDirection::Download));
}

void RateLimitSchedulerTest::testUploadAndDownloadAreIndependent()
{
  RateLimitScheduler scheduler;
  scheduler.setGlobalLimit(RateLimitDirection::Download, 500_k);
  scheduler.setGlobalLimit(RateLimitDirection::Upload, 100_k);
  scheduler.setActive(RateLimitBackend::Ed2k, RateLimitDirection::Download,
                      true);
  scheduler.setActive(RateLimitBackend::Ed2k, RateLimitDirection::Upload, true);

  scheduler.recalculate();

  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(500_k),
      scheduler.backendLimit(RateLimitBackend::Ed2k,
                             RateLimitDirection::Download));
  CPPUNIT_ASSERT_EQUAL(
      static_cast<int64_t>(100_k),
      scheduler.backendLimit(RateLimitBackend::Ed2k,
                             RateLimitDirection::Upload));
}

} // namespace aria2
