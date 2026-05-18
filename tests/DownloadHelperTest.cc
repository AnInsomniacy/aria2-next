#include "download_helper.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

#include <zlib.h>

#include <cppunit/extensions/HelperMacros.h>

#include "RequestGroup.h"
#include "DownloadEngine.h"
#include "DownloadContext.h"
#include "DefaultPieceStorage.h"
#include "DiskAdaptor.h"
#include "DlRetryEx.h"
#include "Ed2kAttribute.h"
#include "Ed2kCommand.h"
#include "Ed2kKadCommand.h"
#include "Ed2kPeerTransfer.h"
#include "Option.h"
#include "Piece.h"
#include "RequestGroupMan.h"
#include "SelectEventPoll.h"
#include "Segment.h"
#include "SegmentMan.h"
#include "SocketCore.h"
#include "DefaultBtProgressInfoFile.h"
#include "array_fun.h"
#include "base32.h"
#include "ed2k_constants.h"
#include "ed2k_aich.h"
#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"
#include "ed2k_policy.h"
#include "ed2k_search.h"
#include "ed2k_server.h"
#include "prefs.h"
#include "Exception.h"
#include "TestUtil.h"
#include "util.h"
#include "FileEntry.h"
#ifdef ENABLE_BITTORRENT
#  include "bittorrent_helper.h"
#endif // ENABLE_BITTORRENT

namespace aria2 {

class DownloadHelperTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadHelperTest);
  CPPUNIT_TEST(testCreateRequestGroupForUri);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2K);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KNodesDat);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KServerMetMetadata);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KKadRoutingState);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KServerState);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KMultipleServerStates);
  CPPUNIT_TEST(testEd2kPeerDeduplication);
  CPPUNIT_TEST(testEd2kSourceExchangeMergePolicy);
  CPPUNIT_TEST(testEd2kSourcePolicyRanksSources);
  CPPUNIT_TEST(testEd2kPiecePolicyUsesPeerAvailability);
  CPPUNIT_TEST(testEd2kPiecePolicyReclaimsIdlePeerSegment);
  CPPUNIT_TEST(testEd2kPeerTransferAppliesAichRecoveryData);
  CPPUNIT_TEST(testEd2kSchedulingKeepsInlineSourceLabel);
  CPPUNIT_TEST(testEd2kPeerSchedulingSkipsBackoff);
  CPPUNIT_TEST(testEd2kPeerCommandRecordsFailure);
  CPPUNIT_TEST(testEd2kPeerCommandBacksOffOnDisconnect);
  CPPUNIT_TEST(testEd2kPeerCommandBacksOffCorruptPiece);
  CPPUNIT_TEST(testEd2kPeerCommandRejectsUnexpectedPartBeforeWrite);
  CPPUNIT_TEST(testEd2kPeerCommandTracksRequestedParts);
  CPPUNIT_TEST(testEd2kPeerCommandWritesCompressedPart);
  CPPUNIT_TEST(testEd2kPeerCommandAppliesAichRecoveryData);
  CPPUNIT_TEST(testEd2kIncomingPeerListenerAnswersHello);
  CPPUNIT_TEST(testEd2kPeerSchedulingSkipsConnectingPeer);
  CPPUNIT_TEST(testEd2kServerStateUpdate);
  CPPUNIT_TEST(testEd2kInitialServerCommandRecordsFailure);
  CPPUNIT_TEST(testEd2kInitialServerCommandUpdatesServerState);
  CPPUNIT_TEST(testEd2kServerSourceRefreshSchedulesHandshakeServer);
  CPPUNIT_TEST(testEd2kServerListSchedulesLearnedServer);
  CPPUNIT_TEST(testEd2kServerCommandRequestsLowIdCallback);
  CPPUNIT_TEST(testEd2kPeerCommandRecordsQueueRank);
  CPPUNIT_TEST(testEd2kPeerCommandAnswersSourceExchange2);
  CPPUNIT_TEST(testEd2kKadCommandUpdatesServerUdpStatus);
  CPPUNIT_TEST(testEd2kKadCommandHandlesClientUdpReask);
  CPPUNIT_TEST(testEd2kSearchResultDeduplication);
  CPPUNIT_TEST(testCreateRequestGroupForUri_parameterized);
  CPPUNIT_TEST(testCreateRequestGroupForUriList);

#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testCreateRequestGroupForUri_BitTorrent);
  CPPUNIT_TEST(testCreateRequestGroupForBitTorrent);
#endif // ENABLE_BITTORRENT

#ifdef ENABLE_METALINK
  CPPUNIT_TEST(testCreateRequestGroupForUri_Metalink);
  CPPUNIT_TEST(testCreateRequestGroupForMetalink);
#endif // ENABLE_METALINK

  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<Option> option_;

public:
  void setUp() { option_.reset(new Option()); }

  void tearDown() {}

  void testCreateRequestGroupForUri();
  void testCreateRequestGroupForUri_ED2K();
  void testCreateRequestGroupForUri_ED2KNodesDat();
  void testCreateRequestGroupForUri_ED2KServerMetMetadata();
  void testCreateRequestGroupForUri_ED2KKadRoutingState();
  void testCreateRequestGroupForUri_ED2KServerState();
  void testCreateRequestGroupForUri_ED2KMultipleServerStates();
  void testEd2kPeerDeduplication();
  void testEd2kSourceExchangeMergePolicy();
  void testEd2kSourcePolicyRanksSources();
  void testEd2kPiecePolicyUsesPeerAvailability();
  void testEd2kPiecePolicyReclaimsIdlePeerSegment();
  void testEd2kPeerTransferAppliesAichRecoveryData();
  void testEd2kSchedulingKeepsInlineSourceLabel();
  void testEd2kPeerSchedulingSkipsBackoff();
  void testEd2kPeerCommandRecordsFailure();
  void testEd2kPeerCommandBacksOffOnDisconnect();
  void testEd2kPeerCommandBacksOffCorruptPiece();
  void testEd2kPeerCommandRejectsUnexpectedPartBeforeWrite();
  void testEd2kPeerCommandTracksRequestedParts();
  void testEd2kPeerCommandWritesCompressedPart();
  void testEd2kPeerCommandAppliesAichRecoveryData();
  void testEd2kIncomingPeerListenerAnswersHello();
  void testEd2kPeerSchedulingSkipsConnectingPeer();
  void testEd2kServerStateUpdate();
  void testEd2kInitialServerCommandRecordsFailure();
  void testEd2kInitialServerCommandUpdatesServerState();
  void testEd2kServerSourceRefreshSchedulesHandshakeServer();
  void testEd2kServerListSchedulesLearnedServer();
  void testEd2kServerCommandRequestsLowIdCallback();
  void testEd2kPeerCommandRecordsQueueRank();
  void testEd2kPeerCommandAnswersSourceExchange2();
  void testEd2kKadCommandUpdatesServerUdpStatus();
  void testEd2kKadCommandHandlesClientUdpReask();
  void testEd2kSearchResultDeduplication();
  void testCreateRequestGroupForUri_parameterized();
  void testCreateRequestGroupForUriList();

#ifdef ENABLE_BITTORRENT
  void testCreateRequestGroupForUri_BitTorrent();
  void testCreateRequestGroupForBitTorrent();
#endif // ENABLE_BITTORRENT

#ifdef ENABLE_METALINK
  void testCreateRequestGroupForUri_Metalink();
  void testCreateRequestGroupForMetalink();
#endif // ENABLE_METALINK
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadHelperTest);

namespace {
std::string deflateEd2kTestData(const std::string& input)
{
  z_stream strm;
  std::memset(&strm, 0, sizeof(strm));
  CPPUNIT_ASSERT_EQUAL(Z_OK, deflateInit(&strm, Z_DEFAULT_COMPRESSION));
  std::string compressed(compressBound(input.size()), '\0');
  strm.avail_in = input.size();
  strm.next_in =
      reinterpret_cast<unsigned char*>(const_cast<char*>(input.data()));
  strm.avail_out = compressed.size();
  strm.next_out = reinterpret_cast<unsigned char*>(&compressed[0]);
  CPPUNIT_ASSERT_EQUAL(Z_STREAM_END, deflate(&strm, Z_FINISH));
  compressed.resize(compressed.size() - strm.avail_out);
  CPPUNIT_ASSERT_EQUAL(Z_OK, deflateEnd(&strm));
  return compressed;
}
} // namespace

