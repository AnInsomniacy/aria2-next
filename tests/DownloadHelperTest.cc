#include "download_helper.h"

#include <string>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <vector>

#include <cppunit/extensions/HelperMacros.h>

#include "RequestGroup.h"
#include "DownloadEngine.h"
#include "DownloadContext.h"
#include "DefaultPieceStorage.h"
#include "DiskAdaptor.h"
#include "DlRetryEx.h"
#include "Ed2kAttribute.h"
#include "Ed2kKadCommand.h"
#include "Ed2kPeerTransfer.h"
#include "Ed2kUploadQueue.h"
#include "Option.h"
#include "Piece.h"
#include "RequestGroupMan.h"
#include "SelectEventPoll.h"
#include "Segment.h"
#include "SegmentMan.h"
#include "DefaultBtProgressInfoFile.h"
#include "array_fun.h"
#include "base32.h"
#include "ed2k_constants.h"
#include "ed2k_aich.h"
#include "ed2k_endpoint.h"
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
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KClientHash);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KDefaultKadBootstrap);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KDefaultMacKadBootstrap);
  CPPUNIT_TEST(testCreateEd2kSearchRequestGroupClientHash);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KNodesDat);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KServerMetMetadata);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KKadRoutingState);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KServerState);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KMultipleServerStates);
  CPPUNIT_TEST(testCreateRequestGroupForUri_ED2KDefaultServers);
  CPPUNIT_TEST(testEd2kPeerDeduplication);
  CPPUNIT_TEST(testEd2kKadSourcePeerMergePreservesUdpMetadata);
  CPPUNIT_TEST(testEd2kServerSourceMergeSkipsUnsupportedSources);
  CPPUNIT_TEST(testEd2kSourceExchangeMergePolicy);
  CPPUNIT_TEST(testEd2kSourcePolicyRanksSources);
  CPPUNIT_TEST(testEd2kSourcePolicyClassifiesLifecycle);
  CPPUNIT_TEST(testEd2kLowIdCallbackStateTransitions);
  CPPUNIT_TEST(testEd2kPeerActionPolicySelectsConnect);
  CPPUNIT_TEST(testEd2kPeerActionPolicyReportsQueuedReask);
  CPPUNIT_TEST(testEd2kPeerActionPolicyHandlesCallbackAndExpiry);
  CPPUNIT_TEST(testEd2kPeerActionPolicyIsolatesUnreachableLowId);
  CPPUNIT_TEST(testEd2kConnectPolicyIgnoresNonConnectActions);
  CPPUNIT_TEST(testEd2kSourcePolicyExpiresDeadSources);
  CPPUNIT_TEST(testEd2kPeerUdpReaskStateTransitions);
  CPPUNIT_TEST(testEd2kPeerUdpReaskReplyMatchesUdpPort);
  CPPUNIT_TEST(testEd2kPeerUdpReaskDueSelection);
  CPPUNIT_TEST(testEd2kKadCommandQueuesDuePeerReask);
  CPPUNIT_TEST(testEd2kKadCommandQueuesKadCallback);
  CPPUNIT_TEST(testEd2kKadCommandQueueFullForUnknownUploadReask);
  CPPUNIT_TEST(testEd2kKadCommandAckForUploadingPeerReask);
  CPPUNIT_TEST(testEd2kSourcePolicyAppliesActiveCap);
  CPPUNIT_TEST(testEd2kServerSourceCadencePolicy);
  CPPUNIT_TEST(testEd2kServerSearchCadencePolicy);
  CPPUNIT_TEST(testEd2kPiecePolicyUsesPeerAvailability);
  CPPUNIT_TEST(testEd2kPiecePolicyReclaimsIdlePeerSegment);
  CPPUNIT_TEST(testEd2kPeerTransferRemovesCompletedRequestedRanges);
  CPPUNIT_TEST(testEd2kPeerTransferExpiresStalledRequests);
  CPPUNIT_TEST(testEd2kPeerTransferReclaimsStalledEndgameRange);
  CPPUNIT_TEST(testEd2kPeerTransferIgnoresDuplicateData);
  CPPUNIT_TEST(testEd2kPeerTransferAcceptsParallelPieceBlocks);
  CPPUNIT_TEST(testEd2kPeerTransferCancelsOwnerAfterParallelHashFailure);
  CPPUNIT_TEST(testEd2kPeerTransferAppliesAichRecoveryData);
  CPPUNIT_TEST(testEd2kSchedulingKeepsInlineSourceLabel);
  CPPUNIT_TEST(testEd2kPeerSchedulingSkipsBackoff);
  CPPUNIT_TEST(testEd2kPeerSchedulingSkipsConnectingPeer);
  CPPUNIT_TEST(testEd2kServerStateUpdate);
  CPPUNIT_TEST(testEd2kSearchResultDeduplication);
  CPPUNIT_TEST(testEd2kSearchResultMergesNetworks);
  CPPUNIT_TEST(testEd2kSearchResultAppliesLocalFilters);
  CPPUNIT_TEST(testCreateRequestGroupForUri_parameterized);
  CPPUNIT_TEST(testCreateRequestGroupForUriList);

#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testCreateRequestGroupForUri_LibtorrentTorrent);
  CPPUNIT_TEST(testCreateRequestGroupForUri_LibtorrentMagnet);
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
  void testCreateRequestGroupForUri_ED2KClientHash();
  void testCreateRequestGroupForUri_ED2KDefaultKadBootstrap();
  void testCreateRequestGroupForUri_ED2KDefaultMacKadBootstrap();
  void testCreateEd2kSearchRequestGroupClientHash();
  void testCreateRequestGroupForUri_ED2KNodesDat();
  void testCreateRequestGroupForUri_ED2KServerMetMetadata();
  void testCreateRequestGroupForUri_ED2KKadRoutingState();
  void testCreateRequestGroupForUri_ED2KServerState();
  void testCreateRequestGroupForUri_ED2KMultipleServerStates();
  void testCreateRequestGroupForUri_ED2KDefaultServers();
  void testEd2kPeerDeduplication();
  void testEd2kKadSourcePeerMergePreservesUdpMetadata();
  void testEd2kServerSourceMergeSkipsUnsupportedSources();
  void testEd2kSourceExchangeMergePolicy();
  void testEd2kSourcePolicyRanksSources();
  void testEd2kSourcePolicyClassifiesLifecycle();
  void testEd2kLowIdCallbackStateTransitions();
  void testEd2kPeerActionPolicySelectsConnect();
  void testEd2kPeerActionPolicyReportsQueuedReask();
  void testEd2kPeerActionPolicyHandlesCallbackAndExpiry();
  void testEd2kPeerActionPolicyIsolatesUnreachableLowId();
  void testEd2kConnectPolicyIgnoresNonConnectActions();
  void testEd2kSourcePolicyExpiresDeadSources();
  void testEd2kPeerUdpReaskStateTransitions();
  void testEd2kPeerUdpReaskReplyMatchesUdpPort();
  void testEd2kPeerUdpReaskDueSelection();
  void testEd2kKadCommandQueuesDuePeerReask();
  void testEd2kKadCommandQueuesKadCallback();
  void testEd2kKadCommandQueueFullForUnknownUploadReask();
  void testEd2kKadCommandAckForUploadingPeerReask();
  void testEd2kSourcePolicyAppliesActiveCap();
  void testEd2kServerSourceCadencePolicy();
  void testEd2kServerSearchCadencePolicy();
  void testEd2kPiecePolicyUsesPeerAvailability();
  void testEd2kPiecePolicyReclaimsIdlePeerSegment();
  void testEd2kPeerTransferRemovesCompletedRequestedRanges();
  void testEd2kPeerTransferExpiresStalledRequests();
  void testEd2kPeerTransferReclaimsStalledEndgameRange();
  void testEd2kPeerTransferIgnoresDuplicateData();
  void testEd2kPeerTransferAcceptsParallelPieceBlocks();
  void testEd2kPeerTransferCancelsOwnerAfterParallelHashFailure();
  void testEd2kPeerTransferAppliesAichRecoveryData();
  void testEd2kSchedulingKeepsInlineSourceLabel();
  void testEd2kPeerSchedulingSkipsBackoff();
  void testEd2kPeerSchedulingSkipsConnectingPeer();
  void testEd2kServerStateUpdate();
  void testEd2kSearchResultDeduplication();
  void testEd2kSearchResultMergesNetworks();
  void testEd2kSearchResultAppliesLocalFilters();
  void testCreateRequestGroupForUri_parameterized();
  void testCreateRequestGroupForUriList();

