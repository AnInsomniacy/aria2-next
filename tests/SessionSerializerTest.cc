#include "SessionSerializer.h"

#include <iostream>
#include <fstream>

#include "a2doctest.h"

#include "TestUtil.h"
#include "RequestGroupMan.h"
#include "array_fun.h"
#include "download_helper.h"
#include "UriListParser.h"
#include "DefaultPieceStorage.h"
#include "prefs.h"
#include "Option.h"
#include "a2functional.h"
#include "FileEntry.h"
#include "SelectEventPoll.h"
#include "DownloadEngine.h"
#include "Ed2kAttribute.h"
#include "Ed2kKadState.h"
#include "Ed2kUploadQueue.h"
#include "util.h"

namespace aria2 {

class SessionSerializerTest {


public:
  void testSave();
  void testSaveErrorDownload();
  void testSaveEd2kDownload();
  void testLoadSavedEd2kState();
  void testSaveActiveEd2kSharing();
  void testSaveEd2kPeerCredits();
};

A2_TEST(SessionSerializerTest, testSave)
A2_TEST(SessionSerializerTest, testSaveErrorDownload)
A2_TEST(SessionSerializerTest, testSaveEd2kDownload)
A2_TEST(SessionSerializerTest, testLoadSavedEd2kState)
A2_TEST(SessionSerializerTest, testSaveActiveEd2kSharing)
A2_TEST(SessionSerializerTest, testSaveEd2kPeerCredits)

void SessionSerializerTest::testSave()
{
#if defined(ENABLE_BITTORRENT) && defined(ENABLE_METALINK)
  std::vector<std::string> uris{
      "http://localhost/file", "http://mirror/file",
      A2_TEST_DIR "/test.torrent", A2_TEST_DIR "/serialize_session.meta4",
      "magnet:?xt=urn:btih:248D0A1CD08284299DE78D5C1ED359BB46717D8C"};
  std::vector<std::shared_ptr<RequestGroup>> result;
  std::shared_ptr<Option> option(new Option());
  option->put(PREF_DIR, "/tmp");
  createRequestGroupForUri(result, option, uris);
  REQUIRE_EQ((size_t)5, result.size());
  result[4]->getOption()->put(PREF_PAUSE, A2_V_TRUE);
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{result, 1, option.get()};
  SessionSerializer s(&rgman);
  std::shared_ptr<DownloadResult> drs[] = {
      // REMOVED downloads will not be saved.
      createDownloadResult(error_code::REMOVED, "http://removed"),
      createDownloadResult(error_code::TIME_OUT, "http://error"),
      createDownloadResult(error_code::FINISHED, "http://finished"),
      createDownloadResult(error_code::FINISHED, "http://force-save")};
  // This URI will be discarded because same URI exists in remaining
  // URIs.
  drs[1]->fileEntries[0]->getRemainingUris().push_back("http://error");
  drs[1]->fileEntries[0]->getRemainingUris().push_back("http://error3");
  // This URI will be discarded because same URI exists in remaining
  // URIs.
  drs[1]->fileEntries[0]->getRemainingUris().push_back("http://error");
  //
  // This URI will be discarded because same URI exists in remaining
  // URIs.
  drs[1]->fileEntries[0]->getSpentUris().push_back("http://error");
  drs[1]->fileEntries[0]->getSpentUris().push_back("http://error2");
  // This URI will be discarded because same URI exists in remaining
  // URIs.
  drs[1]->fileEntries[0]->getSpentUris().push_back("http://error");

  drs[3]->option->put(PREF_FORCE_SAVE, A2_V_TRUE);
  for (size_t i = 0; i < sizeof(drs) / sizeof(drs[0]); ++i) {
    rgman.addDownloadResult(drs[i]);
  }

  DownloadEngine e(make_unique<SelectEventPoll>());
  e.setOption(option.get());
  rgman.fillRequestGroupFromReserver(&e);
  REQUIRE_EQ((size_t)1, rgman.getRequestGroups().size());

  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSave";
  s.save(filename);
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(ss, line);
  REQUIRE_EQ(
      std::string("http://error\thttp://error3\thttp://error2\t"), line);
  std::getline(ss, line);
  REQUIRE_EQ(fmt(" gid=%s", drs[1]->gid->toHex().c_str()), line);
  std::getline(ss, line);
  // finished and force-save option
  REQUIRE_EQ(std::string("http://force-save\t"), line);
  std::getline(ss, line);
  REQUIRE_EQ(fmt(" gid=%s", drs[3]->gid->toHex().c_str()), line);
  std::getline(ss, line);
  REQUIRE_EQ(std::string(" force-save=true"), line);
  // Check active download is also saved
  std::getline(ss, line);
  REQUIRE_EQ(uris[0] + "\t" + uris[1] + "\t", line);
  std::getline(ss, line);
  REQUIRE_EQ(
      fmt(" gid=%s", GroupId::toHex(result[0]->getGID()).c_str()), line);
  std::getline(ss, line);
  REQUIRE_EQ(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  REQUIRE_EQ(uris[2], line);
  std::getline(ss, line);
  REQUIRE_EQ(
      fmt(" gid=%s", GroupId::toHex(result[1]->getGID()).c_str()), line);
  std::getline(ss, line);
  REQUIRE_EQ(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  REQUIRE_EQ(uris[3], line);
  std::getline(ss, line);
  // local metalink download does not save meaningful GID
  REQUIRE(fmt(" gid=%s", GroupId::toHex(result[2]->getGID()).c_str()) !=
                 line);
  std::getline(ss, line);
  REQUIRE_EQ(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  REQUIRE_EQ(uris[4], line);
  std::getline(ss, line);
  REQUIRE_EQ(
      fmt(" gid=%s", GroupId::toHex(result[4]->getGID()).c_str()), line);
  std::getline(ss, line);
  REQUIRE_EQ(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  REQUIRE_EQ(std::string(" pause=true"), line);
  std::getline(ss, line);
  REQUIRE(!ss);
#endif // defined(ENABLE_BITTORRENT) && defined(ENABLE_METALINK)
}

void SessionSerializerTest::testSaveErrorDownload()
{
  std::shared_ptr<DownloadResult> dr =
      createDownloadResult(error_code::TIME_OUT, "http://error");
  dr->fileEntries[0]->getSpentUris().swap(
      dr->fileEntries[0]->getRemainingUris());
  std::shared_ptr<Option> option(new Option());
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{std::vector<std::shared_ptr<RequestGroup>>(), 1,
                        option.get()};
  rgman.addDownloadResult(dr);
  SessionSerializer s(&rgman);
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveErrorDownload";
  REQUIRE(s.save(filename));
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(ss, line);
  REQUIRE_EQ(std::string("http://error\t"), line);
}

void SessionSerializerTest::testSaveEd2kDownload()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|"
      "p=11111111111111111111111111111111:"
      "22222222222222222222222222222222|/"};
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, "/tmp");
  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option, uris);
  REQUIRE_EQ((size_t)1, result.size());
  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
  std::string clientHashHex("01020304050e0708090a0b0c0d0e6f10");
  attrs->clientHash =
      util::fromHex(clientHashHex.begin(), clientHashHex.end());
  std::string firstPieceHash("33333333333333333333333333333333");
  std::string secondPieceHash("44444444444444444444444444444444");
  attrs->pieceHashes = {
      util::fromHex(firstPieceHash.begin(), firstPieceHash.end()),
      util::fromHex(secondPieceHash.begin(), secondPieceHash.end())};
  ed2k::Endpoint learnedPeer;
  learnedPeer.host = "203.0.113.20";
  learnedPeer.port = 4662;
  attrs->peers.push_back(learnedPeer);
  attrs->kadRoutingTable =
      std::make_shared<ed2k::KadRoutingTable>(attrs->link.hash);
  attrs->lastKadFirewalledCheck = 500;
  attrs->lastKadSourcePublish = 600;
  attrs->lastKadSourceSearch = 700;
  attrs->kadSourceSearchCount = 3;
  attrs->kadFirewalled = false;
  attrs->kadObservedAddresses.push_back("203.0.113.55");
  ed2k::ServerState serverState;
  serverState.endpoint.host = "203.0.113.10";
  serverState.endpoint.port = 4661;
  serverState.name = "Peer Server";
  serverState.description = "Primary ED2K server";
  serverState.handshakeCompleted = true;
  serverState.clientId = 0x04030201;
  serverState.highId = true;
  serverState.ipAddress = "1.2.3.4";
  serverState.tcpFlags = 0x55aa;
  serverState.users = 1234;
  serverState.files = 5678;
  serverState.lastMessage = "hello";
  attrs->serverStates.push_back(serverState);
  ed2k::KadContact contact;
  std::string nodeIdHex("23a8ceff57a7a32d562d649ed7893796");
  contact.id = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  contact.host = "203.0.113.8";
  contact.udpPort = 4672;
  contact.tcpPort = 4662;
  contact.version = 8;
  attrs->kadRoutingTable->nodeSeen(contact, 100);

  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{result, 1, option.get()};
  SessionSerializer serializer(&rgman);
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveEd2kDownload";
  REQUIRE(serializer.save(filename));

  std::ifstream in(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(in, line);
  REQUIRE(util::startsWith(line, "ed2k://|file|aria2%20next.bin|"));
  REQUIRE(line.find("0123456789abcdef0123456789abcdef") !=
                 std::string::npos);
  REQUIRE(line.find("33333333333333333333333333333333:"
                           "44444444444444444444444444444444") !=
                 std::string::npos);
  REQUIRE(line.find("sources,203.0.113.20:4662") !=
                 std::string::npos);
  std::getline(in, line);
  REQUIRE_EQ(
      fmt(" gid=%s", GroupId::toHex(result[0]->getGID()).c_str()), line);
  std::getline(in, line);
  REQUIRE_EQ(
      std::string(" ed2k-client-hash=01020304050e0708090a0b0c0d0e6f10"),
      line);
  std::getline(in, line);
  REQUIRE(util::startsWith(line, " ed2k-kad-routing-state="));
  ed2k::KadRoutingSnapshot restoredKad;
  auto kadValue = line.substr(
      std::string(" ed2k-kad-routing-state=").size());
  REQUIRE(ed2k::parseKadRoutingStatePayload(
      restoredKad, util::fromHex(kadValue.begin(), kadValue.end())));
  REQUIRE_EQ((int64_t)500, restoredKad.lastFirewalledCheck);
  REQUIRE_EQ((int64_t)600, restoredKad.lastSourcePublish);
  REQUIRE_EQ((int64_t)700, restoredKad.lastSourceSearch);
  REQUIRE_EQ((uint32_t)3, restoredKad.sourceSearchCount);
  REQUIRE(!restoredKad.firewalled);
  REQUIRE_EQ((size_t)1, restoredKad.observedAddresses.size());
  REQUIRE_EQ(std::string("203.0.113.55"),
                       restoredKad.observedAddresses[0]);
  std::getline(in, line);
  REQUIRE(util::startsWith(line, " ed2k-server-state="));
  ed2k::ServerState restored;
  auto value = line.substr(std::string(" ed2k-server-state=").size());
  REQUIRE(ed2k::parseServerStatePayload(
      restored, util::fromHex(value.begin(), value.end())));
  REQUIRE_EQ(std::string("Peer Server"), restored.name);
  REQUIRE_EQ(std::string("Primary ED2K server"),
                       restored.description);
  REQUIRE_EQ((uint32_t)0x55aa, restored.tcpFlags);
  REQUIRE_EQ((uint32_t)1234, restored.users);
  std::getline(in, line);
  REQUIRE_EQ(std::string(" dir=/tmp"), line);
  std::getline(in, line);
  REQUIRE(!in);
}

void SessionSerializerTest::testLoadSavedEd2kState()
{
  const std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testLoadSavedEd2kState";
  {
    std::ofstream out(filename.c_str(), std::ios::binary);
    out << "ed2k://|file|aria2%20next.bin|1|"
           "0102030405060708090a0b0c0d0e0f10|/\n"
        << " ed2k-client-hash=1112131415161718191a1b1c1d1e1f20\n"
        << " ed2k-server-state=41324544324b535256040000000b0034352e38372e34312e3136761809006564326b2d727573740b00746573742073657276657200000000000000000000000000a806000020cb0a0050c300003f420f0040420f00fb1700006e05000084187618a868c39e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\n";
  }

  UriListParser parser(filename);
  std::vector<std::string> uris;
  Option option;
  parser.parseNext(uris, option);

  REQUIRE_EQ((size_t)1, uris.size());
  REQUIRE_EQ(
      std::string("1112131415161718191a1b1c1d1e1f20"),
      option.get(PREF_ED2K_CLIENT_HASH));
  REQUIRE(option.defined(PREF_ED2K_SERVER_STATE));
}

void SessionSerializerTest::testSaveActiveEd2kSharing()
{
  std::vector<std::string> uris{
      "ed2k://|file|aria2%20sharing.bin|9728001|"
      "0123456789abcdef0123456789abcdef|/"};
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, "/tmp");
  option->put(PREF_FORCE_SAVE, A2_V_FALSE);
  option->put(PREF_DETACH_SHARE_ONLY, A2_V_TRUE);
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  std::vector<std::shared_ptr<RequestGroup>> result;
  createRequestGroupForUri(result, option, uris);
  REQUIRE_EQ((size_t)1, result.size());

  RequestGroupMan rgman{std::vector<std::shared_ptr<RequestGroup>>(), 1,
                        option.get()};
  auto group = result[0];
  group->initPieceStorage();
  group->getPieceStorage()->markAllPiecesDone();
  group->setRequestGroupMan(&rgman);
  rgman.addRequestGroup(group);
  group->enableSeedOnly();
  group->setPauseRequested(true);

  SessionSerializer serializer(&rgman);
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveActiveEd2kSharing";
  REQUIRE(serializer.save(filename));

  std::ifstream in(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(in, line);
  REQUIRE(util::startsWith(line, "ed2k://|file|aria2%20sharing.bin|"));
  std::getline(in, line);
  REQUIRE_EQ(
      fmt(" gid=%s", GroupId::toHex(group->getGID()).c_str()), line);
  std::getline(in, line);
  REQUIRE_EQ(std::string(" pause=true"), line);
}

void SessionSerializerTest::testSaveEd2kPeerCredits()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  option->put(PREF_ED2K_UPLOAD_SLOTS, "3");
  RequestGroupMan rgman{std::vector<std::shared_ptr<RequestGroup>>(), 1,
                        option.get()};
  auto queue = rgman.getEd2kUploadQueue();
  const std::string userHash(ed2k::HASH_LENGTH, '\x44');
  queue->credits().addUploaded(userHash, 1234);
  queue->credits().addDownloaded(userHash, 5678);

  SessionSerializer serializer(&rgman);
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveEd2kPeerCredits";
  REQUIRE(serializer.save(filename));

  std::ifstream in(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(in, line);
  REQUIRE(util::startsWith(line, " ed2k-peer-credit-state="));
  auto value = line.substr(std::string(" ed2k-peer-credit-state=").size());
  ed2k::PeerCreditState restored;
  REQUIRE(ed2k::parsePeerCreditStatePayload(
      restored, util::fromHex(value.begin(), value.end())));
  REQUIRE_EQ(userHash, restored.userHash);
  REQUIRE_EQ((uint64_t)1234, restored.uploaded);
  REQUIRE_EQ((uint64_t)5678, restored.downloaded);
}

} // namespace aria2