void DownloadHelperTest::testCreateRequestGroupForUri()
{
  std::vector<std::string> uris{"http://alpha/file", "http://bravo/file",
                                "http://charlie/file"};
  option_->put(PREF_SPLIT, "7");
  option_->put(PREF_MAX_CONNECTION_PER_SERVER, "2");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_OUT, "file.out");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;
    createRequestGroupForUri(result, option_, uris);
    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
    std::shared_ptr<RequestGroup> group = result[0];
    auto xuris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)6, xuris.size());
    for (size_t i = 0; i < 6; ++i) {
      CPPUNIT_ASSERT_EQUAL(uris[i % 3], xuris[i]);
    }
    CPPUNIT_ASSERT_EQUAL(7, group->getNumConcurrentCommand());
    std::shared_ptr<DownloadContext> ctx = group->getDownloadContext();
    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/file.out"), ctx->getBasePath());
  }
  option_->put(PREF_SPLIT, "5");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;
    createRequestGroupForUri(result, option_, uris);
    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
    std::shared_ptr<RequestGroup> group = result[0];
    auto xuris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)5, xuris.size());
    for (size_t i = 0; i < 5; ++i) {
      CPPUNIT_ASSERT_EQUAL(uris[i % 3], xuris[i]);
    }
  }
  option_->put(PREF_SPLIT, "2");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;
    createRequestGroupForUri(result, option_, uris);
    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
    std::shared_ptr<RequestGroup> group = result[0];
    auto xuris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)3, xuris.size());
    for (size_t i = 0; i < 3; ++i) {
      CPPUNIT_ASSERT_EQUAL(uris[i % 3], xuris[i]);
    }
  }
  option_->put(PREF_FORCE_SEQUENTIAL, A2_V_TRUE);
  {
    std::vector<std::shared_ptr<RequestGroup>> result;
    createRequestGroupForUri(result, option_, uris);
    CPPUNIT_ASSERT_EQUAL((size_t)3, result.size());
    // for alpha server
    std::shared_ptr<RequestGroup> alphaGroup = result[0];
    auto alphaURIs =
        alphaGroup->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)2, alphaURIs.size());
    for (size_t i = 0; i < 2; ++i) {
      CPPUNIT_ASSERT_EQUAL(uris[0], alphaURIs[i]);
    }
    CPPUNIT_ASSERT_EQUAL(2, alphaGroup->getNumConcurrentCommand());
    std::shared_ptr<DownloadContext> alphaCtx =
        alphaGroup->getDownloadContext();
    // See filename is not assigned yet
    CPPUNIT_ASSERT_EQUAL(std::string(""), alphaCtx->getBasePath());
  }
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2K()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%2Fnext.bin|9728001|"
      "0123456789abcdef0123456789abcdef|"
      "p=11111111111111111111111111111111:"
      "22222222222222222222222222222222|/"};
  option_->put(PREF_MAX_CONNECTION_PER_SERVER, "2");
  option_->put(PREF_SPLIT, "4");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER, "203.0.113.10:4661,203.0.113.11:4661");

  std::vector<std::shared_ptr<RequestGroup>> result;

  createRequestGroupForUri(result, option_, uris);

  CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
  auto group = result[0];
  auto ctx = group->getDownloadContext();
  CPPUNIT_ASSERT(ctx->hasAttribute(CTX_ATTR_ED2K));
  CPPUNIT_ASSERT_EQUAL(ed2k::PIECE_LENGTH, ctx->getPieceLength());
  CPPUNIT_ASSERT_EQUAL((int64_t)9728001, ctx->getTotalLength());
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/aria2%2Fnext.bin"),
                       ctx->getBasePath());
  CPPUNIT_ASSERT(ctx->getFirstFileEntry()->isRequested());
  CPPUNIT_ASSERT_EQUAL((size_t)0,
                       ctx->getFirstFileEntry()->getRemainingUris().size());
  CPPUNIT_ASSERT(!ctx->isChecksumVerificationAvailable());
  CPPUNIT_ASSERT(!ctx->isPieceHashVerificationAvailable());
  CPPUNIT_ASSERT_EQUAL(4, group->getNumConcurrentCommand());

  auto attrs = getEd2kAttrs(ctx);
  CPPUNIT_ASSERT_EQUAL(std::string("aria2/next.bin"), attrs->link.name);
  CPPUNIT_ASSERT_EQUAL(
      std::string("0123456789abcdef0123456789abcdef"),
      util::toHex(attrs->link.hash));
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs->link.pieceHashes.size());
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs->servers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), attrs->servers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, attrs->servers[0].port);
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KNodesDat()
{
  std::string nodeIdHex("23a8ceff57a7a32d562d649ed7893796");
  auto nodeId = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  ed2k::KadContact contact;
  contact.id = nodeId;
  contact.host = "127.0.0.1";
  contact.udpPort = 4672;
  contact.tcpPort = 4662;
  contact.version = 8;

  std::string nodesDat;
  nodesDat += ed2k::packUInt32(0);
  nodesDat += ed2k::packUInt32(3);
  nodesDat += ed2k::packUInt32(1);
  nodesDat += ed2k::packUInt32(1);
  nodesDat += ed2k::createKadResponsePayload(
                  nodeId, std::vector<ed2k::KadContact>{contact})
                  .substr(ed2k::HASH_LENGTH + 1);
  const std::string path = A2_TEST_OUT_DIR "/ed2k-nodes.dat";
  createFile(path, nodesDat.size());
  {
    std::ofstream out(path.c_str(), std::ios::binary);
    out.write(nodesDat.data(), nodesDat.size());
  }

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_NODE_LIST, path);

  std::vector<std::shared_ptr<RequestGroup>> result;

  createRequestGroupForUri(result, option_, uris);

  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT(attrs->kadRoutingTable);
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       attrs->kadRoutingTable->getRouterNodes().size());
  CPPUNIT_ASSERT_EQUAL(std::string("127.0.0.1"),
                       attrs->kadRoutingTable->getRouterNodes()[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672,
                       attrs->kadRoutingTable->getRouterNodes()[0].port);
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KServerMetMetadata()
{
  std::string serverMet;
  serverMet.push_back('\x0e');
  serverMet += ed2k::packUInt32(1);
  serverMet += ed2k::packUInt32(0x04030201);
  serverMet += ed2k::packUInt16(4661);
  serverMet += ed2k::packUInt32(2);
  serverMet += ed2k::createStringTag(0x01, "Peer Server");
  serverMet += ed2k::createStringTag(0x0b, "Primary ED2K server");
  const std::string path = A2_TEST_OUT_DIR "/ed2k-server.met";
  createFile(path, serverMet.size());
  {
    std::ofstream out(path.c_str(), std::ios::binary);
    out.write(serverMet.data(), serverMet.size());
  }

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER_LIST, path);

  std::vector<std::shared_ptr<RequestGroup>> result;

  createRequestGroupForUri(result, option_, uris);

  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->servers.size());
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->serverStates.size());
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), attrs->servers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, attrs->servers[0].port);
  CPPUNIT_ASSERT_EQUAL(std::string("Peer Server"),
                       attrs->serverStates[0].name);
  CPPUNIT_ASSERT_EQUAL(std::string("Primary ED2K server"),
                       attrs->serverStates[0].description);
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KKadRoutingState()
{
  std::string selfHex("0123456789abcdef0123456789abcdef");
  auto self = util::fromHex(selfHex.begin(), selfHex.end());
  ed2k::KadRoutingTable table(self);
  ed2k::KadContact contact;
  std::string nodeIdHex("23a8ceff57a7a32d562d649ed7893796");
  contact.id = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  contact.host = "203.0.113.8";
  contact.udpPort = 4672;
  contact.tcpPort = 4662;
  contact.version = 8;
  table.nodeSeen(contact, 100);
  ed2k::Endpoint router;
  router.host = "203.0.113.9";
  router.port = 4672;
  table.addRouterNode(router);

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, A2_TEST_OUT_DIR "/ed2k-command-state");
  File(A2_TEST_OUT_DIR "/ed2k-command-state").mkdirs();
  option_->put(PREF_ED2K_KAD_ROUTING_STATE,
               util::toHex(ed2k::createKadRoutingStatePayload(
                   table.snapshot())));

  std::vector<std::shared_ptr<RequestGroup>> result;

  createRequestGroupForUri(result, option_, uris);

  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT(attrs->kadRoutingTable);
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->kadRoutingTable->liveSize());
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       attrs->kadRoutingTable->getRouterNodes().size());
  auto closest = attrs->kadRoutingTable->findClosest(self, 1, false);
  CPPUNIT_ASSERT_EQUAL((size_t)1, closest.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.8"), closest[0].host);
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KServerState()
{
  ed2k::ServerState state;
  state.endpoint.host = "203.0.113.10";
  state.endpoint.port = 4661;
  state.name = "Peer Server";
  state.description = "Primary ED2K server";
  state.connected = true;
  state.handshakeCompleted = true;
  state.clientId = 0x04030201;
  state.highId = true;
  state.ipAddress = "1.2.3.4";
  state.tcpFlags = 0x55aa;
  state.users = 1234;
  state.files = 5678;
  state.failCount = 2;
  state.lastFailureTime = 100;
  state.nextRetryTime = 160;
  state.lastMessage = "hello";

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER_STATE,
               util::toHex(ed2k::createServerStatePayload(state)));

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);

  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->serverStates.size());
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->servers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), attrs->servers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, attrs->servers[0].port);
  const auto& restored = attrs->serverStates[0];
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), restored.endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, restored.endpoint.port);
  CPPUNIT_ASSERT_EQUAL(std::string("Peer Server"), restored.name);
  CPPUNIT_ASSERT_EQUAL(std::string("Primary ED2K server"),
                       restored.description);
  CPPUNIT_ASSERT(restored.handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x04030201, restored.clientId);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, restored.tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, restored.users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, restored.files);
  CPPUNIT_ASSERT_EQUAL(std::string("hello"), restored.lastMessage);
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KMultipleServerStates()
{
  ed2k::ServerState first;
  first.endpoint.host = "203.0.113.10";
  first.endpoint.port = 4661;
  first.users = 1234;

  ed2k::ServerState second;
  second.endpoint.host = "203.0.113.11";
  second.endpoint.port = 4662;
  second.users = 5678;

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER_STATE,
               util::toHex(ed2k::createServerStatePayload(first)) + "\n" +
                   util::toHex(ed2k::createServerStatePayload(second)) + "\n");

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);

  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs->serverStates.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"),
                       attrs->serverStates[0].endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, attrs->serverStates[0].users);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.11"),
                       attrs->serverStates[1].endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, attrs->serverStates[1].users);
}