#ifdef ENABLE_BITTORRENT
  void testCreateRequestGroupForUri_LibtorrentTorrent();
  void testCreateRequestGroupForUri_LibtorrentMagnet();
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
  CPPUNIT_ASSERT_EQUAL(std::string("/tmp/aria2_next.bin"),
                       ctx->getBasePath());
  CPPUNIT_ASSERT(ctx->getFirstFileEntry()->isRequested());
  CPPUNIT_ASSERT_EQUAL((size_t)0,
                       ctx->getFirstFileEntry()->getRemainingUris().size());
  CPPUNIT_ASSERT(!ctx->isChecksumVerificationAvailable());
  CPPUNIT_ASSERT(!ctx->isPieceHashVerificationAvailable());
  CPPUNIT_ASSERT_EQUAL(4, group->getNumConcurrentCommand());

  auto attrs = getEd2kAttrs(ctx);
  CPPUNIT_ASSERT_EQUAL(std::string("aria2_next.bin"), attrs->link.name);
  CPPUNIT_ASSERT_EQUAL(
      std::string("0123456789abcdef0123456789abcdef"),
      util::toHex(attrs->link.hash));
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs->link.pieceHashes.size());
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs->servers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), attrs->servers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, attrs->servers[0].port);

  option_->remove(PREF_SPLIT);
  result.clear();
  createRequestGroupForUri(result, option_, uris);
  CPPUNIT_ASSERT_EQUAL(ed2k::DEFAULT_PEER_CONNECTIONS,
                       result[0]->getNumConcurrentCommand());
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KClientHash()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_ED2K_CLIENT_HASH,
               "0102030405060708090a0b0c0d0e0f10");

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);

  CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT_EQUAL(std::string("01020304050e0708090a0b0c0d0e6f10"),
                       util::toHex(attrs->clientHash));
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KDefaultKadBootstrap()
{
  std::string nodeIdHex("23a8ceff57a7a32d562d649ed7893796");
  auto nodeId = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  ed2k::KadContact contact;
  contact.id = nodeId;
  contact.host = "203.0.113.1";
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
  const std::string home = A2_TEST_OUT_DIR "/ed2k-default-home";
  const std::string amuleDir = home + "/.aMule";
  File(amuleDir).mkdirs();
  const std::string path = amuleDir + "/nodes.dat";
  {
    std::ofstream out(path.c_str(), std::ios::binary);
    out.write(nodesDat.data(), nodesDat.size());
  }

  const char* oldHome = getenv("HOME");
  const std::string oldHomeValue = oldHome ? oldHome : "";
  setenv("HOME", home.c_str(), 1);

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");

  std::vector<std::shared_ptr<RequestGroup>> result;
  try {
    createRequestGroupForUri(result, option_, uris);
  }
  catch (...) {
    if (oldHome) {
      setenv("HOME", oldHomeValue.c_str(), 1);
    }
    else {
      unsetenv("HOME");
    }
    throw;
  }
  if (oldHome) {
    setenv("HOME", oldHomeValue.c_str(), 1);
  }
  else {
    unsetenv("HOME");
  }

  CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT(attrs->kadRoutingTable);
  CPPUNIT_ASSERT(!attrs->kadRoutingTable->getRouterNodes().empty());
  CPPUNIT_ASSERT_EQUAL(ed2k::ed2kHashToKadId(attrs->clientHash),
                       attrs->kadRoutingTable->snapshot().selfId);
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KDefaultMacKadBootstrap()
{
  std::string nodeIdHex("23a8ceff57a7a32d562d649ed7893796");
  auto nodeId = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  ed2k::KadContact contact;
  contact.id = nodeId;
  contact.host = "203.0.113.2";
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
  const std::string home = A2_TEST_OUT_DIR "/ed2k-default-mac-home";
  const std::string amuleDir = home + "/Library/Application Support/aMule";
  File(amuleDir).mkdirs();
  const std::string path = amuleDir + "/nodes.dat";
  {
    std::ofstream out(path.c_str(), std::ios::binary);
    out.write(nodesDat.data(), nodesDat.size());
  }

  const char* oldHome = getenv("HOME");
  const std::string oldHomeValue = oldHome ? oldHome : "";
  setenv("HOME", home.c_str(), 1);

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");

  std::vector<std::shared_ptr<RequestGroup>> result;
  try {
    createRequestGroupForUri(result, option_, uris);
  }
  catch (...) {
    if (oldHome) {
      setenv("HOME", oldHomeValue.c_str(), 1);
    }
    else {
      unsetenv("HOME");
    }
    throw;
  }
  if (oldHome) {
    setenv("HOME", oldHomeValue.c_str(), 1);
  }
  else {
    unsetenv("HOME");
  }

  CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT(!attrs->servers.empty());
  CPPUNIT_ASSERT(attrs->kadRoutingTable);
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       attrs->kadRoutingTable->getRouterNodes().size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.2"),
                       attrs->kadRoutingTable->getRouterNodes()[0].host);
  CPPUNIT_ASSERT_EQUAL(ed2k::ed2kHashToKadId(attrs->clientHash),
                       attrs->kadRoutingTable->snapshot().selfId);
}

void DownloadHelperTest::testCreateEd2kSearchRequestGroupClientHash()
{
  option_->put(PREF_ED2K_CLIENT_HASH,
               "0102030405060708090a0b0c0d0e0f10");
  option_->put(PREF_ED2K_SERVER, "203.0.113.10:4661");
  ed2k::SearchQuery query;
  query.keyword = "test";

  auto group = createEd2kSearchRequestGroup(query, option_);
  auto attrs = getEd2kAttrs(group->getDownloadContext());

  CPPUNIT_ASSERT(attrs->searchActive);
  CPPUNIT_ASSERT_EQUAL(std::string("01020304050e0708090a0b0c0d0e6f10"),
                       util::toHex(attrs->clientHash));
  CPPUNIT_ASSERT(attrs->link.hash.empty());
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KNodesDat()
{
  std::string nodeIdHex("23a8ceff57a7a32d562d649ed7893796");
  auto nodeId = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  ed2k::KadContact contact;
  contact.id = nodeId;
  contact.host = "203.0.113.1";
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
  CPPUNIT_ASSERT(!attrs->servers.empty());
  CPPUNIT_ASSERT(attrs->kadRoutingTable);
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       attrs->kadRoutingTable->getRouterNodes().size());
  CPPUNIT_ASSERT_EQUAL(ed2k::ed2kHashToKadId(attrs->clientHash),
                       attrs->kadRoutingTable->snapshot().selfId);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.1"),
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
  serverMet += ed2k::packUInt32(5);
  serverMet += ed2k::createStringTag(0x01, "Peer Server");
  serverMet += ed2k::createStringTag(0x0b, "Primary ED2K server");
  serverMet += ed2k::createUInt32Tag(0x87, 9000);
  serverMet += ed2k::createUInt32Tag(0x92, 0x01020304);
  serverMet += ed2k::createUInt32Tag(0x97, 4666);
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
  CPPUNIT_ASSERT_EQUAL((uint32_t)9000, attrs->serverStates[0].maxUsers);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x01020304,
                       attrs->serverStates[0].udpFlags);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666,
                       attrs->serverStates[0].tcpObfuscationPort);
}

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KKadRoutingState()
{
  std::string selfHex("0123456789abcdef0123456789abcdef");
  auto self = util::fromHex(selfHex.begin(), selfHex.end());
  ed2k::KadRoutingTable table(self);
  ed2k::KadRoutingSnapshot snapshot;
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
  snapshot = table.snapshot();
  snapshot.lastFirewalledCheck = 500;
  snapshot.lastSourcePublish = 600;
  snapshot.firewalled = false;
  snapshot.observedAddresses.push_back("203.0.113.55");

  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, A2_TEST_OUT_DIR "/ed2k-command-state");
  File(A2_TEST_OUT_DIR "/ed2k-command-state").mkdirs();
  option_->put(PREF_ED2K_KAD_ROUTING_STATE,
               util::toHex(ed2k::createKadRoutingStatePayload(snapshot)));

  std::vector<std::shared_ptr<RequestGroup>> result;

  createRequestGroupForUri(result, option_, uris);

  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT(attrs->kadRoutingTable);
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->kadRoutingTable->liveSize());
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       attrs->kadRoutingTable->getRouterNodes().size());
  CPPUNIT_ASSERT_EQUAL(ed2k::ed2kHashToKadId(attrs->clientHash),
                       attrs->kadRoutingTable->snapshot().selfId);
  auto closest = attrs->kadRoutingTable->findClosest(self, 1, false);
  CPPUNIT_ASSERT_EQUAL((size_t)1, closest.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.8"), closest[0].host);
  CPPUNIT_ASSERT_EQUAL((int64_t)500, attrs->lastKadFirewalledCheck);
  CPPUNIT_ASSERT_EQUAL((int64_t)600, attrs->lastKadSourcePublish);
  CPPUNIT_ASSERT(!attrs->kadFirewalled);
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->kadObservedAddresses.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.55"),
                       attrs->kadObservedAddresses[0]);
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

