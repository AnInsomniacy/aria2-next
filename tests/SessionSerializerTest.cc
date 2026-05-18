#include "SessionSerializer.h"

#include <iostream>
#include <fstream>

#include <cppunit/extensions/HelperMacros.h>

#include "TestUtil.h"
#include "RequestGroupMan.h"
#include "array_fun.h"
#include "download_helper.h"
#include "prefs.h"
#include "Option.h"
#include "a2functional.h"
#include "FileEntry.h"
#include "SelectEventPoll.h"
#include "DownloadEngine.h"
#include "Ed2kAttribute.h"
#include "Ed2kKadState.h"
#include "util.h"

namespace aria2 {

class SessionSerializerTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SessionSerializerTest);
  CPPUNIT_TEST(testSave);
  CPPUNIT_TEST(testSaveErrorDownload);
  CPPUNIT_TEST(testSaveEd2kDownload);
  CPPUNIT_TEST_SUITE_END();

public:
  void testSave();
  void testSaveErrorDownload();
  void testSaveEd2kDownload();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SessionSerializerTest);

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
  CPPUNIT_ASSERT_EQUAL((size_t)5, result.size());
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
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgman.getRequestGroups().size());

  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSave";
  s.save(filename);
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      std::string("http://error\thttp://error3\thttp://error2\t"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(fmt(" gid=%s", drs[1]->gid->toHex().c_str()), line);
  std::getline(ss, line);
  // finished and force-save option
  CPPUNIT_ASSERT_EQUAL(std::string("http://force-save\t"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(fmt(" gid=%s", drs[3]->gid->toHex().c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" force-save=true"), line);
  // Check active download is also saved
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[0] + "\t" + uris[1] + "\t", line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(result[0]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[2], line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(result[1]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[3], line);
  std::getline(ss, line);
  // local metalink download does not save meaningful GID
  CPPUNIT_ASSERT(fmt(" gid=%s", GroupId::toHex(result[2]->getGID()).c_str()) !=
                 line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(uris[4], line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(result[4]->getGID()).c_str()), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" pause=true"), line);
  std::getline(ss, line);
  CPPUNIT_ASSERT(!ss);
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
  CPPUNIT_ASSERT(s.save(filename));
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string("http://error\t"), line);
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
  CPPUNIT_ASSERT_EQUAL((size_t)1, result.size());
  auto attrs = getEd2kAttrs(result[0]->getDownloadContext());
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
  CPPUNIT_ASSERT(serializer.save(filename));

  std::ifstream in(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(in, line);
  CPPUNIT_ASSERT(util::startsWith(line, "ed2k://|file|aria2%20next.bin|"));
  CPPUNIT_ASSERT(line.find("0123456789abcdef0123456789abcdef") !=
                 std::string::npos);
  CPPUNIT_ASSERT(line.find("33333333333333333333333333333333:"
                           "44444444444444444444444444444444") !=
                 std::string::npos);
  CPPUNIT_ASSERT(line.find("sources,203.0.113.20:4662") !=
                 std::string::npos);
  std::getline(in, line);
  CPPUNIT_ASSERT_EQUAL(
      fmt(" gid=%s", GroupId::toHex(result[0]->getGID()).c_str()), line);
  std::getline(in, line);
  CPPUNIT_ASSERT(util::startsWith(line, " ed2k-kad-routing-state="));
  std::getline(in, line);
  CPPUNIT_ASSERT(util::startsWith(line, " ed2k-server-state="));
  ed2k::ServerState restored;
  auto value = line.substr(std::string(" ed2k-server-state=").size());
  CPPUNIT_ASSERT(ed2k::parseServerStatePayload(
      restored, util::fromHex(value.begin(), value.end())));
  CPPUNIT_ASSERT_EQUAL(std::string("Peer Server"), restored.name);
  CPPUNIT_ASSERT_EQUAL(std::string("Primary ED2K server"),
                       restored.description);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, restored.tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, restored.users);
  std::getline(in, line);
  CPPUNIT_ASSERT_EQUAL(std::string(" dir=/tmp"), line);
  std::getline(in, line);
  CPPUNIT_ASSERT(!in);
}

} // namespace aria2