void DownloadHelperTest::testEd2kPeerDeduplication()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint peer;
  peer.host = "203.0.113.10";
  peer.port = 4662;

  CPPUNIT_ASSERT(addEd2kPeer(&attrs, peer));
  CPPUNIT_ASSERT(!addEd2kPeer(&attrs, peer));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peers.size());
  auto state = getEd2kPeerState(&attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), state->endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, state->endpoint.port);

  peer.port = 4663;
  CPPUNIT_ASSERT(addEd2kPeer(&attrs, peer));
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs.peers.size());

  std::vector<bool> partStatus;
  partStatus.push_back(true);
  partStatus.push_back(false);
  CPPUNIT_ASSERT(markEd2kPeerQueued(&attrs, peer, 7, partStatus));
  state = getEd2kPeerState(&attrs, peer);
  CPPUNIT_ASSERT(state->queued);
  CPPUNIT_ASSERT(!state->dead);
  CPPUNIT_ASSERT_EQUAL((uint16_t)7, state->queueRank);
  CPPUNIT_ASSERT_EQUAL((size_t)2, state->partStatus.size());

  CPPUNIT_ASSERT(markEd2kPeerDead(&attrs, peer, 100, 30));
  CPPUNIT_ASSERT(!state->queued);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT_EQUAL((int64_t)100, state->lastFailureTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)130, state->nextRetryTime);
}

void DownloadHelperTest::testEd2kSourceExchangeMergePolicy()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint remote;
  remote.host = "203.0.113.20";
  remote.port = 4662;

  std::string userHash(16, '\x11');
  ed2k::SourceExchangeEntry first;
  first.endpoint.host = "203.0.113.10";
  first.endpoint.port = 4662;
  first.userHash = userHash;
  first.cryptOptions = 0x83;

  ed2k::SourceExchangeEntry duplicate = first;
  ed2k::SourceExchangeEntry self;
  self.endpoint = remote;
  ed2k::SourceExchangeEntry loopback;
  loopback.endpoint.host = "127.0.0.1";
  loopback.endpoint.port = 4662;

  std::vector<ed2k::SourceExchangeEntry> entries{first, duplicate, self,
                                                 loopback};
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       mergeEd2kSourceExchangePeers(&attrs, entries, remote));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peers.size());
  auto state = getEd2kPeerState(&attrs, first.endpoint);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL(userHash, state->endpoint.userHash);
  CPPUNIT_ASSERT_EQUAL((uint16_t)0x83, state->endpoint.cryptOptions);
  CPPUNIT_ASSERT((state->sourceFlags & ed2k::PEER_SOURCE_EXCHANGE) != 0);
}

void DownloadHelperTest::testEd2kSourcePolicyRanksSources()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint sx;
  sx.host = "203.0.113.10";
  sx.port = 4662;
  ed2k::Endpoint kad;
  kad.host = "203.0.113.11";
  kad.port = 4662;
  ed2k::Endpoint server;
  server.host = "203.0.113.12";
  server.port = 4662;
  addEd2kPeer(&attrs, sx, ed2k::PEER_SOURCE_EXCHANGE);
  addEd2kPeer(&attrs, kad, ed2k::PEER_SOURCE_KAD);
  addEd2kPeer(&attrs, server, ed2k::PEER_SOURCE_SERVER);
  auto serverState = getEd2kPeerState(&attrs, server);
  serverState->failCount = 2;

  auto selected = ed2k::selectConnectPeer(attrs.peerStates, 0);

  CPPUNIT_ASSERT(selected);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.11"), selected->endpoint.host);

  auto kadState = getEd2kPeerState(&attrs, kad);
  CPPUNIT_ASSERT(markEd2kPeerDead(&attrs, kad, 10, 30));
  selected = ed2k::selectConnectPeer(attrs.peerStates, 20);

  CPPUNIT_ASSERT(selected);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), selected->endpoint.host);
}

void DownloadHelperTest::testEd2kPiecePolicyUsesPeerAvailability()
{
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(ed2k::PIECE_LENGTH) * 3,
      "aria2-next-ed2k.bin");
  auto attrs = std::make_shared<Ed2kAttribute>();
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option_.get());
  auto segmentMan = std::make_shared<SegmentMan>(dctx, pieceStorage);

  std::vector<bool> peerAvailability;
  peerAvailability.push_back(false);
  peerAvailability.push_back(true);
  peerAvailability.push_back(false);

  auto selected = ed2k::selectRequestSegments(segmentMan.get(), 1,
                                              peerAvailability, 3);

  CPPUNIT_ASSERT_EQUAL((size_t)1, selected.size());
  CPPUNIT_ASSERT_EQUAL((size_t)1, selected[0]->getIndex());
  std::vector<std::shared_ptr<Segment>> inFlight;
  segmentMan->getInFlightSegment(inFlight, 1);
  CPPUNIT_ASSERT_EQUAL((size_t)1, inFlight.size());
  CPPUNIT_ASSERT_EQUAL((size_t)1, inFlight[0]->getIndex());
}

void DownloadHelperTest::testEd2kPiecePolicyReclaimsIdlePeerSegment()
{
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(ed2k::PIECE_LENGTH) * 2,
      "aria2-next-ed2k.bin");
  auto attrs = std::make_shared<Ed2kAttribute>();
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option_.get());
  auto segmentMan = std::make_shared<SegmentMan>(dctx, pieceStorage);
  CPPUNIT_ASSERT(segmentMan->getSegmentWithIndex(1, 0));

  std::vector<bool> peerAvailability;
  peerAvailability.push_back(true);
  peerAvailability.push_back(false);
  auto selected = ed2k::selectRequestSegments(segmentMan.get(), 2,
                                              peerAvailability, 1);

  CPPUNIT_ASSERT_EQUAL((size_t)1, selected.size());
  CPPUNIT_ASSERT_EQUAL((size_t)0, selected[0]->getIndex());
  std::vector<std::shared_ptr<Segment>> oldOwner;
  segmentMan->getInFlightSegment(oldOwner, 1);
  CPPUNIT_ASSERT(oldOwner.empty());
}