void DownloadHelperTest::testCreateRequestGroupForUri_ED2KDefaultServers()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->remove(PREF_ED2K_SERVER);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);

  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  CPPUNIT_ASSERT_EQUAL((size_t)7, attrs->servers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("45.82.80.155"), attrs->servers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)5687, attrs->servers[0].port);
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

void DownloadHelperTest::testEd2kKadSourcePeerMergePreservesUdpMetadata()
{
  Ed2kAttribute attrs;
  ed2k::KadSourceEndpoint source;
  source.endpoint.host = "203.0.113.44";
  source.endpoint.port = 4662;
  source.endpoint.userHash = std::string(ed2k::HASH_LENGTH, '\x44');
  source.endpoint.cryptOptions = 0x03;
  source.udpPort = 4672;
  source.sourceType = 1;

  CPPUNIT_ASSERT(addEd2kKadSourcePeer(&attrs, source,
                                      ed2k::PEER_SOURCE_KAD));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peers.size());
  auto state = getEd2kPeerState(&attrs, source.endpoint);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL(std::string(ed2k::HASH_LENGTH, '\x44'),
                       state->endpoint.userHash);
  CPPUNIT_ASSERT_EQUAL((uint16_t)0x03, state->endpoint.cryptOptions);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672, state->udpPort);
  CPPUNIT_ASSERT_EQUAL((uint8_t)4, state->udpVersion);

  source.udpPort = 4682;
  CPPUNIT_ASSERT(!addEd2kKadSourcePeer(&attrs, source,
                                       ed2k::PEER_SOURCE_KAD));
  CPPUNIT_ASSERT_EQUAL((uint16_t)4682, state->udpPort);

  source.endpoint.host = "203.0.113.45";
  source.sourceType = 3;
  source.buddyIp = ed2k::ipv4ToEndpointValue("203.0.113.99");
  source.buddyPort = 4672;
  source.buddyHash = std::string(ed2k::HASH_LENGTH, '\x55');
  CPPUNIT_ASSERT(!addEd2kKadSourcePeer(&attrs, source,
                                       ed2k::PEER_SOURCE_KAD));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peers.size());
  auto callbackState = getEd2kPeerState(&attrs, source.endpoint);
  CPPUNIT_ASSERT(callbackState);
  CPPUNIT_ASSERT(callbackState->lowId);
  CPPUNIT_ASSERT(callbackState->callbackRequested);
  CPPUNIT_ASSERT_EQUAL(ed2k::LowIdCallbackState::REQUESTED,
                       callbackState->lowIdCallbackState);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.99"),
                       callbackState->callbackBuddy.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672, callbackState->callbackBuddy.port);
  CPPUNIT_ASSERT_EQUAL(
      ed2k::ed2kHashToKadId(std::string(ed2k::HASH_LENGTH, '\x55')),
      callbackState->callbackBuddyId);
}

void DownloadHelperTest::testEd2kServerSourceMergeSkipsUnsupportedSources()
{
  Ed2kAttribute attrs;
  ed2k::FoundSource direct;
  direct.endpoint.host = "203.0.113.10";
  direct.endpoint.port = 4662;
  ed2k::FoundSource lowId;
  lowId.endpoint.host = "120.0.0.0";
  lowId.endpoint.port = 4662;
  lowId.clientId = 0x01020304;
  lowId.lowId = true;
  ed2k::FoundSource cryptRequired;
  cryptRequired.endpoint.host = "203.0.113.11";
  cryptRequired.endpoint.port = 4662;
  cryptRequired.endpoint.cryptOptions = ed2k::SOURCE_CRYPT_REQUIRE;

  CPPUNIT_ASSERT_EQUAL(
      (size_t)1,
      mergeEd2kServerSources(
          &attrs, std::vector<ed2k::FoundSource>{direct, lowId, cryptRequired},
          ed2k::PEER_SOURCE_SERVER));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peers.size());
  CPPUNIT_ASSERT_EQUAL(direct.endpoint.host, attrs.peers[0].host);
  auto state = getEd2kPeerState(&attrs, direct.endpoint);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT((state->sourceFlags & ed2k::PEER_SOURCE_SERVER) != 0);
  state = getEd2kPeerState(&attrs, lowId.endpoint);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->lowId);
  CPPUNIT_ASSERT(state->callbackImpossible);
  CPPUNIT_ASSERT(!state->callbackRequested);
  CPPUNIT_ASSERT((state->sourceFlags & ed2k::PEER_SOURCE_SERVER) != 0);
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peers.size());
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrs.peerStates.size());
  CPPUNIT_ASSERT(!ed2k::selectConnectPeer(attrs.peerStates, 0)->lowId);

  Ed2kAttribute callbackAttrs;
  CPPUNIT_ASSERT(!addEd2kFoundSource(
      &callbackAttrs, lowId, ed2k::PEER_SOURCE_SERVER, true));
  state = getEd2kPeerState(&callbackAttrs, lowId.endpoint);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->lowId);
  CPPUNIT_ASSERT(state->callbackRequested);
  CPPUNIT_ASSERT(!state->callbackImpossible);
  CPPUNIT_ASSERT(!ed2k::selectConnectPeer(callbackAttrs.peerStates, 0));
  CPPUNIT_ASSERT(markEd2kCallbackFailed(&callbackAttrs, lowId.clientId));
  CPPUNIT_ASSERT(!state->callbackRequested);
  CPPUNIT_ASSERT(state->callbackImpossible);
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

  ed2k::Endpoint cryptRequired;
  cryptRequired.host = "203.0.113.13";
  cryptRequired.port = 4662;
  cryptRequired.cryptOptions = ed2k::SOURCE_CRYPT_REQUIRE;
  addEd2kPeer(&attrs, cryptRequired, ed2k::PEER_SOURCE_SERVER);
  auto cryptState = getEd2kPeerState(&attrs, cryptRequired);
  cryptState->failCount = 0;
  selected = ed2k::selectConnectPeer(attrs.peerStates, 40);

  CPPUNIT_ASSERT(selected);
  CPPUNIT_ASSERT(selected->endpoint.host != cryptRequired.host);
}

void DownloadHelperTest::testEd2kSourcePolicyClassifiesLifecycle()
{
  ed2k::PeerState peer;
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::USEFUL,
                       ed2k::classifyPeerLifecycle(peer, 100));
  peer.connecting = true;
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::CONNECTING,
                       ed2k::classifyPeerLifecycle(peer, 100));
  peer.connecting = false;
  peer.queued = true;
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::QUEUED,
                       ed2k::classifyPeerLifecycle(peer, 100));
  peer.outOfParts = true;
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::NO_NEEDED_PARTS,
                       ed2k::classifyPeerLifecycle(peer, 100));
  peer.outOfParts = false;
  peer.dead = true;
  peer.nextRetryTime = 150;
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::DEAD,
                       ed2k::classifyPeerLifecycle(peer, 100));
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::RETRYING,
                       ed2k::classifyPeerLifecycle(peer, 160));
  peer.noFile = true;
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::NO_FILE,
                       ed2k::classifyPeerLifecycle(peer, 160));
  peer.noFile = false;
  peer.cancelled = true;
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::CANCELLED,
                       ed2k::classifyPeerLifecycle(peer, 160));
}

