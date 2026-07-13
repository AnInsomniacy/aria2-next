#include "DefaultPeerStorage.h"

#include <algorithm>
#include <sstream>

#include "a2doctest.h"

#include "util.h"
#include "Exception.h"
#include "Peer.h"
#include "Option.h"
#include "BtRuntime.h"
#include "BtPeerBlocklist.h"
#include "array_fun.h"

namespace aria2 {

class DefaultPeerStorageTest {


private:
  std::shared_ptr<BtRuntime> btRuntime;
  Option* option;

public:
  void setUp()
  {
    option = new Option();
    btRuntime.reset(new BtRuntime());
  }

  void tearDown() { delete option; }

  void testCountAllPeer();
  void testDeleteUnusedPeer();
  void testAddPeer();
  void testAddAndCheckoutPeer();
  void testIsPeerAvailable();
  void testCheckoutPeer();
  void testReturnPeer();
  void testOnErasingPeer();
  void testTemporarilyRejectPeer();
  void testRejectBlocklistedPeer();
};

A2_TEST(DefaultPeerStorageTest, testCountAllPeer)
A2_TEST(DefaultPeerStorageTest, testDeleteUnusedPeer)
A2_TEST(DefaultPeerStorageTest, testAddPeer)
A2_TEST(DefaultPeerStorageTest, testAddAndCheckoutPeer)
A2_TEST(DefaultPeerStorageTest, testIsPeerAvailable)
A2_TEST(DefaultPeerStorageTest, testCheckoutPeer)
A2_TEST(DefaultPeerStorageTest, testReturnPeer)
A2_TEST(DefaultPeerStorageTest, testOnErasingPeer)
A2_TEST(DefaultPeerStorageTest, testTemporarilyRejectPeer)
A2_TEST(DefaultPeerStorageTest, testRejectBlocklistedPeer)

void DefaultPeerStorageTest::testCountAllPeer()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());

  REQUIRE_EQ((size_t)0, ps.countAllPeer());
  for (int i = 0; i < 2; ++i) {
    std::shared_ptr<Peer> peer(new Peer("192.168.0.1", 6889 + i));
    ps.addPeer(peer);
  }
  REQUIRE_EQ((size_t)2, ps.countAllPeer());
  std::shared_ptr<Peer> peer = ps.checkoutPeer(1);
  REQUIRE(peer);
  REQUIRE_EQ((size_t)2, ps.countAllPeer());
  ps.returnPeer(peer);
  REQUIRE_EQ((size_t)1, ps.countAllPeer());
}

void DefaultPeerStorageTest::testDeleteUnusedPeer()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());

  auto peer1 = std::make_shared<Peer>("192.168.0.1", 6889);
  auto peer2 = std::make_shared<Peer>("192.168.0.2", 6889);
  auto peer3 = std::make_shared<Peer>("192.168.0.3", 6889);

  REQUIRE(ps.addPeer(peer1));
  REQUIRE(ps.addPeer(peer2));
  REQUIRE(ps.addPeer(peer3));

  ps.deleteUnusedPeer(2);

  REQUIRE_EQ((size_t)1, ps.getUnusedPeers().size());
  REQUIRE_EQ(std::string("192.168.0.1"),
                       ps.getUnusedPeers()[0]->getIPAddress());

  ps.deleteUnusedPeer(100);
  REQUIRE(ps.getUnusedPeers().empty());
}

void DefaultPeerStorageTest::testAddPeer()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());
  std::shared_ptr<BtRuntime> btRuntime(new BtRuntime());
  ps.setMaxPeerListSize(2);
  ps.setBtRuntime(btRuntime);

  auto peer1 = std::make_shared<Peer>("192.168.0.1", 6889);
  auto peer2 = std::make_shared<Peer>("192.168.0.2", 6889);
  auto peer3 = std::make_shared<Peer>("192.168.0.3", 6889);

  REQUIRE(ps.addPeer(peer1));
  REQUIRE(ps.addPeer(peer2));
  REQUIRE(!ps.addPeer(peer3));

  REQUIRE_EQ((size_t)2, ps.getUnusedPeers().size());
  REQUIRE_EQ(std::string("192.168.0.1"),
                       ps.getUnusedPeers()[0]->getIPAddress());

  REQUIRE(!ps.addPeer(peer2));
  REQUIRE(!ps.addPeer(peer3));

  REQUIRE_EQ((size_t)2, ps.getUnusedPeers().size());
  REQUIRE_EQ(std::string("192.168.0.1"),
                       ps.getUnusedPeers()[0]->getIPAddress());

  REQUIRE_EQ(peer1->getIPAddress(),
                       ps.checkoutPeer(1)->getIPAddress());
  REQUIRE(!ps.addPeer(peer1));
}