void DownloadHelperTest::testEd2kPeerTransferAppliesAichRecoveryData()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-transfer-aich-recovery";
  const std::string outfile = outdir + "/aria2 next aich transfer.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::string block0(ed2k::EMBLOCK_LENGTH, 'a');
  std::string block1(ed2k::EMBLOCK_LENGTH, 'b');
  std::string block2(100, 'c');
  const auto data = block0 + block1 + block2;
  std::string corruptData = data;
  corruptData[block0.size()] = 'x';
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(data.size()),
      outfile);
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.hash = ed2k::md4Digest(data);
  attrs->aichRootHash = ed2k::aichRootHash(data.data(), data.size());
  attrs->pieceHashes.push_back(attrs->link.hash);
  ed2k::AichRecoverySet recoverySet;
  recoverySet.partIndex = 0;
  ed2k::AichRecoveryBlock recoveryBlock;
  recoveryBlock.offset = 0;
  recoveryBlock.length = block0.size();
  recoveryBlock.hash = ed2k::aichHash(block0);
  recoverySet.blocks.push_back(recoveryBlock);
  recoveryBlock.offset = block0.size();
  recoveryBlock.length = block1.size();
  recoveryBlock.hash = ed2k::aichHash(block1);
  recoverySet.blocks.push_back(recoveryBlock);
  recoveryBlock.offset = block0.size() + block1.size();
  recoveryBlock.length = block2.size();
  recoveryBlock.hash = ed2k::aichHash(block2);
  recoverySet.blocks.push_back(recoveryBlock);
  attrs->aichRecoverySets.push_back(recoverySet);
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option_.get());
  pieceStorage->initStorage();
  pieceStorage->getDiskAdaptor()->openFile();
  group->setPieceStorage(pieceStorage);
  auto segmentMan = std::make_shared<SegmentMan>(dctx, pieceStorage);
  auto segment = segmentMan->getSegmentWithIndex(1, 0);
  CPPUNIT_ASSERT(segment);

  ed2k::PeerTransfer transfer(dctx.get(), pieceStorage.get(),
                              segmentMan.get(), 1);
  CPPUNIT_ASSERT_THROW(transfer.writePartData(0, corruptData),
                       DlRetryEx);

  auto piece = pieceStorage->getPiece(0);
  CPPUNIT_ASSERT(piece);
  CPPUNIT_ASSERT(piece->hasBlock(0));
  CPPUNIT_ASSERT(!piece->hasBlock(ed2k::EMBLOCK_LENGTH /
                                  piece->getBlockLength()));
  CPPUNIT_ASSERT(!piece->pieceComplete());
}

void DownloadHelperTest::testEd2kSchedulingKeepsInlineSourceLabel()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-inline-source-label";
  const std::string outfile = outdir + "/aria2 next.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|sources,203.0.113.20:4662|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);

  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->peerStates.size());
  auto state = getEd2kPeerState(attrs, attrs->link.sources[0]);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT((state->sourceFlags & ed2k::PEER_SOURCE_RESUME) == 0);
  CPPUNIT_ASSERT((state->sourceFlags & ed2k::PEER_SOURCE_INLINE) != 0);
}

void DownloadHelperTest::testEd2kPeerSchedulingSkipsBackoff()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER, "203.0.113.10:4661");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_TRUE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());
  ed2k::Endpoint peer;
  peer.host = "203.0.113.20";
  peer.port = 4662;
  addEd2kPeer(attrs, peer);
  auto state = getEd2kPeerState(attrs, peer);
  state->dead = true;
  state->nextRetryTime = std::numeric_limits<int64_t>::max();

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));

  schedulePendingEd2kPeers(group.get(), &engine);

  CPPUNIT_ASSERT_EQUAL((int32_t)0, group->getNumCommand());
}

void DownloadHelperTest::testEd2kPeerCommandRecordsFailure()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-peer-failure";
  const std::string outfile = outdir + "/aria2 next peer failure.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20peer%20failure.bin|9728001|"
      "0123456789abcdef0123456789abcdef|"
      "sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());
  auto peer = attrs->link.sources[0];
  addEd2kPeer(attrs, peer);

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  engine.addCommand(make_unique<Ed2kCommand>(engine.newCUID(), group.get(),
                                             &engine, peer, false));

  for (int i = 0; i < 8 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT(state->lastFailureTime > 0);
  CPPUNIT_ASSERT(state->nextRetryTime >= state->lastFailureTime + 30);
}

void DownloadHelperTest::testEd2kPeerCommandBacksOffOnDisconnect()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-peer-disconnect";
  const std::string outfile = outdir + "/aria2 next peer disconnect.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20peer%20disconnect.bin|9728001|"
      "0123456789abcdef0123456789abcdef|sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->writeData(std::string("\xe3\x10\x00\x00\x00\x40", 6));
  peerSocket->closeConnection();

  engine.run(true);

  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT(!state->connecting);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT(state->lastFailureTime > 0);
  CPPUNIT_ASSERT(state->nextRetryTime >= state->lastFailureTime + 30);
}

void DownloadHelperTest::testEd2kPeerCommandBacksOffCorruptPiece()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-corrupt-piece";
  const std::string outfile = outdir + "/aria2 next corrupt piece.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string goodData = "verified data";
  std::string corruptData = goodData;
  corruptData[0] = 'x';
  const auto fileHash = ed2k::md4Digest(goodData);
  const auto fileHashHex = util::toHex(fileHash);
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20corrupt%20piece.bin|" +
      util::uitos(goodData.size()) + "|" + fileHashHex +
      "|sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->setNonBlockingMode();
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_ACCEPTUPLOADREQ, ""));

  for (int i = 0; i < 8 && !peerSocket->isReadable(0); ++i) {
    engine.run(true);
  }
  CPPUNIT_ASSERT(peerSocket->isReadable(0));

  std::string partPayload = fileHash + ed2k::packUInt32(0) +
                            ed2k::packUInt32(corruptData.size()) +
                            corruptData;
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_SENDINGPART, partPayload));

  for (int i = 0; i < 8 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT(!state->accepted);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT(state->lastFailureTime > 0);
  CPPUNIT_ASSERT(state->nextRetryTime >= state->lastFailureTime + 30);
}

void DownloadHelperTest::testEd2kPeerCommandRejectsUnexpectedPartBeforeWrite()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-unexpected-part";
  const std::string outfile = outdir + "/aria2 next unexpected part.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string data = "verified data";
  const std::string unexpected = "bad";
  const auto fileHash = ed2k::md4Digest(data);
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20unexpected%20part.bin|" +
      util::uitos(data.size()) + "|" + util::toHex(fileHash) +
      "|sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->setNonBlockingMode();
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_ACCEPTUPLOADREQ, ""));

  for (int i = 0; i < 8 && !peerSocket->isReadable(0); ++i) {
    engine.run(true);
  }

  std::string partPayload = fileHash + ed2k::packUInt32(1) +
                            ed2k::packUInt32(1 + unexpected.size()) +
                            unexpected;
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_SENDINGPART, partPayload));

  for (int i = 0; i < 8 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->dead);
  std::ifstream in(outfile.c_str(), std::ios::binary);
  std::string fileData((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  CPPUNIT_ASSERT(fileData.find(unexpected) == std::string::npos);
}

void DownloadHelperTest::testEd2kPeerCommandTracksRequestedParts()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-requested-parts";
  const std::string outfile = outdir + "/aria2 next requested parts.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string data = "verified data";
  const auto fileHash = ed2k::md4Digest(data);
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20requested%20parts.bin|" +
      util::uitos(data.size()) + "|" + util::toHex(fileHash) +
      "|sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->setNonBlockingMode();
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_ACCEPTUPLOADREQ, ""));

  for (int i = 0; i < 8 && !peerSocket->isReadable(0); ++i) {
    engine.run(true);
  }
  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL((size_t)1, state->requestedParts.size());
  CPPUNIT_ASSERT_EQUAL((int64_t)0, state->requestedParts[0].begin);
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(data.size()),
                       state->requestedParts[0].end);

  std::string partPayload = fileHash + ed2k::packUInt32(0) +
                            ed2k::packUInt32(data.size()) + data;
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_SENDINGPART, partPayload));

  for (int i = 0; i < 8 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  CPPUNIT_ASSERT(state->requestedParts.empty());
  CPPUNIT_ASSERT(!state->accepted);
  CPPUNIT_ASSERT(!state->connecting);
}