void DownloadHelperTest::testEd2kLowIdCallbackStateTransitions()
{
  Ed2kAttribute attrs;
  ed2k::FoundSource source;
  source.endpoint.host = "120.0.0.0";
  source.endpoint.port = 4662;
  source.clientId = 0x00000120;
  source.lowId = true;

  CPPUNIT_ASSERT(!addEd2kFoundSource(&attrs, source,
                                     ed2k::PEER_SOURCE_SERVER, true));
  auto state = getEd2kPeerState(&attrs, source.endpoint);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL(ed2k::LowIdCallbackState::REQUESTED,
                       state->lowIdCallbackState);
  CPPUNIT_ASSERT(state->callbackRequested);
  CPPUNIT_ASSERT(!state->callbackImpossible);
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::CALLBACK_WAITING,
                       ed2k::classifyPeerLifecycle(*state, 100));
  CPPUNIT_ASSERT(markEd2kCallbackRequestSent(&attrs, source.clientId,
                                             105, 45));
  CPPUNIT_ASSERT_EQUAL((int64_t)105, state->lastCallbackTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)150, state->callbackDeadline);

  ed2k::Endpoint callbackPeer;
  callbackPeer.host = "203.0.113.44";
  callbackPeer.port = 4662;
  CPPUNIT_ASSERT(markEd2kCallbackAccepted(&attrs, source.clientId,
                                          callbackPeer, 120));
  state = getEd2kPeerState(&attrs, callbackPeer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL(ed2k::LowIdCallbackState::ACCEPTED,
                       state->lowIdCallbackState);
  CPPUNIT_ASSERT(!state->lowId);
  CPPUNIT_ASSERT(!state->callbackRequested);
  CPPUNIT_ASSERT(!state->callbackImpossible);
  CPPUNIT_ASSERT_EQUAL(source.clientId, state->clientId);
  CPPUNIT_ASSERT_EQUAL((int64_t)120, state->lastCallbackTime);
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerLifecycle::USEFUL,
                       ed2k::classifyPeerLifecycle(*state, 121));

  CPPUNIT_ASSERT(markEd2kCallbackCompleted(&attrs, callbackPeer));
  CPPUNIT_ASSERT_EQUAL(ed2k::LowIdCallbackState::COMPLETED,
                       state->lowIdCallbackState);

  Ed2kAttribute failAttrs;
  CPPUNIT_ASSERT(!addEd2kFoundSource(&failAttrs, source,
                                     ed2k::PEER_SOURCE_SERVER, true));
  state = getEd2kPeerState(&failAttrs, source.endpoint);
  CPPUNIT_ASSERT(markEd2kCallbackFailed(&failAttrs, source.clientId, 180, 30));
  CPPUNIT_ASSERT_EQUAL(ed2k::LowIdCallbackState::FAILED,
                       state->lowIdCallbackState);
  CPPUNIT_ASSERT(state->callbackImpossible);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT_EQUAL((int64_t)210, state->nextRetryTime);

  CPPUNIT_ASSERT(expireEd2kCallbackWaits(&failAttrs, 211) == 1);
  CPPUNIT_ASSERT_EQUAL(ed2k::LowIdCallbackState::IMPOSSIBLE,
                       state->lowIdCallbackState);
  CPPUNIT_ASSERT(!state->dead);
  CPPUNIT_ASSERT(state->callbackImpossible);

  Ed2kAttribute timeoutAttrs;
  CPPUNIT_ASSERT(!addEd2kFoundSource(&timeoutAttrs, source,
                                     ed2k::PEER_SOURCE_SERVER, true));
  state = getEd2kPeerState(&timeoutAttrs, source.endpoint);
  CPPUNIT_ASSERT(markEd2kCallbackRequestSent(&timeoutAttrs, source.clientId,
                                             300, 45));
  CPPUNIT_ASSERT(expireEd2kCallbackWaits(&timeoutAttrs, 345) == 1);
  CPPUNIT_ASSERT_EQUAL(ed2k::LowIdCallbackState::TIMED_OUT,
                       state->lowIdCallbackState);
  CPPUNIT_ASSERT(state->callbackImpossible);
}

void DownloadHelperTest::testEd2kPeerActionPolicySelectsConnect()
{
  std::vector<ed2k::PeerState> peers(2);
  peers[0].endpoint.host = "203.0.113.10";
  peers[0].endpoint.port = 4662;
  peers[0].sourceFlags = ed2k::PEER_SOURCE_EXCHANGE;
  peers[0].failCount = 1;
  peers[1].endpoint.host = "203.0.113.11";
  peers[1].endpoint.port = 4662;
  peers[1].sourceFlags = ed2k::PEER_SOURCE_SERVER;

  auto action = ed2k::selectPeerAction(peers, 100, 1);

  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::CONNECT, action.type);
  CPPUNIT_ASSERT(action.peer);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.11"), action.peer->endpoint.host);
  action.peer->connecting = true;

  action = ed2k::selectPeerAction(peers, 100, 1);
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::WAIT, action.type);
  CPPUNIT_ASSERT(!action.peer);
}

void DownloadHelperTest::testEd2kPeerActionPolicyReportsQueuedReask()
{
  std::vector<ed2k::PeerState> peers(1);
  peers[0].endpoint.host = "203.0.113.10";
  peers[0].endpoint.port = 4662;
  peers[0].queued = true;
  peers[0].queueRank = 12;

  auto action = ed2k::selectPeerAction(peers, 100, 1);

  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::REASK, action.type);
  CPPUNIT_ASSERT(action.peer);
  CPPUNIT_ASSERT_EQUAL((uint16_t)12, action.peer->queueRank);
}

void DownloadHelperTest::testEd2kPeerActionPolicyHandlesCallbackAndExpiry()
{
  std::vector<ed2k::PeerState> peers(3);
  peers[0].endpoint.host = "203.0.113.10";
  peers[0].endpoint.port = 4662;
  peers[0].lowId = true;
  peers[0].callbackRequested = true;
  peers[0].lowIdCallbackState = ed2k::LowIdCallbackState::REQUESTED;
  peers[0].clientId = 42;
  peers[1].endpoint.host = "203.0.113.11";
  peers[1].endpoint.port = 4662;
  peers[1].dead = true;
  peers[1].nextRetryTime = 120;
  peers[2].endpoint.host = "203.0.113.12";
  peers[2].endpoint.port = 4662;
  peers[2].noFile = true;

  auto action = ed2k::selectPeerAction(peers, 100, 1);
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::REQUEST_CALLBACK, action.type);
  CPPUNIT_ASSERT(action.peer);
  CPPUNIT_ASSERT_EQUAL((uint32_t)42, action.peer->clientId);

  peers[0].callbackRequested = false;
  peers[0].callbackImpossible = true;
  peers[0].lowIdCallbackState = ed2k::LowIdCallbackState::IMPOSSIBLE;
  action = ed2k::selectPeerAction(peers, 130, 1);
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::RETRY, action.type);
  CPPUNIT_ASSERT(action.peer);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.11"), action.peer->endpoint.host);
}

void DownloadHelperTest::testEd2kPeerActionPolicyIsolatesUnreachableLowId()
{
  std::vector<ed2k::PeerState> peers(2);
  peers[0].endpoint.host = "120.0.0.0";
  peers[0].endpoint.port = 4662;
  peers[0].lowId = true;
  peers[0].clientId = 0x00000120;
  peers[0].callbackImpossible = true;
  peers[0].lowIdCallbackState = ed2k::LowIdCallbackState::IMPOSSIBLE;
  peers[1].endpoint.host = "203.0.113.12";
  peers[1].endpoint.port = 4662;

  auto action = ed2k::selectPeerAction(peers, 100, 1);

  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::CONNECT, action.type);
  CPPUNIT_ASSERT(action.peer);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.12"), action.peer->endpoint.host);
  CPPUNIT_ASSERT(!ed2k::selectConnectPeer(peers, 100)->lowId);

  peers[1].connecting = true;
  action = ed2k::selectPeerAction(peers, 100, 1);
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::WAIT, action.type);
  CPPUNIT_ASSERT(!action.peer);
}

void DownloadHelperTest::testEd2kConnectPolicyIgnoresNonConnectActions()
{
  std::vector<ed2k::PeerState> peers(3);
  peers[0].endpoint.host = "203.0.113.10";
  peers[0].endpoint.port = 4662;
  peers[0].queued = true;
  peers[0].queueRank = 1;
  peers[1].endpoint.host = "203.0.113.11";
  peers[1].endpoint.port = 4662;
  peers[1].lowId = true;
  peers[1].callbackRequested = true;
  peers[2].endpoint.host = "203.0.113.12";
  peers[2].endpoint.port = 4662;
  peers[2].dead = true;
  peers[2].nextRetryTime = 100;

  auto selected = ed2k::selectConnectPeer(peers, 130, 1);

  CPPUNIT_ASSERT(selected);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.12"), selected->endpoint.host);
}

