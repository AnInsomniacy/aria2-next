#include "DHTPeerAnnounceEntry.h"

#include <cstring>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "FileEntry.h"
#include "Peer.h"

namespace aria2 {

class DHTPeerAnnounceEntryTest {


public:
  void testRemoveStalePeerAddrEntry();
  void testEmpty();
  void testAddPeerAddrEntry();
  void testGetPeers();
};

A2_TEST(DHTPeerAnnounceEntryTest, testRemoveStalePeerAddrEntry)
A2_TEST(DHTPeerAnnounceEntryTest, testEmpty)
A2_TEST(DHTPeerAnnounceEntryTest, testAddPeerAddrEntry)
A2_TEST(DHTPeerAnnounceEntryTest, testGetPeers)

void DHTPeerAnnounceEntryTest::testRemoveStalePeerAddrEntry()
{
  unsigned char infohash[DHT_ID_LENGTH];
  memset(infohash, 0xff, DHT_ID_LENGTH);
  DHTPeerAnnounceEntry entry(infohash);

  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.1", 6881));
  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.2", 6882, Timer::zero()));
  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.3", 6883));
  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.4", 6884, Timer::zero()));

  entry.removeStalePeerAddrEntry(10_s);

  REQUIRE_EQ((size_t)2, entry.countPeerAddrEntry());

  const std::vector<PeerAddrEntry>& peerAddrEntries =
      entry.getPeerAddrEntries();
  REQUIRE_EQ(std::string("192.168.0.1"),
                       peerAddrEntries[0].getIPAddress());
  REQUIRE_EQ(std::string("192.168.0.3"),
                       peerAddrEntries[1].getIPAddress());
}

void DHTPeerAnnounceEntryTest::testEmpty()
{
  unsigned char infohash[DHT_ID_LENGTH];
  memset(infohash, 0xff, DHT_ID_LENGTH);
  {
    DHTPeerAnnounceEntry entry(infohash);
    entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.1", 6881));
    REQUIRE(!entry.empty());
  }
  {
    DHTPeerAnnounceEntry entry(infohash);
    REQUIRE(entry.empty());
  }
}

void DHTPeerAnnounceEntryTest::testAddPeerAddrEntry()
{
  unsigned char infohash[DHT_ID_LENGTH];
  memset(infohash, 0xff, DHT_ID_LENGTH);

  DHTPeerAnnounceEntry entry(infohash);
  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.1", 6881, Timer::zero()));
  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.1", 6882));

  REQUIRE_EQ((size_t)2, entry.countPeerAddrEntry());

  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.1", 6881));

  REQUIRE_EQ((size_t)2, entry.countPeerAddrEntry());
  REQUIRE(!entry.getPeerAddrEntries()[0].getLastUpdated().isZero());
}

void DHTPeerAnnounceEntryTest::testGetPeers()
{
  unsigned char infohash[DHT_ID_LENGTH];
  memset(infohash, 0xff, DHT_ID_LENGTH);

  DHTPeerAnnounceEntry entry(infohash);
  {
    std::vector<std::shared_ptr<Peer>> peers;
    entry.getPeers(peers);
    REQUIRE_EQ((size_t)0, peers.size());
  }

  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.1", 6881, Timer::zero()));
  entry.addPeerAddrEntry(PeerAddrEntry("192.168.0.2", 6882));

  {
    std::vector<std::shared_ptr<Peer>> peers;
    entry.getPeers(peers);
    REQUIRE_EQ((size_t)2, peers.size());
    REQUIRE_EQ(std::string("192.168.0.1"), peers[0]->getIPAddress());
    REQUIRE_EQ((uint16_t)6881, peers[0]->getPort());
    REQUIRE_EQ(std::string("192.168.0.2"), peers[1]->getIPAddress());
    REQUIRE_EQ((uint16_t)6882, peers[1]->getPort());
  }
}

} // namespace aria2