void DownloadHelperTest::testEd2kPeerCommandWritesCompressedPart()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-compressed-part";
  const std::string outfile = outdir + "/aria2 next compressed part.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string data = "verified compressed data";
  const auto fileHash = ed2k::md4Digest(data);
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20compressed%20part.bin|" +
      util::uitos(data.size()) + "|" + util::toHex(fileHash) +
      "|sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->setNonBlockingMode();
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_ACCEPTUPLOADREQ, ""));

  for (int i = 0; i < 8 && !peerSocket->isReadable(0); ++i) {
    engine.run(true);
  }
  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL((size_t)1, state->requestedParts.size());

  const auto compressed = deflateEd2kTestData(data);
  std::string partPayload =
      fileHash + ed2k::packUInt32(0) +
      ed2k::packUInt32(static_cast<uint32_t>(compressed.size())) + compressed;
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_COMPRESSEDPART, partPayload));

  for (int i = 0; i < 8 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  CPPUNIT_ASSERT(state->requestedParts.empty());
  CPPUNIT_ASSERT(!state->accepted);
  CPPUNIT_ASSERT(!state->connecting);
  std::ifstream in(outfile.c_str(), std::ios::binary);
  std::string fileData((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  CPPUNIT_ASSERT_EQUAL(data, fileData);
}

void DownloadHelperTest::testEd2kPeerCommandAppliesAichRecoveryData()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-aich-recovery";
  const std::string outfile = outdir + "/aria2 next aich.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::string block0(ed2k::EMBLOCK_LENGTH, 'a');
  std::string block1(ed2k::EMBLOCK_LENGTH, 'b');
  std::string block2(100, 'c');
  const auto data = block0 + block1 + block2;
  const auto fileHash = ed2k::md4Digest(data);
  const auto aichRoot = ed2k::aichRootHash(data.data(), data.size());
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20aich.bin|" + util::uitos(data.size()) +
      "|" + util::toHex(fileHash) + "|h=" + base32::encode(aichRoot) +
      "|sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->setNonBlockingMode();
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_ACCEPTUPLOADREQ, ""));

  for (int i = 0; i < 8 && !peerSocket->isReadable(0); ++i) {
    engine.run(true);
  }
  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL((size_t)1, state->requestedParts.size());

  std::string recovery;
  recovery += ed2k::packUInt16(3);
  recovery += ed2k::packUInt16(7);
  recovery += ed2k::aichHash(block0);
  recovery += ed2k::packUInt16(6);
  recovery += ed2k::aichHash(block1);
  recovery += ed2k::packUInt16(2);
  recovery += ed2k::aichHash(block2);
  recovery += ed2k::packUInt16(0);
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EMULE, ed2k::OP_AICHANSWER,
      ed2k::createAichAnswerPayload(fileHash, 0, aichRoot, recovery)));

  for (int i = 0; i < 8 && attrs->aichRecoverySets.empty(); ++i) {
    engine.run(true);
  }

  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->aichRecoverySets.size());
  CPPUNIT_ASSERT_EQUAL((size_t)0, attrs->aichRecoverySets[0].partIndex);
  CPPUNIT_ASSERT_EQUAL((size_t)3, attrs->aichRecoverySets[0].blocks.size());
  CPPUNIT_ASSERT_EQUAL(ed2k::aichHash(block2),
                       attrs->aichRecoverySets[0].blocks[2].hash);

}

void DownloadHelperTest::testEd2kIncomingPeerListenerAnswersHello()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-incoming-peer";
  const std::string outfile = outdir + "/aria2 next incoming peer.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string data = "incoming peer data";
  const auto fileHash = ed2k::md4Digest(data);
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20incoming%20peer.bin|" +
      util::uitos(data.size()) + "|" + util::toHex(fileHash) + "|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_ED2K_LISTEN_PORT, "0");
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  auto rgman = make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get());
  group->setRequestGroupMan(rgman.get());
  group->setState(RequestGroup::STATE_ACTIVE);
  rgman->addRequestGroup(group);
  engine.setRequestGroupMan(std::move(rgman));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  for (int i = 0; i < 8 && !engine.getEd2kTcpPort(); ++i) {
    engine.run(true);
  }
  CPPUNIT_ASSERT(engine.getEd2kTcpPort() != 0);

  SocketCore peerSocket;
  peerSocket.establishConnection("127.0.0.1", engine.getEd2kTcpPort());
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = 4662;
  peerSocket.setNonBlockingMode();
  for (int i = 0; i < 8 && !peerSocket.isWritable(0); ++i) {
    engine.run(true);
  }
  CPPUNIT_ASSERT(peerSocket.isWritable(0));

  std::string hello;
  hello.push_back(static_cast<char>(ed2k::HASH_LENGTH));
  hello += ed2k::createLoginRequestPayload(
      std::string(ed2k::HASH_LENGTH, '\x22'), peer.port, "incoming");
  hello += std::string(6, '\0');
  peerSocket.writeData(
      ed2k::createPacket(ed2k::PROTO_EDONKEY, ed2k::OP_HELLO, hello));

  auto readHelloAnswer = [&](SocketCore& socket, int attempts) {
    ed2k::PacketHeader header;
    std::string packet;
    for (int i = 0; i < attempts; ++i) {
      if (!socket.isReadable(0)) {
        engine.run(true);
      }
      if (!socket.isReadable(0)) {
        continue;
      }
      char buf[128];
      size_t len = sizeof(buf);
      socket.readData(buf, len);
      packet.append(buf, len);
      size_t offset = 0;
      while (packet.size() - offset >= 6) {
        CPPUNIT_ASSERT(ed2k::readPacketHeader(header, packet.data() + offset,
                                              packet.size() - offset));
        const auto packetSize = 5 + header.size;
        if (packet.size() - offset < packetSize) {
          break;
        }
        if (header.opcode == ed2k::OP_HELLOANSWER) {
          CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::PROTO_EDONKEY,
                               header.protocol);
          return true;
        }
        offset += packetSize;
      }
      packet.erase(0, offset);
    }
    return false;
  };

  CPPUNIT_ASSERT(readHelloAnswer(peerSocket, 16));
  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->connecting);
  CPPUNIT_ASSERT(!state->dead);

  SocketCore duplicateSocket;
  duplicateSocket.establishConnection("127.0.0.1", engine.getEd2kTcpPort());
  duplicateSocket.setNonBlockingMode();
  for (int i = 0; i < 8 && !duplicateSocket.isWritable(0); ++i) {
    engine.run(true);
  }
  CPPUNIT_ASSERT(duplicateSocket.isWritable(0));
  duplicateSocket.writeData(
      ed2k::createPacket(ed2k::PROTO_EDONKEY, ed2k::OP_HELLO, hello));
  for (int i = 0; i < 8 && duplicateSocket.isOpen(); ++i) {
    if (!duplicateSocket.isReadable(0)) {
      engine.run(true);
    }
  }
  CPPUNIT_ASSERT(!readHelloAnswer(duplicateSocket, 2));
}

void DownloadHelperTest::testEd2kPeerSchedulingSkipsConnectingPeer()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER, "203.0.113.10:4661");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_TRUE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());
  ed2k::Endpoint peer;
  peer.host = "203.0.113.20";
  peer.port = 4662;
  addEd2kPeer(attrs, peer);
  auto state = getEd2kPeerState(attrs, peer);
  state->connecting = true;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));

  schedulePendingEd2kPeers(group.get(), &engine);

  CPPUNIT_ASSERT_EQUAL((int32_t)0, group->getNumCommand());
}

void DownloadHelperTest::testEd2kServerStateUpdate()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint server;
  server.host = "203.0.113.10";
  server.port = 4661;

  auto state = updateEd2kServerConnected(&attrs, server);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->connected);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), state->endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, state->endpoint.port);

  ed2k::ServerIdChange idChange;
  idChange.clientId = 0x0a000001;
  idChange.highId = true;
  idChange.ipAddress = "1.0.0.10";
  idChange.tcpFlags = 0x55aa;
  updateEd2kServerIdChange(&attrs, server, idChange);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x0a000001, state->clientId);
  CPPUNIT_ASSERT(state->highId);
  CPPUNIT_ASSERT(state->handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL(std::string("1.0.0.10"), state->ipAddress);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, state->tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, state->failCount);

  ed2k::ServerStatus status;
  status.users = 1234;
  status.files = 5678;
  updateEd2kServerStatus(&attrs, server, status);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, state->users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, state->files);

  status.challenge = 0x55aa0011;
  status.users = 2234;
  status.files = 6678;
  status.maxUsers = 9000;
  status.softFiles = 100;
  status.hardFiles = 200;
  status.udpFlags = 0x01020304;
  status.lowIdUsers = 77;
  status.udpObfuscationPort = 4665;
  status.tcpObfuscationPort = 4666;
  status.udpKey = 0x11223344;
  updateEd2kServerUdpStatus(&attrs, server, status, 120);
  CPPUNIT_ASSERT_EQUAL((uint32_t)2234, state->users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)6678, state->files);
  CPPUNIT_ASSERT_EQUAL((uint32_t)9000, state->maxUsers);
  CPPUNIT_ASSERT_EQUAL((uint32_t)100, state->softFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)200, state->hardFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x01020304, state->udpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)77, state->lowIdUsers);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4665, state->udpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, state->tcpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x11223344, state->udpKey);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa0011, state->udpStatusChallenge);
  CPPUNIT_ASSERT_EQUAL((int64_t)120, state->lastUdpStatusTime);

  updateEd2kServerMessage(&attrs, server, "hello");
  CPPUNIT_ASSERT_EQUAL(std::string("hello"), state->lastMessage);

  updateEd2kServerSourceRequestTime(&attrs, server, 90);
  CPPUNIT_ASSERT(!state->connected);
  CPPUNIT_ASSERT(!state->connecting);
  CPPUNIT_ASSERT_EQUAL((int64_t)90, state->nextSourceRequestTime);

  updateEd2kServerFailure(&attrs, server, 100, 30);
  CPPUNIT_ASSERT(!state->connected);
  CPPUNIT_ASSERT(!state->handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT_EQUAL((int64_t)100, state->lastFailureTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)130, state->nextRetryTime);
}

