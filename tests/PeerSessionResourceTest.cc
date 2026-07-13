#include "PeerSessionResource.h"

#include "a2doctest.h"

#include "MockBtMessageDispatcher.h"
#include "Exception.h"
#include "util.h"

namespace aria2 {

class PeerSessionResourceTest {


public:
  void setUp() {}

  void tearDown() {}

  void testPeerAllowedIndexSetContains();
  void testAmAllowedIndexSetContains();
  void testHasAllPieces();
  void testHasPiece();
  void testUpdateUploadLength();
  void testUpdateDownloadLength();
  void testExtendedMessageEnabled();
  void testGetExtensionMessageID();
  void testFastExtensionEnabled();
  void testSnubbing();
  void testAmChoking();
  void testAmInterested();
  void testPeerChoking();
  void testPeerInterested();
  void testChokingRequired();
  void testOptUnchoking();
  void testShouldBeChoking();
  void testCountOutstandingRequest();
};

A2_TEST(PeerSessionResourceTest, testPeerAllowedIndexSetContains)
A2_TEST(PeerSessionResourceTest, testAmAllowedIndexSetContains)
A2_TEST(PeerSessionResourceTest, testHasAllPieces)
A2_TEST(PeerSessionResourceTest, testHasPiece)
A2_TEST(PeerSessionResourceTest, testUpdateUploadLength)
A2_TEST(PeerSessionResourceTest, testUpdateDownloadLength)
A2_TEST(PeerSessionResourceTest, testExtendedMessageEnabled)
A2_TEST(PeerSessionResourceTest, testGetExtensionMessageID)
A2_TEST(PeerSessionResourceTest, testFastExtensionEnabled)
A2_TEST(PeerSessionResourceTest, testSnubbing)
A2_TEST(PeerSessionResourceTest, testAmChoking)
A2_TEST(PeerSessionResourceTest, testAmInterested)
A2_TEST(PeerSessionResourceTest, testPeerChoking)
A2_TEST(PeerSessionResourceTest, testPeerInterested)
A2_TEST(PeerSessionResourceTest, testChokingRequired)
A2_TEST(PeerSessionResourceTest, testOptUnchoking)
A2_TEST(PeerSessionResourceTest, testShouldBeChoking)
A2_TEST(PeerSessionResourceTest, testCountOutstandingRequest)

void PeerSessionResourceTest::testPeerAllowedIndexSetContains()
{
  PeerSessionResource res(1_k, 1_m);

  res.addPeerAllowedIndex(567);
  res.addPeerAllowedIndex(789);

  REQUIRE(res.peerAllowedIndexSetContains(567));
  REQUIRE(res.peerAllowedIndexSetContains(789));
  REQUIRE(!res.peerAllowedIndexSetContains(123));
}

void PeerSessionResourceTest::testAmAllowedIndexSetContains()
{
  PeerSessionResource res(1_k, 1_m);

  res.addAmAllowedIndex(567);
  res.addAmAllowedIndex(789);

  REQUIRE(res.amAllowedIndexSetContains(567));
  REQUIRE(res.amAllowedIndexSetContains(789));
  REQUIRE(!res.amAllowedIndexSetContains(123));
}

void PeerSessionResourceTest::testHasAllPieces()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.hasAllPieces());
  res.markSeeder();
  REQUIRE(res.hasAllPieces());
}

void PeerSessionResourceTest::testHasPiece()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.hasPiece(300));
  res.updateBitfield(300, 1);
  REQUIRE(res.hasPiece(300));
  res.updateBitfield(300, 0);
  REQUIRE(!res.hasPiece(300));
}

void PeerSessionResourceTest::testUpdateUploadLength()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE_EQ((int64_t)0LL, res.uploadLength());
  res.updateUploadLength(100);
  res.updateUploadLength(200);
  REQUIRE_EQ((int64_t)300LL, res.uploadLength());
}

