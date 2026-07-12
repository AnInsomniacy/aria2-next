#include "BtPeerBlocklist.h"

#include <sstream>

#include <cppunit/extensions/HelperMacros.h>

#include "Exception.h"
#include "DownloadEngine.h"
#include "Option.h"
#include "Peer.h"
#include "PeerAbstractCommand.h"
#include "SelectEventPoll.h"
#include "BtRegistry.h"
#include "prefs.h"

namespace aria2 {

class BtPeerBlocklistTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(BtPeerBlocklistTest);
  CPPUNIT_TEST(testLoadBtnRules);
  CPPUNIT_TEST(testRejectInvalidReload);
  CPPUNIT_TEST(testStopBlocklistedPeerCommand);
  CPPUNIT_TEST_SUITE_END();

public:
  void testLoadBtnRules();
  void testRejectInvalidReload();
  void testStopBlocklistedPeerCommand();
};

CPPUNIT_TEST_SUITE_REGISTRATION(BtPeerBlocklistTest);

void BtPeerBlocklistTest::testLoadBtnRules()
{
  std::istringstream input(
      "# BTN rules\n"
      "203.0.113.25\n"
      "198.51.100.0/24\n"
      "2001:db8::1234\n"
      "2001:db8:abcd::/48\n");
  BtPeerBlocklist blocklist;

  blocklist.load(input, "memory");

  CPPUNIT_ASSERT_EQUAL((size_t)4, blocklist.count());
  CPPUNIT_ASSERT(blocklist.contains("203.0.113.25"));
  CPPUNIT_ASSERT(!blocklist.contains("203.0.113.26"));
  CPPUNIT_ASSERT(blocklist.contains("198.51.100.255"));
  CPPUNIT_ASSERT(!blocklist.contains("198.51.101.0"));
  CPPUNIT_ASSERT(blocklist.contains("2001:db8::1234"));
  CPPUNIT_ASSERT(blocklist.contains("2001:db8:abcd:ffff::1"));
  CPPUNIT_ASSERT(!blocklist.contains("2001:db8:abce::1"));
}

void BtPeerBlocklistTest::testRejectInvalidReload()
{
  BtPeerBlocklist blocklist;
  std::istringstream valid("203.0.113.0/24\n");
  blocklist.load(valid, "valid");

  std::istringstream invalid("not-an-ip\n");
  CPPUNIT_ASSERT_THROW(blocklist.load(invalid, "invalid"), Exception);

  CPPUNIT_ASSERT_EQUAL((size_t)1, blocklist.count());
  CPPUNIT_ASSERT(blocklist.contains("203.0.113.10"));
}

namespace {

class TestPeerCommand : public PeerAbstractCommand {
public:
  bool blocked = false;
  bool executed = false;
  bool retried = false;

  TestPeerCommand(const std::shared_ptr<Peer>& peer, DownloadEngine* engine)
      : PeerAbstractCommand(1, peer, engine)
  {
  }

private:
  bool prepareForNextPeer(time_t wait) override
  {
    retried = true;
    return true;
  }
  bool exitBeforeExecute() override { return false; }
  bool executeInternal() override
  {
    executed = true;
    return true;
  }
  bool onBlocked() override
  {
    blocked = true;
    return true;
  }
};

} // namespace

void BtPeerBlocklistTest::testStopBlocklistedPeerCommand()
{
  Option option;
  option.put(PREF_BT_TIMEOUT, "180");
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(&option);
  TestPeerCommand command(std::make_shared<Peer>("203.0.113.25", 6881),
                          &engine);
  std::istringstream input("203.0.113.0/24\n");
  engine.getBtRegistry()->getPeerBlocklist()->load(input, "memory");

  CPPUNIT_ASSERT(command.execute());
  CPPUNIT_ASSERT(command.blocked);
  CPPUNIT_ASSERT(!command.executed);
  CPPUNIT_ASSERT(!command.retried);
}

} // namespace aria2