void DownloadHelperTest::testEd2kInitialServerCommandRecordsFailure()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-command-failure";
  const std::string outfile = outdir + "/aria2 next failure.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next%20failure.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_ED2K_SERVER, "127.0.0.1:1");
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_RETRY_WAIT, "30");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  for (int i = 0; i < 8 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  auto state = getEd2kServerState(attrs, attrs->servers[0]);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(!state->connected);
  CPPUNIT_ASSERT(!state->handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT(state->lastFailureTime > 0);
  CPPUNIT_ASSERT(state->nextRetryTime >= state->lastFailureTime + 30);
}

void DownloadHelperTest::testEd2kInitialServerCommandUpdatesServerState()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-command-state";
  const std::string outfile = outdir + "/aria2 next.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_ED2K_SERVER, "127.0.0.1:1");
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->servers[0].host = "127.0.0.1";
  attrs->servers[0].port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto serverSocket = listenSocket.acceptConnection();
  serverSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_IDCHANGE,
      ed2k::packUInt32(0x04030201) + ed2k::packUInt32(0x55aa) +
          ed2k::packUInt32(4661)));
  serverSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_SERVERSTATUS,
      ed2k::packUInt32(1234) + ed2k::packUInt32(5678)));
  serverSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_SERVERMESSAGE,
      ed2k::packUInt16(5) + "hello"));

  for (int i = 0; i < 8 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  auto state = getEd2kServerState(attrs, attrs->servers[0]);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x04030201, state->clientId);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, state->tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, state->users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, state->files);
  CPPUNIT_ASSERT_EQUAL(std::string("hello"), state->lastMessage);
}

void DownloadHelperTest::testEd2kServerSourceRefreshSchedulesHandshakeServer()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER, "203.0.113.10:4661");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_TRUE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());
  auto state = getEd2kServerState(attrs, attrs->servers[0]);
  state->handshakeCompleted = true;
  state->nextSourceRequestTime = 1;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;

  schedulePendingEd2kServers(commands, group.get(), &engine);

  CPPUNIT_ASSERT_EQUAL((size_t)1, commands.size());
}

void DownloadHelperTest::testEd2kServerListSchedulesLearnedServer()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-server-list-schedule";
  const std::string outfile = outdir + "/aria2 next.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_ED2K_SERVER, "127.0.0.1:1");
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore firstListenSocket;
  firstListenSocket.bind(0);
  firstListenSocket.beginListen();
  firstListenSocket.setBlockingMode();
  const auto firstEndpoint = firstListenSocket.getAddrInfo();
  attrs->servers[0].host = "127.0.0.1";
  attrs->servers[0].port = firstEndpoint.port;

  SocketCore learnedListenSocket;
  learnedListenSocket.bind(0);
  learnedListenSocket.beginListen();
  learnedListenSocket.setBlockingMode();
  const auto learnedEndpoint = learnedListenSocket.getAddrInfo();

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto firstServerSocket = firstListenSocket.acceptConnection();
  firstServerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_IDCHANGE,
      ed2k::packUInt32(0x04030201)));
  ed2k::Endpoint learnedServer;
  learnedServer.host = "127.0.0.1";
  learnedServer.port = learnedEndpoint.port;
  firstServerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_SERVERLIST,
      ed2k::createServerListPayload(std::vector<ed2k::Endpoint>{
          learnedServer})));

  for (int i = 0; i < 8 && !learnedListenSocket.isReadable(0); ++i) {
    engine.run(true);
  }

  CPPUNIT_ASSERT(learnedListenSocket.isReadable(0));
  auto learnedServerSocket = learnedListenSocket.acceptConnection();
  learnedServerSocket->setNonBlockingMode();
  std::string packet;
  for (int i = 0; i < 4; ++i) {
    char data[128];
    size_t len = sizeof(data);
    learnedServerSocket->readData(data, len);
    packet.append(data, len);
    if (!packet.empty()) {
      break;
    }
    engine.run(true);
  }

  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(ed2k::readPacketHeader(header, packet.data(), packet.size()));
  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::PROTO_EDONKEY, header.protocol);
  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::OP_LOGINREQUEST, header.opcode);
}

void DownloadHelperTest::testEd2kServerCommandRequestsLowIdCallback()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-command-callback";
  const std::string outfile = outdir + "/aria2 next.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string fileHashHex = "0123456789abcdef0123456789abcdef";
  const auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_ED2K_SERVER, "127.0.0.1:1");
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->servers[0].host = "127.0.0.1";
  attrs->servers[0].port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto serverSocket = listenSocket.acceptConnection();
  serverSocket->setNonBlockingMode();
  serverSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_IDCHANGE,
      ed2k::packUInt32(0x04030201) + ed2k::packUInt32(0x55aa) +
          ed2k::packUInt32(4661)));

  for (int i = 0; i < 4 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  ed2k::Endpoint lowIdSource;
  lowIdSource.host = "120.0.0.0";
  lowIdSource.port = 4662;
  serverSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_FOUNDSOURCES,
      ed2k::createFoundSourcesPayload(
          fileHash, std::vector<ed2k::Endpoint>{lowIdSource})));

  for (int i = 0; i < 4 && group->getNumCommand() > 0 &&
                  !serverSocket->isReadable(0);
       ++i) {
    engine.run(true);
  }

  ed2k::PacketHeader header;
  std::string packet;
  for (int i = 0; i < 4; ++i) {
    char data[64];
    size_t len = sizeof(data);
    serverSocket->readData(data, len);
    packet.append(data, len);
    size_t offset = 0;
    while (packet.size() - offset >= 6) {
      CPPUNIT_ASSERT(ed2k::readPacketHeader(header, packet.data() + offset,
                                            packet.size() - offset));
      const auto packetSize = 5 + header.size;
      if (packet.size() - offset < packetSize) {
        break;
      }
      if (header.opcode == ed2k::OP_CALLBACKREQUEST) {
        CPPUNIT_ASSERT_EQUAL((uint32_t)120,
                             ed2k::readUInt32(packet.data() + offset + 6));
        return;
      }
      offset += packetSize;
    }
    packet.erase(0, offset);
    if (!serverSocket->isReadable(0)) {
      engine.run(true);
    }
  }

  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::PROTO_EMULE, header.protocol);
  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::OP_CALLBACKREQUEST, header.opcode);
}

void DownloadHelperTest::testEd2kPeerCommandRecordsQueueRank()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-command-queue-rank";
  const std::string outfile = outdir + "/aria2 next.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|"
      "sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = endpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->setNonBlockingMode();
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_QUEUERANK,
                                           ed2k::packUInt32(42)));

  for (int i = 0; i < 4 && group->getNumCommand() > 0; ++i) {
    engine.run(true);
  }

  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->queued);
  CPPUNIT_ASSERT(!state->dead);
  CPPUNIT_ASSERT_EQUAL((uint16_t)42, state->queueRank);
}

void DownloadHelperTest::testEd2kPeerCommandAnswersSourceExchange2()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-command-sx2";
  const std::string outfile = outdir + "/aria2 next.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string fileHashHex = "0123456789abcdef0123456789abcdef";
  const auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|"
      "sources,127.0.0.1:1|/"};
  option_->put(PREF_DIR, outdir);
  option_->put(PREF_CONNECT_TIMEOUT, "1");
  option_->put(PREF_TIMEOUT, "1");
  option_->put(PREF_SPLIT, "1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_FALSE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  const auto endpoint = listenSocket.getAddrInfo();
  attrs->link.sources[0].host = "127.0.0.1";
  attrs->link.sources[0].port = endpoint.port;
  ed2k::Endpoint extraPeer;
  extraPeer.host = "203.0.113.9";
  extraPeer.port = 4662;
  addEd2kPeer(attrs, extraPeer);

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  std::vector<std::unique_ptr<Command>> commands;
  group->createInitialCommand(commands, &engine);
  engine.addCommand(std::move(commands));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerSocket = listenSocket.acceptConnection();
  peerSocket->setNonBlockingMode();
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EMULE, ed2k::OP_REQUESTSOURCES2,
      ed2k::createRequestSources2Payload(fileHash)));

  ed2k::PacketHeader header;
  std::string packet;
  for (int i = 0; i < 6; ++i) {
    if (!peerSocket->isReadable(0)) {
      engine.run(true);
    }
    char data[128];
    size_t len = sizeof(data);
    peerSocket->readData(data, len);
    packet.append(data, len);
    size_t offset = 0;
    while (packet.size() - offset >= 6) {
      CPPUNIT_ASSERT(ed2k::readPacketHeader(header, packet.data() + offset,
                                            packet.size() - offset));
      const auto packetSize = 5 + header.size;
      if (packet.size() - offset < packetSize) {
        break;
      }
      if (header.opcode == ed2k::OP_ANSWERSOURCES2) {
        ed2k::SourceExchangeAnswer answer;
        CPPUNIT_ASSERT(ed2k::parseAnswerSources2Payload(
            answer, packet.substr(offset + 6, header.payloadSize()),
            fileHash));
        CPPUNIT_ASSERT_EQUAL((size_t)1, answer.entries.size());
        CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.9"),
                             answer.entries[0].endpoint.host);
        CPPUNIT_ASSERT_EQUAL((uint16_t)4662,
                             answer.entries[0].endpoint.port);
        return;
      }
      offset += packetSize;
    }
    packet.erase(0, offset);
  }

  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::OP_ANSWERSOURCES2, header.opcode);
}