void DefaultPeerStorageTest::testAddAndCheckoutPeer()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());
  auto btRuntime = std::make_shared<BtRuntime>();

  ps.setBtRuntime(btRuntime);

  auto peer1 = std::make_shared<Peer>("192.168.0.1", 6889);
  // peer2 and peer3 have the same address and port deliberately.
  auto peer2 = std::make_shared<Peer>("192.168.0.2", 6889);
  auto peer3 = std::make_shared<Peer>("192.168.0.2", 6889);

  REQUIRE(ps.addPeer(peer1));
  REQUIRE(ps.addPeer(peer2));
  REQUIRE_EQ((size_t)2, ps.getUnusedPeers().size());

  auto usedPeer = ps.checkoutPeer(1);

  REQUIRE_EQ((size_t)1, ps.getUnusedPeers().size());
  REQUIRE_EQ(peer1->getIPAddress(), usedPeer->getIPAddress());
  // Since peer1 is already used, this fails.
  REQUIRE(!ps.addAndCheckoutPeer(peer1, 2));
  REQUIRE_EQ((size_t)1, ps.getUnusedPeers().size());

  // Since peer2 is not used yet, this succeeds.
  usedPeer = ps.addAndCheckoutPeer(peer3, 2);

  REQUIRE_EQ((size_t)0, ps.getUnusedPeers().size());
  REQUIRE_EQ(peer2->getIPAddress(), usedPeer->getIPAddress());
  REQUIRE_EQ(peer3.get(), usedPeer.get());
}

void DefaultPeerStorageTest::testIsPeerAvailable()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());
  ps.setBtRuntime(btRuntime);
  std::shared_ptr<Peer> peer1(new Peer("192.168.0.1", 6889));

  REQUIRE(!ps.isPeerAvailable());
  ps.addPeer(peer1);
  REQUIRE(ps.isPeerAvailable());
  REQUIRE(ps.checkoutPeer(1));
  REQUIRE(!ps.isPeerAvailable());
}

void DefaultPeerStorageTest::testCheckoutPeer()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());

  auto peers = {
      std::make_shared<Peer>("192.168.0.1", 1000),
      std::make_shared<Peer>("192.168.0.2", 1000),
      std::make_shared<Peer>("192.168.0.3", 1000),
  };

  for (auto& peer : peers) {
    ps.addPeer(peer);
  }

  int i = 0;
  for (auto& peer : peers) {
    auto p = ps.checkoutPeer(i + 1);
    ++i;
    REQUIRE_EQ(peer->getIPAddress(), p->getIPAddress());
  }
  REQUIRE(!ps.checkoutPeer(peers.size() + 1));
}

void DefaultPeerStorageTest::testReturnPeer()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());

  std::shared_ptr<Peer> peer1(new Peer("192.168.0.1", 0));
  peer1->allocateSessionResource(1_m, 10_m);
  std::shared_ptr<Peer> peer2(new Peer("192.168.0.2", 6889));
  peer2->allocateSessionResource(1_m, 10_m);
  std::shared_ptr<Peer> peer3(new Peer("192.168.0.1", 6889));
  peer2->setDisconnectedGracefully(true);
  ps.addPeer(peer1);
  ps.addPeer(peer2);
  ps.addPeer(peer3);
  for (int i = 1; i <= 3; ++i) {
    REQUIRE(ps.checkoutPeer(i));
  }
  REQUIRE_EQ((size_t)3, ps.getUsedPeers().size());

  ps.returnPeer(peer2); // peer2 removed from the container
  REQUIRE_EQ((size_t)2, ps.getUsedPeers().size());
  REQUIRE(ps.getUsedPeers().count(peer2) == 0);
  REQUIRE_EQ((size_t)1, ps.getDroppedPeers().size());

  ps.returnPeer(peer1); // peer1 is removed from the container
  REQUIRE_EQ((size_t)1, ps.getUsedPeers().size());
  REQUIRE(ps.getUsedPeers().count(peer1) == 0);
  REQUIRE_EQ((size_t)1, ps.getDroppedPeers().size());
}

void DefaultPeerStorageTest::testOnErasingPeer()
{
  // test this
}

void DefaultPeerStorageTest::testTemporarilyRejectPeer()
{
  DefaultPeerStorage ps(std::make_shared<BtPeerBlocklist>());
  ps.rejectPeerTemporarily("192.168.0.1");
  REQUIRE(ps.isTemporarilyRejectedPeer("192.168.0.1"));
  REQUIRE(!ps.isTemporarilyRejectedPeer("192.168.0.2"));
}

void DefaultPeerStorageTest::testRejectBlocklistedPeer()
{
  auto blocklist = std::make_shared<BtPeerBlocklist>();
  std::istringstream input("192.168.0.0/24\n");
  blocklist->load(input, "memory");
  DefaultPeerStorage ps(blocklist);

  REQUIRE(!ps.addPeer(std::make_shared<Peer>("192.168.0.1", 6881)));
  REQUIRE(!ps.addAndCheckoutPeer(
      std::make_shared<Peer>("192.168.0.2", 6881, true), 1));
  REQUIRE(ps.addPeer(std::make_shared<Peer>("192.168.1.1", 6881)));
}

} // namespace aria2