void PeerSessionResourceTest::testUpdateDownloadLength()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE_EQ((int64_t)0LL, res.downloadLength());
  res.updateDownload(100);
  res.updateDownload(200);
  REQUIRE_EQ((int64_t)300LL, res.downloadLength());
}

void PeerSessionResourceTest::testExtendedMessageEnabled()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.extendedMessagingEnabled());
  res.extendedMessagingEnabled(true);
  REQUIRE(res.extendedMessagingEnabled());
  res.extendedMessagingEnabled(false);
  REQUIRE(!res.extendedMessagingEnabled());
}

void PeerSessionResourceTest::testGetExtensionMessageID()
{
  PeerSessionResource res(1_k, 1_m);

  res.addExtension(ExtensionMessageRegistry::UT_PEX, 9);
  REQUIRE_EQ(
      (uint8_t)9, res.getExtensionMessageID(ExtensionMessageRegistry::UT_PEX));
  REQUIRE_EQ((uint8_t)0, res.getExtensionMessageID(
                                       ExtensionMessageRegistry::UT_METADATA));

  REQUIRE_EQ(std::string("ut_pex"),
                       std::string(res.getExtensionName(9)));
  REQUIRE(!res.getExtensionName(10));
}

void PeerSessionResourceTest::testFastExtensionEnabled()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.fastExtensionEnabled());
  res.fastExtensionEnabled(true);
  REQUIRE(res.fastExtensionEnabled());
  res.fastExtensionEnabled(false);
  REQUIRE(!res.fastExtensionEnabled());
}

void PeerSessionResourceTest::testSnubbing()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.snubbing());
  res.snubbing(true);
  REQUIRE(res.snubbing());
  res.snubbing(false);
  REQUIRE(!res.snubbing());
}

void PeerSessionResourceTest::testAmChoking()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(res.amChoking());
  res.amChoking(false);
  REQUIRE(!res.amChoking());
  res.amChoking(true);
  REQUIRE(res.amChoking());
}

void PeerSessionResourceTest::testAmInterested()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.amInterested());
  res.amInterested(true);
  REQUIRE(res.amInterested());
  res.amInterested(false);
  REQUIRE(!res.amInterested());
}

void PeerSessionResourceTest::testPeerChoking()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(res.peerChoking());
  res.peerChoking(false);
  REQUIRE(!res.peerChoking());
  res.peerChoking(true);
  REQUIRE(res.peerChoking());
}

void PeerSessionResourceTest::testPeerInterested()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.peerInterested());
  res.peerInterested(true);
  REQUIRE(res.peerInterested());
  res.peerInterested(false);
  REQUIRE(!res.peerInterested());
}

void PeerSessionResourceTest::testChokingRequired()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(res.chokingRequired());
  res.chokingRequired(false);
  REQUIRE(!res.chokingRequired());
  res.chokingRequired(true);
  REQUIRE(res.chokingRequired());
}

void PeerSessionResourceTest::testOptUnchoking()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(!res.optUnchoking());
  res.optUnchoking(true);
  REQUIRE(res.optUnchoking());
  res.optUnchoking(false);
  REQUIRE(!res.optUnchoking());
}

void PeerSessionResourceTest::testShouldBeChoking()
{
  PeerSessionResource res(1_k, 1_m);

  REQUIRE(res.shouldBeChoking());
  res.chokingRequired(false);
  REQUIRE(!res.shouldBeChoking());
  res.chokingRequired(true);
  res.optUnchoking(true);
  REQUIRE(!res.shouldBeChoking());
}

void PeerSessionResourceTest::testCountOutstandingRequest()
{
  PeerSessionResource res(1_k, 1_m);
  std::shared_ptr<MockBtMessageDispatcher> dispatcher(
      new MockBtMessageDispatcher());
  res.setBtMessageDispatcher(dispatcher.get());

  REQUIRE_EQ((size_t)0, res.countOutstandingUpload());
}

} // namespace aria2