void DownloadHelperTest::testEd2kKadCommandUpdatesServerUdpStatus()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER, "127.0.0.1:1");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_TRUE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore udpServer(SOCK_DGRAM);
  udpServer.bind(0);
  udpServer.setNonBlockingMode();
  auto serverEndpoint = udpServer.getAddrInfo();
  attrs->servers[0].host = "127.0.0.1";
  attrs->servers[0].port = serverEndpoint.port - 4;
  auto state = getEd2kServerState(attrs, attrs->servers[0]);
  state->handshakeCompleted = true;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  auto command = make_unique<Ed2kKadCommand>(engine.newCUID(), group.get(),
                                             &engine);
  auto commandPtr = command.get();
  engine.addRoutineCommand(std::move(command));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  CPPUNIT_ASSERT(state->udpStatusChallenge != 0);

  const auto response = ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_GLOBSERVSTATRES,
      ed2k::packUInt32(state->udpStatusChallenge) + ed2k::packUInt32(1234) +
          ed2k::packUInt32(5678) + ed2k::packUInt32(9000) +
          ed2k::packUInt32(100) + ed2k::packUInt32(200) +
          ed2k::packUInt32(0x01020304) + ed2k::packUInt32(77) +
          ed2k::packUInt16(4665) + ed2k::packUInt16(4666) +
          ed2k::packUInt32(0x11223344));
  udpServer.writeData(response.data(), response.size(), "127.0.0.1",
                      commandPtr->getLocalUdpPort());

  CPPUNIT_ASSERT(commandPtr->waitLocalUdpReadable(1));
  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, state->users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, state->files);
  CPPUNIT_ASSERT_EQUAL((uint32_t)9000, state->maxUsers);
  CPPUNIT_ASSERT_EQUAL((uint32_t)100, state->softFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)200, state->hardFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x01020304, state->udpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)77, state->lowIdUsers);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4665, state->udpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, state->tcpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x11223344, state->udpKey);
}

void DownloadHelperTest::testEd2kKadCommandHandlesClientUdpReask()
{
  const std::string fileHashHex = "0123456789abcdef0123456789abcdef";
  const auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_TRUE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  SocketCore peerSocket(SOCK_DGRAM);
  peerSocket.bind(0);
  peerSocket.setNonBlockingMode();
  auto peerSocketEndpoint = peerSocket.getAddrInfo();
  ed2k::Endpoint peerEndpoint;
  peerEndpoint.host = "127.0.0.1";
  peerEndpoint.port = peerSocketEndpoint.port;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  auto command = make_unique<Ed2kKadCommand>(engine.newCUID(), group.get(),
                                             &engine);
  auto commandPtr = command.get();
  engine.addRoutineCommand(std::move(command));

  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));

  auto ack = ed2k::createPacket(ed2k::PROTO_EMULE, ed2k::OP_REASKACK,
                                ed2k::createUdpReaskAckPayload(5));
  peerSocket.writeData(ack.data(), ack.size(), "127.0.0.1",
                       commandPtr->getLocalUdpPort());

  CPPUNIT_ASSERT(commandPtr->waitLocalUdpReadable(1));
  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  auto peerState = getEd2kPeerState(attrs, peerEndpoint);
  CPPUNIT_ASSERT(peerState);
  CPPUNIT_ASSERT(peerState->queued);
  CPPUNIT_ASSERT(!peerState->dead);
  CPPUNIT_ASSERT_EQUAL((uint16_t)5, peerState->queueRank);

  auto queueFull = ed2k::createPacket(ed2k::PROTO_EMULE, ed2k::OP_QUEUEFULL,
                                      std::string());
  peerSocket.writeData(queueFull.data(), queueFull.size(), "127.0.0.1",
                       commandPtr->getLocalUdpPort());

  CPPUNIT_ASSERT(commandPtr->waitLocalUdpReadable(1));
  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  peerState = getEd2kPeerState(attrs, peerEndpoint);
  CPPUNIT_ASSERT(peerState);
  CPPUNIT_ASSERT(!peerState->queued);
  CPPUNIT_ASSERT(peerState->dead);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, peerState->failCount);

  auto reask = ed2k::createPacket(ed2k::PROTO_EMULE, ed2k::OP_REASKFILEPING,
                                  ed2k::createUdpReaskFilePingPayload(
                                      fileHash, 0));
  peerSocket.writeData(reask.data(), reask.size(), "127.0.0.1",
                       commandPtr->getLocalUdpPort());

  CPPUNIT_ASSERT(commandPtr->waitLocalUdpReadable(1));
  CPPUNIT_ASSERT_EQUAL(1, engine.run(true));
  CPPUNIT_ASSERT(peerSocket.isReadable(1));
  char data[64];
  size_t len = sizeof(data);
  peerSocket.readData(data, len);
  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(ed2k::readPacketHeader(header, data, len));
  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::PROTO_EMULE, header.protocol);
  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::OP_REASKACK, header.opcode);
  ed2k::UdpReaskAck parsedAck;
  CPPUNIT_ASSERT(ed2k::parseUdpReaskAckPayload(
      parsedAck, std::string(data + 6, data + len)));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, parsedAck.rank);
}

void DownloadHelperTest::testEd2kSearchResultDeduplication()
{
  Ed2kAttribute attrs;
  ed2k::SearchResultEntry entry;
  const std::string hashHex = "0123456789abcdef0123456789abcdef";
  entry.hash = util::fromHex(hashHex.begin(), hashHex.end());
  entry.name = "video.mkv";
  entry.size = 12345;
  entry.sourceNetwork = "server";

  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       addEd2kSearchResults(&attrs, {entry}, true));
  CPPUNIT_ASSERT_EQUAL((size_t)0,
                       addEd2kSearchResults(&attrs, {entry}, false));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.searchResults.size());
  CPPUNIT_ASSERT(!attrs.searchMoreResults);

  entry.size = 12346;
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       addEd2kSearchResults(&attrs, {entry}, true));
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs.searchResults.size());
  CPPUNIT_ASSERT(attrs.searchMoreResults);
}

void DownloadHelperTest::testCreateRequestGroupForUri_parameterized()
{
  std::vector<std::string> uris{"http://{alpha, bravo}/file",
                                "http://charlie/file"};
  option_->put(PREF_MAX_CONNECTION_PER_SERVER, "1");
  option_->put(PREF_SPLIT, "3");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_OUT, "file.out");
  option_->put(PREF_PARAMETERIZED_URI, A2_V_TRUE);
  {
    std::vector<std::shared_ptr<RequestGroup>> result;

    createRequestGroupForUri(result, option_, uris);

    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
    std::shared_ptr<RequestGroup> group = result[0];
    auto uris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)3, uris.size());

    CPPUNIT_ASSERT_EQUAL(std::string("http://alpha/file"), uris[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("http://bravo/file"), uris[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("http://charlie/file"), uris[2]);

    CPPUNIT_ASSERT_EQUAL(3, group->getNumConcurrentCommand());
    std::shared_ptr<DownloadContext> ctx = group->getDownloadContext();
    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/file.out"), ctx->getBasePath());
  }
}