void DownloadHelperTest::testEd2kSourcePolicyExpiresDeadSources()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint peer;
  peer.host = "203.0.113.10";
  peer.port = 4662;
  addEd2kPeer(&attrs, peer, ed2k::PEER_SOURCE_SERVER);
  CPPUNIT_ASSERT(markEd2kPeerDead(&attrs, peer, 100, 30));
  auto state = getEd2kPeerState(&attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT(state->noFile);

  CPPUNIT_ASSERT_EQUAL((size_t)0, expireEd2kDeadSources(&attrs, 120));
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT_EQUAL((size_t)1, expireEd2kDeadSources(&attrs, 131));
  CPPUNIT_ASSERT(!state->dead);
  CPPUNIT_ASSERT(!state->noFile);
  CPPUNIT_ASSERT_EQUAL((int64_t)0, state->nextRetryTime);
}

void DownloadHelperTest::testEd2kPeerUdpReaskStateTransitions()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint peer;
  peer.host = "203.0.113.10";
  peer.port = 4662;
  addEd2kPeer(&attrs, peer, ed2k::PEER_SOURCE_SERVER);
  CPPUNIT_ASSERT(markEd2kPeerQueued(&attrs, peer, 7,
                                    std::vector<bool>{true, false}));
  auto state = getEd2kPeerState(&attrs, peer);
  state->udpPort = 4672;
  state->udpVersion = 4;

  CPPUNIT_ASSERT(markEd2kPeerUdpReaskSent(&attrs, peer, 100));
  CPPUNIT_ASSERT(state->udpReaskPending);
  CPPUNIT_ASSERT_EQUAL((int64_t)100, state->lastUdpReaskTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)1400, state->nextUdpReaskTime);

  CPPUNIT_ASSERT(markEd2kPeerUdpReaskAck(&attrs, peer, 3,
                                         std::vector<bool>{false, true},
                                         120));
  CPPUNIT_ASSERT(!state->udpReaskPending);
  CPPUNIT_ASSERT(!state->remoteQueueFull);
  CPPUNIT_ASSERT_EQUAL((uint16_t)3, state->queueRank);
  CPPUNIT_ASSERT_EQUAL((int64_t)120, state->lastUdpReaskTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)1420, state->nextUdpReaskTime);
  CPPUNIT_ASSERT_EQUAL((size_t)2, state->partStatus.size());
  CPPUNIT_ASSERT(!state->partStatus[0]);
  CPPUNIT_ASSERT(state->partStatus[1]);

  CPPUNIT_ASSERT(markEd2kPeerQueueFull(&attrs, peer, 200, 30));
  CPPUNIT_ASSERT(!state->udpReaskPending);
  CPPUNIT_ASSERT(state->remoteQueueFull);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT(!state->noFile);
  CPPUNIT_ASSERT_EQUAL((int64_t)230, state->nextRetryTime);
}

void DownloadHelperTest::testEd2kPeerUdpReaskReplyMatchesUdpPort()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint peer;
  peer.host = "203.0.113.10";
  peer.port = 4662;
  addEd2kPeer(&attrs, peer, ed2k::PEER_SOURCE_SERVER);
  markEd2kPeerQueued(&attrs, peer, 7, std::vector<bool>{true});
  auto state = getEd2kPeerState(&attrs, peer);
  state->udpPort = 4672;
  state->udpVersion = 4;
  state->udpReaskPending = true;

  ed2k::Endpoint udpPeer = peer;
  udpPeer.port = 4672;

  CPPUNIT_ASSERT(markEd2kPeerUdpReaskAck(&attrs, udpPeer, 5,
                                         std::vector<bool>{false}, 100));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peerStates.size());
  CPPUNIT_ASSERT(!state->udpReaskPending);
  CPPUNIT_ASSERT_EQUAL((uint16_t)5, state->queueRank);

  CPPUNIT_ASSERT(markEd2kPeerQueueFull(&attrs, udpPeer, 200, 30));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.peerStates.size());
  CPPUNIT_ASSERT(state->remoteQueueFull);
  CPPUNIT_ASSERT(state->dead);
}

void DownloadHelperTest::testEd2kPeerUdpReaskDueSelection()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint waiting;
  waiting.host = "203.0.113.10";
  waiting.port = 4662;
  addEd2kPeer(&attrs, waiting, ed2k::PEER_SOURCE_SERVER);
  markEd2kPeerQueued(&attrs, waiting, 8, std::vector<bool>{true});
  auto waitingState = getEd2kPeerState(&attrs, waiting);
  waitingState->udpPort = 4672;
  waitingState->udpVersion = 4;
  waitingState->nextUdpReaskTime = 500;

  ed2k::Endpoint due;
  due.host = "203.0.113.11";
  due.port = 4662;
  addEd2kPeer(&attrs, due, ed2k::PEER_SOURCE_KAD);
  markEd2kPeerQueued(&attrs, due, 2, std::vector<bool>{true});
  auto dueState = getEd2kPeerState(&attrs, due);
  dueState->udpPort = 4672;
  dueState->udpVersion = 4;
  dueState->nextUdpReaskTime = 300;

  ed2k::Endpoint pending;
  pending.host = "203.0.113.12";
  pending.port = 4662;
  addEd2kPeer(&attrs, pending, ed2k::PEER_SOURCE_EXCHANGE);
  markEd2kPeerQueued(&attrs, pending, 1, std::vector<bool>{true});
  auto pendingState = getEd2kPeerState(&attrs, pending);
  pendingState->udpPort = 4672;
  pendingState->udpVersion = 4;
  pendingState->udpReaskPending = true;

  auto selected = selectDueEd2kUdpReaskPeer(&attrs, 400);

  CPPUNIT_ASSERT(selected);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.11"), selected->endpoint.host);
}

void DownloadHelperTest::testEd2kKadCommandQueuesDuePeerReask()
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
  addEd2kPeer(attrs, peer, ed2k::PEER_SOURCE_SERVER);
  markEd2kPeerQueued(attrs, peer, 4, std::vector<bool>{true});
  auto state = getEd2kPeerState(attrs, peer);
  state->udpPort = 4672;
  state->udpVersion = 4;
  state->nextUdpReaskTime = 100;

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  Ed2kKadCommand command(1, group.get(), &engine);

  CPPUNIT_ASSERT_EQUAL((size_t)1, command.testQueueDuePeerReasks(200));
  CPPUNIT_ASSERT_EQUAL((size_t)1, command.testQueuedPacketCount());
  const auto& item = command.testQueuedPacketAt(0);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.20"), item.first.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672, item.first.port);
  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(ed2k::readDatagramHeader(header, item.second.data(),
                                          item.second.size()));
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EMULE, header.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_REASKFILEPING, header.opcode);
  ed2k::UdpReask reask;
  CPPUNIT_ASSERT(ed2k::parseUdpReaskFilePingPayload(
      reask, item.second.substr(2)));
  CPPUNIT_ASSERT_EQUAL(attrs->link.hash, reask.fileHash);
  CPPUNIT_ASSERT(state->udpReaskPending);
  CPPUNIT_ASSERT_EQUAL((int64_t)200, state->lastUdpReaskTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)1500, state->nextUdpReaskTime);
}

void DownloadHelperTest::testEd2kKadCommandQueuesKadCallback()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  option_->put(PREF_DIR, "/tmp");
  option_->put(PREF_ED2K_SERVER, "203.0.113.10:4661");
  option_->put(PREF_ED2K_LISTEN_PORT, "4662");
  option_->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
  option_->put(PREF_MAX_UPLOAD_LIMIT, "0");
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  option_->put(PREF_DRY_RUN, A2_V_TRUE);

  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option_, uris);
  auto group = result[0];
  auto attrs = getEd2kAttrs(group->getDownloadContext());
  ed2k::KadSourceEndpoint source;
  source.endpoint.host = "203.0.113.44";
  source.endpoint.port = 4662;
  source.endpoint.userHash = std::string(ed2k::HASH_LENGTH, '\x44');
  source.udpPort = 4672;
  source.sourceType = 3;
  source.buddyIp = ed2k::ipv4ToEndpointValue("203.0.113.99");
  source.buddyPort = 4672;
  source.buddyHash = std::string(ed2k::HASH_LENGTH, '\x55');
  addEd2kKadSourcePeer(attrs, source, ed2k::PEER_SOURCE_KAD);
  auto state = getEd2kPeerState(attrs, source.endpoint);

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  Ed2kKadCommand command(1, group.get(), &engine);

  CPPUNIT_ASSERT_EQUAL((size_t)1, command.testQueueDueKadCallbacks(200));
  CPPUNIT_ASSERT_EQUAL((size_t)1, command.testQueuedPacketCount());
  const auto& item = command.testQueuedPacketAt(0);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.99"), item.first.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672, item.first.port);
  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(ed2k::readDatagramHeader(header, item.second.data(),
                                          item.second.size()));
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_PROTOCOL, header.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_CALLBACK_REQ, header.opcode);
  ed2k::KadCallbackRequest request;
  CPPUNIT_ASSERT(ed2k::parseKadCallbackRequestPayload(
      request, item.second.substr(2)));
  CPPUNIT_ASSERT_EQUAL(ed2k::ed2kHashToKadId(source.buddyHash),
                       request.buddyId);
  CPPUNIT_ASSERT_EQUAL(ed2k::ed2kHashToKadId(attrs->link.hash),
                       request.fileId);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, request.tcpPort);
  CPPUNIT_ASSERT_EQUAL((int64_t)200, state->lastCallbackTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)245, state->callbackDeadline);
  CPPUNIT_ASSERT_EQUAL((size_t)0, command.testQueueDueKadCallbacks(210));
}

