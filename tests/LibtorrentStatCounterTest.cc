#include "LibtorrentStatCounter.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class LibtorrentStatCounterTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(LibtorrentStatCounterTest);
  CPPUNIT_TEST(testInitialZeroPayloadHasNoDelta);
  CPPUNIT_TEST(testPayloadDeltas);
  CPPUNIT_TEST(testCounterRollbackDoesNotCreateDelta);
  CPPUNIT_TEST_SUITE_END();

public:
  void testInitialZeroPayloadHasNoDelta();
  void testPayloadDeltas();
  void testCounterRollbackDoesNotCreateDelta();
};

CPPUNIT_TEST_SUITE_REGISTRATION(LibtorrentStatCounterTest);

void LibtorrentStatCounterTest::testInitialZeroPayloadHasNoDelta()
{
  LibtorrentStatCounter counter;

  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), counter.updateDownload(0));
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), counter.updateUpload(0));
}

void LibtorrentStatCounterTest::testPayloadDeltas()
{
  LibtorrentStatCounter counter;

  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(1024), counter.updateDownload(1024));
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(512), counter.updateUpload(512));
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(2048), counter.updateDownload(3072));
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(256), counter.updateUpload(768));
}

void LibtorrentStatCounterTest::testCounterRollbackDoesNotCreateDelta()
{
  LibtorrentStatCounter counter;
  counter.updateDownload(4096);
  counter.updateUpload(4096);

  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), counter.updateDownload(1024));
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(0), counter.updateUpload(1024));
}

} // namespace aria2