#ifdef ENABLE_BITTORRENT
void DownloadHelperTest::testCreateRequestGroupForUri_BitTorrent()
{
  std::vector<std::string> uris{"http://alpha/file",
                                A2_TEST_DIR "/test.torrent",
                                "http://bravo/file", "http://charlie/file"};
  option_->put(PREF_MAX_CONNECTION_PER_SERVER, "1");
  option_->put(PREF_SPLIT, "3");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_OUT, "file.out");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;

    createRequestGroupForUri(result, option_, uris);

    CPPUNIT_ASSERT_EQUAL((size_t)2, result.size());
    std::shared_ptr<RequestGroup> group = result[0];
    auto xuris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)3, xuris.size());

    CPPUNIT_ASSERT_EQUAL(uris[0], xuris[0]);
    CPPUNIT_ASSERT_EQUAL(uris[2], xuris[1]);
    CPPUNIT_ASSERT_EQUAL(uris[3], xuris[2]);

    CPPUNIT_ASSERT_EQUAL(3, group->getNumConcurrentCommand());
    std::shared_ptr<DownloadContext> ctx = group->getDownloadContext();
    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/file.out"), ctx->getBasePath());

    std::shared_ptr<RequestGroup> torrentGroup = result[1];
    auto auxURIs =
        torrentGroup->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT(auxURIs.empty());
    CPPUNIT_ASSERT_EQUAL(3, torrentGroup->getNumConcurrentCommand());
    std::shared_ptr<DownloadContext> btctx = torrentGroup->getDownloadContext();
    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/aria2-test"), btctx->getBasePath());
  }
}
#endif // ENABLE_BITTORRENT

#ifdef ENABLE_METALINK
void DownloadHelperTest::testCreateRequestGroupForUri_Metalink()
{
  std::vector<std::string> uris{"http://alpha/file", "http://bravo/file",
                                "http://charlie/file", A2_TEST_DIR "/test.xml"};
  option_->put(PREF_MAX_CONNECTION_PER_SERVER, "1");
  option_->put(PREF_SPLIT, "2");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_OUT, "file.out");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;

    createRequestGroupForUri(result, option_, uris);

// group1: http://alpha/file, ...
// group2-7: 6 file entry in Metalink and 1 torrent file download
#  ifdef ENABLE_BITTORRENT
    CPPUNIT_ASSERT_EQUAL((size_t)7, result.size());
#  else  // !ENABLE_BITTORRENT
    CPPUNIT_ASSERT_EQUAL((size_t)6, result.size());
#  endif // !ENABLE_BITTORRENT

    std::shared_ptr<RequestGroup> group = result[0];
    auto xuris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)3, xuris.size());
    for (size_t i = 0; i < 3; ++i) {
      CPPUNIT_ASSERT_EQUAL(uris[i], xuris[i]);
    }
    CPPUNIT_ASSERT_EQUAL(2, group->getNumConcurrentCommand());
    std::shared_ptr<DownloadContext> ctx = group->getDownloadContext();
    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/file.out"), ctx->getBasePath());

    std::shared_ptr<RequestGroup> aria2052Group = result[1];
    CPPUNIT_ASSERT_EQUAL(1, // because of maxconnections attribute
                         aria2052Group->getNumConcurrentCommand());
    std::shared_ptr<DownloadContext> aria2052Ctx =
        aria2052Group->getDownloadContext();
    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/aria2-0.5.2.tar.bz2"),
                         aria2052Ctx->getBasePath());

    std::shared_ptr<RequestGroup> aria2051Group = result[2];
    CPPUNIT_ASSERT_EQUAL(2, aria2051Group->getNumConcurrentCommand());
  }
}
#endif // ENABLE_METALINK

void DownloadHelperTest::testCreateRequestGroupForUriList()
{
  option_->put(PREF_MAX_CONNECTION_PER_SERVER, "3");
  option_->put(PREF_SPLIT, "3");
  option_->put(PREF_INPUT_FILE, A2_TEST_DIR "/input_uris.txt");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_OUT, "file.out");

  std::vector<std::shared_ptr<RequestGroup>> result;

  createRequestGroupForUriList(result, option_);

  CPPUNIT_ASSERT_EQUAL((size_t)2, result.size());

  std::shared_ptr<RequestGroup> fileGroup = result[0];
  auto fileURIs =
      fileGroup->getDownloadContext()->getFirstFileEntry()->getUris();
  CPPUNIT_ASSERT_EQUAL(std::string("http://alpha/file"), fileURIs[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://bravo/file"), fileURIs[1]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://charlie/file"), fileURIs[2]);
  CPPUNIT_ASSERT_EQUAL(3, fileGroup->getNumConcurrentCommand());
  std::shared_ptr<DownloadContext> fileCtx = fileGroup->getDownloadContext();
  CPPUNIT_ASSERT_EQUAL(std::string("/mydownloads/myfile.out"),
                       fileCtx->getBasePath());

  std::shared_ptr<RequestGroup> fileISOGroup = result[1];
  std::shared_ptr<DownloadContext> fileISOCtx =
      fileISOGroup->getDownloadContext();
  // PREF_OUT in option_ must be ignored.
  CPPUNIT_ASSERT_EQUAL(std::string(), fileISOCtx->getBasePath());
}

#ifdef ENABLE_BITTORRENT
void DownloadHelperTest::testCreateRequestGroupForBitTorrent()
{
  std::vector<std::string> auxURIs{"http://alpha/file", "http://bravo/file",
                                   "http://charlie/file"};

  option_->put(PREF_MAX_CONNECTION_PER_SERVER, "2");
  option_->put(PREF_SPLIT, "5");
  option_->put(PREF_TORRENT_FILE, A2_TEST_DIR "/test.torrent");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_OUT, "file.out");
  option_->put(PREF_BT_EXCLUDE_TRACKER, "http://tracker1");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;

    createRequestGroupForBitTorrent(result, option_, auxURIs,
                                    option_->get(PREF_TORRENT_FILE));

    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());

    std::shared_ptr<RequestGroup> group = result[0];
    auto uris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    std::sort(std::begin(uris), std::end(uris));
    // See -s option is ignored. See processRootDictionary() in
    // bittorrent_helper.cc
    CPPUNIT_ASSERT_EQUAL((size_t)3, uris.size());
    for (size_t i = 0; i < auxURIs.size(); ++i) {
      CPPUNIT_ASSERT_EQUAL(auxURIs[i] + "/aria2-test/aria2/src/aria2c",
                           uris[i]);
    }
    CPPUNIT_ASSERT_EQUAL(5, group->getNumConcurrentCommand());
    auto attrs = bittorrent::getTorrentAttrs(group->getDownloadContext());
    // http://tracker1 was deleted.
    CPPUNIT_ASSERT_EQUAL((size_t)2, attrs->announceList.size());
  }
  {
    // no URIs are given
    std::vector<std::shared_ptr<RequestGroup>> result;
    std::vector<std::string> emptyURIs;
    createRequestGroupForBitTorrent(result, option_, emptyURIs,
                                    option_->get(PREF_TORRENT_FILE));

    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
    std::shared_ptr<RequestGroup> group = result[0];
    auto uris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    CPPUNIT_ASSERT_EQUAL((size_t)0, uris.size());
  }
  option_->put(PREF_FORCE_SEQUENTIAL, A2_V_TRUE);
  {
    std::vector<std::shared_ptr<RequestGroup>> result;

    createRequestGroupForBitTorrent(result, option_, auxURIs,
                                    option_->get(PREF_TORRENT_FILE));

    // See --force-requencial is ignored
    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
  }
}
#endif // ENABLE_BITTORRENT

#ifdef ENABLE_METALINK
void DownloadHelperTest::testCreateRequestGroupForMetalink()
{
  option_->put(PREF_SPLIT, "5");
  option_->put(PREF_METALINK_FILE, A2_TEST_DIR "/test.xml");
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_OUT, "file.out");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;

    createRequestGroupForMetalink(result, option_);

#  ifdef ENABLE_BITTORRENT
    CPPUNIT_ASSERT_EQUAL((size_t)6, result.size());
#  else  // !ENABLE_BITTORRENT
    CPPUNIT_ASSERT_EQUAL((size_t)5, result.size());
#  endif // !ENABLE_BITTORRENT
    std::shared_ptr<RequestGroup> group = result[0];
    auto uris = group->getDownloadContext()->getFirstFileEntry()->getUris();
    std::sort(uris.begin(), uris.end());
    CPPUNIT_ASSERT_EQUAL((size_t)2, uris.size());
    CPPUNIT_ASSERT_EQUAL(std::string("ftp://ftphost/aria2-0.5.2.tar.bz2"),
                         uris[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("http://httphost/aria2-0.5.2.tar.bz2"),
                         uris[1]);
    // See numConcurrentCommand is 1 because of maxconnections attribute.
    CPPUNIT_ASSERT_EQUAL(1, group->getNumConcurrentCommand());
  }
}
#endif // ENABLE_METALINK

} // namespace aria2