void DownloadHelperTest::testEd2kKadCommandQueueFullForUnknownUploadReask()
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

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  Ed2kKadCommand command(1, group.get(), &engine);

  ed2k::Endpoint remote;
  remote.host = "203.0.113.30";
  remote.port = 4672;
  command.testHandleEd2kUdpPacket(
      remote, ed2k::OP_REASKFILEPING,
      ed2k::createUdpReaskFilePingPayload(attrs->link.hash));

  CPPUNIT_ASSERT_EQUAL((size_t)1, command.testQueuedPacketCount());
  const auto& item = command.testQueuedPacketAt(0);
  CPPUNIT_ASSERT_EQUAL(remote.host, item.first.host);
  CPPUNIT_ASSERT_EQUAL(remote.port, item.first.port);
  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(ed2k::readDatagramHeader(header, item.second.data(),
                                          item.second.size()));
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EMULE, header.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_QUEUEFULL, header.opcode);
  CPPUNIT_ASSERT_EQUAL((size_t)0, header.payloadSize());
}

void DownloadHelperTest::testEd2kKadCommandAckForUploadingPeerReask()
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

  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option_.get());
  engine.setRequestGroupMan(make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{group}, 1, option_.get()));
  auto uploadQueue = engine.getRequestGroupMan()->getEd2kUploadQueue();

  ed2k::Endpoint remote;
  remote.host = "203.0.113.31";
  remote.port = 4672;
  CPPUNIT_ASSERT(uploadQueue->requestUpload(remote,
                                            std::string(ed2k::HASH_LENGTH,
                                                        '\x44'),
                                            attrs->link.hash, 1000, nullptr));
  Ed2kKadCommand command(1, group.get(), &engine);
  command.testHandleEd2kUdpPacket(
      remote, ed2k::OP_REASKFILEPING,
      ed2k::createUdpReaskFilePingPayload(attrs->link.hash));

  CPPUNIT_ASSERT_EQUAL((size_t)1, command.testQueuedPacketCount());
  const auto& item = command.testQueuedPacketAt(0);
  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(ed2k::readDatagramHeader(header, item.second.data(),
                                          item.second.size()));
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EMULE, header.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_REASKACK, header.opcode);
  ed2k::UdpReaskAck ack;
  CPPUNIT_ASSERT(ed2k::parseUdpReaskAckPayload(ack, item.second.substr(2)));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, ack.rank);
}

void DownloadHelperTest::testEd2kSourcePolicyAppliesActiveCap()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint first;
  first.host = "203.0.113.10";
  first.port = 4662;
  ed2k::Endpoint second;
  second.host = "203.0.113.11";
  second.port = 4662;
  ed2k::Endpoint queued;
  queued.host = "203.0.113.12";
  queued.port = 4662;
  addEd2kPeer(&attrs, first, ed2k::PEER_SOURCE_SERVER);
  addEd2kPeer(&attrs, second, ed2k::PEER_SOURCE_KAD);
  addEd2kPeer(&attrs, queued, ed2k::PEER_SOURCE_EXCHANGE);
  markEd2kPeerQueued(&attrs, queued, 2, std::vector<bool>{true});

  auto selected = ed2k::selectConnectPeer(attrs.peerStates, 100, 1);
  CPPUNIT_ASSERT(selected);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), selected->endpoint.host);
  selected->connecting = true;
  selected = ed2k::selectConnectPeer(attrs.peerStates, 100, 1);
  CPPUNIT_ASSERT(!selected);

  auto action = ed2k::selectPeerAction(attrs.peerStates, 100, 1);
  CPPUNIT_ASSERT_EQUAL(ed2k::PeerActionType::REASK, action.type);
  CPPUNIT_ASSERT(action.peer);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.12"), action.peer->endpoint.host);

  selected = ed2k::selectConnectPeer(attrs.peerStates, 100, 2);
  CPPUNIT_ASSERT(selected);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.11"), selected->endpoint.host);
}

void DownloadHelperTest::testEd2kServerSourceCadencePolicy()
{
  Ed2kAttribute attrs;
  attrs.link.size = 100;

  ed2k::ServerState fresh;
  fresh.endpoint.host = "203.0.113.10";
  fresh.endpoint.port = 4661;
  fresh.handshakeCompleted = true;
  fresh.nextSourceRequestTime = 1000;
  fresh.lastSourceResponseTime = 930;
  fresh.lastSourceCount = 2;
  attrs.serverStates.push_back(fresh);

  ed2k::ServerState unknownLargeServer;
  unknownLargeServer.endpoint.host = "203.0.113.9";
  unknownLargeServer.endpoint.port = 4661;
  CPPUNIT_ASSERT(ed2k::serverTcpSourceRequestDue(
      unknownLargeServer,
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1, 950));

  CPPUNIT_ASSERT(!ed2k::serverTcpSourceRequestDue(attrs.serverStates[0],
                                                  attrs.link.size, 950));
  CPPUNIT_ASSERT(!ed2k::serverTcpSourceRequestDue(attrs.serverStates[0],
                                                  attrs.link.size, 1000));

  attrs.serverStates[0].lastSourceCount = 0;
  attrs.serverStates[0].lastSourceResponseTime = 600;
  CPPUNIT_ASSERT(ed2k::serverTcpSourceRequestDue(attrs.serverStates[0],
                                                 attrs.link.size, 1000));

  ed2k::ServerState udp = attrs.serverStates[0];
  udp.endpoint.port = 4665;
  udp.udpFlags = ed2k::SRV_UDPFLG_EXT_GETSOURCES;
  CPPUNIT_ASSERT(ed2k::serverUdpSourceRequestDue(udp, attrs.link.size, 1000));

  udp.lastUdpSourceRequestTime = 990;
  CPPUNIT_ASSERT(!ed2k::serverUdpSourceRequestDue(udp, attrs.link.size, 1000));

  udp.lastUdpSourceRequestTime = 0;
  udp.udpFlags = 0;
  CPPUNIT_ASSERT(!ed2k::serverUdpSourceRequestDue(udp, attrs.link.size, 1000));

  udp.udpFlags = ed2k::SRV_UDPFLG_EXT_GETSOURCES2;
  attrs.link.size =
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1;
  CPPUNIT_ASSERT(!ed2k::serverUdpSourceRequestDue(udp, attrs.link.size, 1000));

  udp.udpFlags |= ed2k::SRV_UDPFLG_LARGEFILES;
  CPPUNIT_ASSERT(ed2k::serverUdpSourceRequestDue(udp, attrs.link.size, 1000));

  ed2k::ServerState failed = udp;
  failed.nextRetryTime = 1200;
  CPPUNIT_ASSERT(!ed2k::serverUdpSourceRequestDue(failed, attrs.link.size,
                                                  1000));
}

void DownloadHelperTest::testEd2kServerSearchCadencePolicy()
{
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->searchActive = true;
  ed2k::Endpoint server;
  server.host = "203.0.113.10";
  server.port = 4661;
  attrs->servers.push_back(server);
  attrs->serverStates.push_back(ed2k::ServerState());
  attrs->serverStates[0].endpoint = server;
  attrs->serverStates[0].handshakeCompleted = true;
  attrs->serverStates[0].nextSourceRequestTime = 0;
  attrs->serverStates[0].lastSourceResponseTime = 1000;
  attrs->serverStates[0].lastSourceCount = 2;

  auto option = std::make_shared<Option>(*option_);
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, 0, "/tmp/aria2-next-ed2k-search-test");
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  group->setDownloadContext(dctx);
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());

  std::vector<std::unique_ptr<Command>> commands;
  schedulePendingEd2kServers(commands, group.get(), &engine);

  CPPUNIT_ASSERT_EQUAL((size_t)1, commands.size());
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

