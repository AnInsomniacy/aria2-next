#include "download_helper.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <vector>

#include <cppunit/extensions/HelperMacros.h>

#include "RequestGroup.h"
#include "DownloadEngine.h"
#include "DownloadContext.h"
#include "Ed2kAttribute.h"
#include "Ed2kKadCommand.h"
#include "Option.h"
#include "RequestGroupMan.h"
#include "SelectEventPoll.h"
#include "SocketCore.h"
#include "DefaultBtProgressInfoFile.h"
#include "array_fun.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"
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
  CPPUNIT_TEST(testEd2kPeerSchedulingSkipsBackoff);
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
  void testEd2kPeerSchedulingSkipsBackoff();
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