void DownloadHelperTest::testEd2kPeerTransferRemovesCompletedRequestedRanges()
{
  Ed2kAttribute attrs;
  ed2k::Endpoint peer;
  peer.host = "203.0.113.10";
  peer.port = 4662;
  addEd2kPeer(&attrs, peer, ed2k::PEER_SOURCE_SERVER);

  std::vector<ed2k::PartRange> ranges;
  ed2k::PartRange range;
  range.begin = 0;
  range.end = 10;
  ranges.push_back(range);
  range.begin = 10;
  range.end = 20;
  ranges.push_back(range);
  CPPUNIT_ASSERT(updateEd2kPeerRequestedParts(&attrs, peer, ranges, 100));

  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       removeEd2kPeerCompletedRequestedRange(&attrs, peer,
                                                             0, 10, 120));
  auto state = getEd2kPeerState(&attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT_EQUAL((size_t)1, state->requestedParts.size());
  CPPUNIT_ASSERT_EQUAL((int64_t)10, state->requestedParts[0].begin);
  CPPUNIT_ASSERT_EQUAL((int64_t)20, state->requestedParts[0].end);
  CPPUNIT_ASSERT_EQUAL((int64_t)120, state->lastTransferProgressTime);

  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       removeEd2kPeerCompletedRequestedRange(&attrs, peer,
                                                             10, 20, 125));
  CPPUNIT_ASSERT(state->requestedParts.empty());
  CPPUNIT_ASSERT_EQUAL((int64_t)125, state->lastTransferProgressTime);
}

void DownloadHelperTest::testEd2kPeerTransferExpiresStalledRequests()
{
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(ed2k::PIECE_LENGTH) * 2,
      "aria2-next-ed2k.bin");
  auto attrs = std::make_shared<Ed2kAttribute>();
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option_.get());
  auto segmentMan = std::make_shared<SegmentMan>(dctx, pieceStorage);
  auto segment = segmentMan->getSegmentWithIndex(7, 0);
  CPPUNIT_ASSERT(segment);

  ed2k::Endpoint peer;
  peer.host = "203.0.113.10";
  peer.port = 4662;
  addEd2kPeer(attrs.get(), peer, ed2k::PEER_SOURCE_SERVER);
  std::vector<ed2k::PartRange> ranges;
  ed2k::PartRange range;
  range.begin = 0;
  range.end = 10;
  ranges.push_back(range);
  CPPUNIT_ASSERT(updateEd2kPeerRequestedParts(attrs.get(), peer, ranges, 100));
  auto state = getEd2kPeerState(attrs.get(), peer);
  state->accepted = true;

  CPPUNIT_ASSERT(!expireEd2kStalledPeerTransfer(
      attrs.get(), segmentMan.get(), peer, 7, 129, 30, 15));
  CPPUNIT_ASSERT_EQUAL((size_t)1, state->requestedParts.size());

  CPPUNIT_ASSERT(expireEd2kStalledPeerTransfer(
      attrs.get(), segmentMan.get(), peer, 7, 130, 30, 15));
  CPPUNIT_ASSERT(state->requestedParts.empty());
  CPPUNIT_ASSERT(state->queued);
  CPPUNIT_ASSERT(!state->accepted);
  CPPUNIT_ASSERT(state->dead);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT_EQUAL((int64_t)145, state->nextRetryTime);

  std::vector<std::shared_ptr<Segment>> inFlight;
  segmentMan->getInFlightSegment(inFlight, 7);
  CPPUNIT_ASSERT(inFlight.empty());
}

void DownloadHelperTest::testEd2kPeerTransferReclaimsStalledEndgameRange()
{
  Ed2kAttribute attrs;
  attrs.link.size = static_cast<int64_t>(ed2k::PIECE_LENGTH) * 2;

  ed2k::Endpoint slowPeer;
  slowPeer.host = "203.0.113.10";
  slowPeer.port = 4662;
  ed2k::Endpoint fastPeer;
  fastPeer.host = "203.0.113.11";
  fastPeer.port = 4662;

  addEd2kPeer(&attrs, slowPeer, ed2k::PEER_SOURCE_SERVER);
  addEd2kPeer(&attrs, fastPeer, ed2k::PEER_SOURCE_SERVER);

  std::vector<ed2k::PartRange> ranges;
  ed2k::PartRange range;
  range.begin = 0;
  range.end = Piece::BLOCK_LENGTH;
  ranges.push_back(range);
  CPPUNIT_ASSERT(updateEd2kPeerRequestedParts(&attrs, slowPeer, ranges, 100));

  auto slowState = getEd2kPeerState(&attrs, slowPeer);
  auto fastState = getEd2kPeerState(&attrs, fastPeer);
  CPPUNIT_ASSERT(slowState);
  CPPUNIT_ASSERT(fastState);
  slowState->accepted = true;
  fastState->accepted = true;
  fastState->partStatus.push_back(true);
  fastState->partStatus.push_back(false);

  ed2k::PartRange reclaimed;
  CPPUNIT_ASSERT(!reclaimEd2kStalledRequestedRange(
      &attrs, fastPeer, fastState->partStatus, 109, 10, reclaimed));
  CPPUNIT_ASSERT_EQUAL((size_t)1, slowState->requestedParts.size());

  CPPUNIT_ASSERT(reclaimEd2kStalledRequestedRange(
      &attrs, fastPeer, fastState->partStatus, 110, 10, reclaimed));
  CPPUNIT_ASSERT_EQUAL((int64_t)0, reclaimed.begin);
  CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(Piece::BLOCK_LENGTH),
                       reclaimed.end);
  CPPUNIT_ASSERT(slowState->requestedParts.empty());
  CPPUNIT_ASSERT(slowState->cancelTransferSent);
  CPPUNIT_ASSERT(attrs.requestedPartRanges.empty());

  CPPUNIT_ASSERT(updateEd2kPeerRequestedParts(&attrs, fastPeer,
                                              std::vector<ed2k::PartRange>{
                                                  reclaimed},
                                              110));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.requestedPartRanges.size());
  CPPUNIT_ASSERT_EQUAL((int64_t)0, attrs.requestedPartRanges[0].begin);
}

void DownloadHelperTest::testEd2kPeerTransferIgnoresDuplicateData()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-transfer-duplicate";
  const std::string outfile = outdir + "/aria2 next duplicate transfer.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string data = "verified ed2k data";
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(data.size()), outfile);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.hash = ed2k::md4Digest(data);
  attrs->pieceHashes.push_back(attrs->link.hash);
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option_.get());
  pieceStorage->initStorage();
  pieceStorage->getDiskAdaptor()->openFile();
  auto segmentMan = std::make_shared<SegmentMan>(dctx, pieceStorage);
  CPPUNIT_ASSERT(segmentMan->getSegmentWithIndex(1, 0));

  ed2k::PeerTransfer transfer(dctx.get(), pieceStorage.get(),
                              segmentMan.get(), 1);
  CPPUNIT_ASSERT(!transfer.writePartData(0, data.substr(0, 8)));
  CPPUNIT_ASSERT(!transfer.writePartData(0, data.substr(0, 8)));
  auto completed = transfer.writePartData(0, data);
  CPPUNIT_ASSERT(completed);
  CPPUNIT_ASSERT(transfer.completeVerifiedSegment(completed));
  CPPUNIT_ASSERT(!transfer.writePartData(0, data));
}

void DownloadHelperTest::testEd2kPeerTransferAcceptsParallelPieceBlocks()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-transfer-parallel";
  const std::string outfile = outdir + "/aria2 next parallel transfer.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string first(Piece::BLOCK_LENGTH, 'a');
  const std::string second(Piece::BLOCK_LENGTH, 'b');
  const auto data = first + second;
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(data.size()), outfile);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.hash = ed2k::md4Digest(data);
  attrs->pieceHashes.push_back(attrs->link.hash);
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option_.get());
  pieceStorage->initStorage();
  pieceStorage->getDiskAdaptor()->openFile();
  auto segmentMan = std::make_shared<SegmentMan>(dctx, pieceStorage);

  CPPUNIT_ASSERT(segmentMan->getSegmentWithIndex(1, 0));
  ed2k::PeerTransfer firstPeer(dctx.get(), pieceStorage.get(),
                               segmentMan.get(), 1);
  CPPUNIT_ASSERT(!firstPeer.writePartData(0, first));

  ed2k::PeerTransfer secondPeer(dctx.get(), pieceStorage.get(),
                                segmentMan.get(), 2);
  auto completed = secondPeer.writePartData(first.size(), second);
  CPPUNIT_ASSERT(completed);
  CPPUNIT_ASSERT(secondPeer.completeVerifiedSegment(completed));
  CPPUNIT_ASSERT(pieceStorage->hasPiece(0));
}

void DownloadHelperTest::testEd2kPeerTransferCancelsOwnerAfterParallelHashFailure()
{
  const std::string outdir = A2_TEST_OUT_DIR "/ed2k-transfer-parallel-bad";
  const std::string outfile = outdir + "/aria2 next parallel bad transfer.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();
  File(outdir).mkdirs();

  const std::string first(Piece::BLOCK_LENGTH, 'a');
  const std::string second(Piece::BLOCK_LENGTH, 'b');
  const auto data = first + second;
  std::string corruptSecond = second;
  corruptSecond[0] = 'x';
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(data.size()), outfile);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.hash = ed2k::md4Digest(data);
  attrs->pieceHashes.push_back(attrs->link.hash);
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  option_->put(PREF_FILE_ALLOCATION, V_NONE);
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option_.get());
  pieceStorage->initStorage();
  pieceStorage->getDiskAdaptor()->openFile();
  auto segmentMan = std::make_shared<SegmentMan>(dctx, pieceStorage);

  CPPUNIT_ASSERT(segmentMan->getSegmentWithIndex(1, 0));
  ed2k::PeerTransfer firstPeer(dctx.get(), pieceStorage.get(),
                               segmentMan.get(), 1);
  CPPUNIT_ASSERT(!firstPeer.writePartData(0, first));

  ed2k::PeerTransfer secondPeer(dctx.get(), pieceStorage.get(),
                                segmentMan.get(), 2);
  CPPUNIT_ASSERT_THROW(secondPeer.writePartData(first.size(), corruptSecond),
                       DlRetryEx);
  CPPUNIT_ASSERT(!pieceStorage->isPieceUsed(0));
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
  idChange.tcpObfuscationPort = 4666;
  updateEd2kServerIdChange(&attrs, server, idChange);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x0a000001, state->clientId);
  CPPUNIT_ASSERT(state->highId);
  CPPUNIT_ASSERT(state->handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL(std::string("1.0.0.10"), state->ipAddress);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, state->tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, state->tcpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, state->failCount);

  ed2k::ServerStatus status;
  status.users = 1234;
  status.files = 5678;
  updateEd2kServerStatus(&attrs, server, status);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, state->users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, state->files);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, state->tcpObfuscationPort);

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
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, state->udpStatusChallenge);
  CPPUNIT_ASSERT_EQUAL((int64_t)120, state->lastUdpStatusTime);

  updateEd2kServerMessage(&attrs, server, "hello");
  CPPUNIT_ASSERT_EQUAL(std::string("hello"), state->lastMessage);

  ed2k::ServerIdent ident;
  ident.name = "server name";
  ident.description = "server description";
  updateEd2kServerIdent(&attrs, server, ident);
  CPPUNIT_ASSERT_EQUAL(std::string("server name"), state->name);
  CPPUNIT_ASSERT_EQUAL(std::string("server description"), state->description);

  updateEd2kServerSourceRequestTime(&attrs, server, 90);
  CPPUNIT_ASSERT(state->connected);
  CPPUNIT_ASSERT_EQUAL((int64_t)90, state->nextSourceRequestTime);
  markEd2kServerSourceRequestFinished(&attrs, server);
  CPPUNIT_ASSERT(!state->connected);
  CPPUNIT_ASSERT(!state->connecting);

  updateEd2kServerFailure(&attrs, server, 100, 30);
  CPPUNIT_ASSERT(!state->connected);
  CPPUNIT_ASSERT(!state->handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, state->failCount);
  CPPUNIT_ASSERT_EQUAL((int64_t)100, state->lastFailureTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)130, state->nextRetryTime);
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

void DownloadHelperTest::testEd2kSearchResultMergesNetworks()
{
  Ed2kAttribute attrs;
  ed2k::SearchResultEntry server;
  server.hash = std::string(ed2k::HASH_LENGTH, '\x31');
  server.name = "movie.mkv";
  server.size = 42;
  server.sourceCount = 3;
  server.completeSourceCount = 1;
  server.sourceNetwork = "server";
  server.ed2kLink = "ed2k://|file|movie.mkv|42|31313131313131313131313131313131|/";

  ed2k::SearchResultEntry kad = server;
  kad.sourceCount = 7;
  kad.completeSourceCount = 4;
  kad.sourceNetwork = "kad";

  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       addEd2kSearchResults(&attrs, {server}, false));
  CPPUNIT_ASSERT_EQUAL((size_t)0,
                       addEd2kSearchResults(&attrs, {kad}, false));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.searchResults.size());
  CPPUNIT_ASSERT_EQUAL((uint32_t)7, attrs.searchResults[0].sourceCount);
  CPPUNIT_ASSERT_EQUAL((uint32_t)4,
                       attrs.searchResults[0].completeSourceCount);
  CPPUNIT_ASSERT_EQUAL(std::string("server|kad"),
                       attrs.searchResults[0].sourceNetwork);
}

void DownloadHelperTest::testEd2kSearchResultAppliesLocalFilters()
{
  Ed2kAttribute attrs;
  attrs.searchQuery.minSize = 100;
  attrs.searchQuery.maxSize = 200;
  attrs.searchQuery.minSourceCount = 3;
  attrs.searchQuery.minCompleteSourceCount = 2;

  ed2k::SearchResultEntry rejected;
  rejected.hash = std::string(ed2k::HASH_LENGTH, '\x31');
  rejected.name = "small.iso";
  rejected.size = 50;
  rejected.sourceCount = 10;
  rejected.completeSourceCount = 5;
  rejected.sourceNetwork = "kad";
  rejected.ed2kLink =
      "ed2k://|file|small.iso|50|31313131313131313131313131313131|/";

  ed2k::SearchResultEntry accepted = rejected;
  accepted.hash = std::string(ed2k::HASH_LENGTH, '\x32');
  accepted.name = "movie.iso";
  accepted.size = 150;
  accepted.sourceCount = 3;
  accepted.completeSourceCount = 2;
  accepted.ed2kLink =
      "ed2k://|file|movie.iso|150|32323232323232323232323232323232|/";

  CPPUNIT_ASSERT_EQUAL((size_t)1, addEd2kSearchResults(
                                     &attrs, {rejected, accepted}, false));
  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs.searchResults.size());
  CPPUNIT_ASSERT_EQUAL(accepted.hash, attrs.searchResults[0].hash);
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
void DownloadHelperTest::testCreateRequestGroupForUri_LibtorrentTorrent()
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
    std::shared_ptr<DownloadContext> btctx = torrentGroup->getDownloadContext();
    CPPUNIT_ASSERT(btctx->hasAttribute(CTX_ATTR_LIBTORRENT));
    CPPUNIT_ASSERT(!btctx->hasAttribute(CTX_ATTR_BT));
    CPPUNIT_ASSERT_EQUAL(std::string(A2_TEST_DIR "/test.torrent"),
                         torrentGroup->getMetadataInfo()->getUri());
  }
}

void DownloadHelperTest::testCreateRequestGroupForUri_LibtorrentMagnet()
{
  std::vector<std::string> uris{
      "magnet:?xt=urn:btih:248D0A1CD08284299DE78D5C1ED359BB46717D8C&dn=aria2-test"};
  option_->put(PREF_DIR, "/tmp");
  {
    std::vector<std::shared_ptr<RequestGroup>> result;

    createRequestGroupForUri(result, option_, uris);

    CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
    auto group = result[0];
    auto dctx = group->getDownloadContext();
    CPPUNIT_ASSERT(dctx->hasAttribute(CTX_ATTR_LIBTORRENT));
    CPPUNIT_ASSERT(!dctx->hasAttribute(CTX_ATTR_BT));
    CPPUNIT_ASSERT_EQUAL(uris[0], group->getMetadataInfo()->getUri());
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
