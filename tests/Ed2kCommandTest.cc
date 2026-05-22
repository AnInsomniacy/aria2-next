#include "Ed2kCommand.h"

#include <array>
#include <cppunit/extensions/HelperMacros.h>
#include <memory>
#include <vector>

#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "Ed2kAttribute.h"
#include "Ed2kKadCommand.h"
#include "Ed2kListenCommand.h"
#include "DefaultBtProgressInfoFile.h"
#include "DefaultPieceStorage.h"
#include "DiskAdaptor.h"
#include "DownloadResult.h"
#include "FileEntry.h"
#include "File.h"
#include "Option.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "SelectEventPoll.h"
#include "SocketCore.h"
#include "ed2k_constants.h"
#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_kad_search.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"
#include "ed2k_server.h"
#include "prefs.h"
#include "util.h"

namespace aria2 {

namespace {
constexpr int MAX_ENGINE_TICKS = 200;

std::shared_ptr<Option> createOption()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_CONNECT_TIMEOUT, "1");
  option->put(PREF_RETRY_WAIT, "1");
  option->put(PREF_ED2K_LISTEN_PORT, "0");
  option->put(PREF_ED2K_UPLOAD_SLOTS, "3");
  option->put(PREF_DIR, A2_TEST_OUT_DIR);
  option->put(PREF_OUT, "ed2k-command-test.bin");
  option->put(PREF_MAX_CONCURRENT_DOWNLOADS, "5");
  option->put(PREF_MAX_DOWNLOAD_RESULT, "5");
  option->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT, "0");
  option->put(PREF_MAX_OVERALL_UPLOAD_LIMIT, "0");
  option->put(PREF_BT_MAX_OPEN_FILES, "100");
  option->put(PREF_ENABLE_RPC, A2_V_FALSE);
  return option;
}

std::vector<std::string> createPieceHashes()
{
  return std::vector<std::string>{std::string(16, '\x11'),
                                  std::string(16, '\x22')};
}

std::shared_ptr<DownloadContext> createEd2kContext()
{
  auto dctx = std::make_shared<DownloadContext>();
  const std::shared_ptr<FileEntry> entries[] = {
      std::make_shared<FileEntry>(A2_TEST_OUT_DIR "/ed2k-command-test.bin",
                                  ed2k::PIECE_LENGTH + 1, 0)};
  dctx->setFileEntries(std::begin(entries), std::end(entries));
  dctx->setPieceLength(ed2k::PIECE_LENGTH);

  auto attrs = make_unique<Ed2kAttribute>();
  attrs->link.type = ed2k::LinkType::FILE;
  attrs->link.name = "ed2k-command-test.bin";
  attrs->link.size = ed2k::PIECE_LENGTH + 1;
  attrs->link.hash = ed2k::rootHash(createPieceHashes());
  attrs->clientHash = normalizeEd2kClientHash(std::string(ed2k::HASH_LENGTH,
                                                          '\x42'));
  dctx->setAttribute(CTX_ATTR_ED2K, std::move(attrs));
  return dctx;
}

std::shared_ptr<RequestGroup>
createRequestGroup(const std::shared_ptr<Option>& option,
                   const std::shared_ptr<DownloadContext>& dctx)
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  group->setDownloadContext(dctx);
  group->setNumConcurrentCommand(3);
  return group;
}

std::shared_ptr<SocketCore> acceptPeer(SocketCore& listenSocket,
                                       DownloadEngine& engine)
{
  for (int i = 0; i < MAX_ENGINE_TICKS && !listenSocket.isReadable(0); ++i) {
    engine.run(true);
  }
  CPPUNIT_ASSERT(listenSocket.isReadable(0));
  auto socket = listenSocket.acceptConnection();
  socket->setNonBlockingMode();
  return socket;
}

void readFromSocket(const std::shared_ptr<SocketCore>& socket,
                    DownloadEngine& engine, char* data, size_t length)
{
  size_t readLength = 0;
  for (int i = 0; i < MAX_ENGINE_TICKS && readLength < length; ++i) {
    if (!socket->isReadable(0)) {
      engine.run(true);
      continue;
    }
    size_t len = length - readLength;
    socket->readData(data + readLength, len);
    readLength += len;
  }
  CPPUNIT_ASSERT_EQUAL(length, readLength);
}

std::string readPacket(const std::shared_ptr<SocketCore>& socket,
                       DownloadEngine& engine)
{
  std::array<char, 6> header;
  readFromSocket(socket, engine, header.data(), header.size());
  ed2k::PacketHeader packetHeader;
  CPPUNIT_ASSERT(ed2k::readPacketHeader(packetHeader, header.data(),
                                        header.size()));
  std::string body(packetHeader.payloadSize(), '\0');
  if (!body.empty()) {
    readFromSocket(socket, engine, &body[0], body.size());
  }
  return std::string(header.data(), header.size()) + body;
}

ed2k::PacketHeader packetHeaderOf(const std::string& packet)
{
  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(ed2k::readPacketHeader(header, packet.data(), 6));
  return header;
}

std::string packetBodyOf(const std::string& packet)
{
  return packet.substr(6);
}

void runEngineTicks(DownloadEngine& engine, int ticks)
{
  for (int i = 0; i < ticks; ++i) {
    engine.run(true);
  }
}

struct ReceivedDatagram {
  std::string data;
  Endpoint sender;
};

ReceivedDatagram readDatagramFrom(SocketCore& socket, DownloadEngine& engine)
{
  std::array<char, 64_k> data;
  for (int i = 0; i < MAX_ENGINE_TICKS && !socket.isReadable(0); ++i) {
    engine.run(true);
  }
  CPPUNIT_ASSERT(socket.isReadable(0));
  Endpoint sender;
  auto length = socket.readDataFrom(data.data(), data.size(), sender);
  CPPUNIT_ASSERT(length >= 2);
  ReceivedDatagram result;
  result.data.assign(data.data(), data.data() + length);
  result.sender = sender;
  return result;
}

std::string readDatagram(SocketCore& socket, DownloadEngine& engine)
{
  return readDatagramFrom(socket, engine).data;
}

void writeDatagram(SocketCore& socket, const std::string& datagram,
                   uint16_t port)
{
  CPPUNIT_ASSERT_EQUAL(static_cast<ssize_t>(datagram.size()),
                       socket.writeData(datagram.data(), datagram.size(),
                                        "127.0.0.1", port));
}

ed2k::PacketHeader datagramHeaderOf(const std::string& datagram)
{
  ed2k::PacketHeader header;
  CPPUNIT_ASSERT(
      ed2k::readDatagramHeader(header, datagram.data(), datagram.size()));
  return header;
}

std::string datagramBodyOf(const std::string& datagram)
{
  return datagram.substr(2);
}

std::string decodeKadDatagram(const std::string& datagram,
                              const std::string& contactId)
{
  ed2k::KadObfuscatedDatagram parsed;
  if (ed2k::parseKadObfuscatedDatagram(parsed, datagram, contactId)) {
    return parsed.datagram;
  }
  return datagram;
}

std::string decodeKadDatagramWithKey(const std::string& datagram,
                                     uint32_t udpKey)
{
  ed2k::KadObfuscatedDatagram parsed;
  if (ed2k::parseKadObfuscatedDatagram(parsed, datagram, udpKey)) {
    return parsed.datagram;
  }
  return datagram;
}

ReceivedDatagram readDatagramWithOpcode(SocketCore& socket,
                                        DownloadEngine& engine, uint8_t opcode)
{
  for (int i = 0; i < MAX_ENGINE_TICKS; ++i) {
    auto datagram = readDatagramFrom(socket, engine);
    if (datagramHeaderOf(datagram.data).opcode == opcode) {
      return datagram;
    }
  }
  CPPUNIT_FAIL("Expected ED2K Kad datagram opcode was not received.");
  return ReceivedDatagram();
}

ReceivedDatagram readKadDatagramWithOpcode(SocketCore& socket,
                                           DownloadEngine& engine,
                                           uint8_t opcode,
                                           const std::string& contactId)
{
  for (int i = 0; i < MAX_ENGINE_TICKS; ++i) {
    auto datagram = readDatagramFrom(socket, engine);
    datagram.data = decodeKadDatagram(datagram.data, contactId);
    if (datagramHeaderOf(datagram.data).opcode == opcode) {
      return datagram;
    }
  }
  CPPUNIT_FAIL("Expected ED2K Kad datagram opcode was not received.");
  return ReceivedDatagram();
}

ed2k::KadContact createKadContact(const std::string& id, uint16_t udpPort)
{
  ed2k::KadContact contact;
  contact.id = id;
  contact.host = "127.0.0.1";
  contact.udpPort = udpPort;
  contact.tcpPort = 4662;
  contact.version = 8;
  return contact;
}
} // namespace

class Ed2kCommandTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(Ed2kCommandTest);
  CPPUNIT_TEST(testServerSourceDiscoveryFlow);
  CPPUNIT_TEST(testPeerHandshakeQueuesFileRequestAndQueueRank);
  CPPUNIT_TEST(testPeerCommandFinishesGroupAfterLastPart);
  CPPUNIT_TEST(testPendingConnectDrainsAfterHalt);
  CPPUNIT_TEST(testForceHaltDrainsIdleKadGroup);
  CPPUNIT_TEST(testKadBootstrapSourceSearchAddsPeer);
  CPPUNIT_TEST(testKadDecodesReceiverKeyObfuscatedResponse);
  CPPUNIT_TEST(testKadDecodesSelfIdObfuscatedResponse);
  CPPUNIT_TEST_SUITE_END();

public:
  void testServerSourceDiscoveryFlow();
  void testPeerHandshakeQueuesFileRequestAndQueueRank();
  void testPeerCommandFinishesGroupAfterLastPart();
  void testPendingConnectDrainsAfterHalt();
  void testForceHaltDrainsIdleKadGroup();
  void testKadBootstrapSourceSearchAddsPeer();
  void testKadDecodesReceiverKeyObfuscatedResponse();
  void testKadDecodesSelfIdObfuscatedResponse();
};

CPPUNIT_TEST_SUITE_REGISTRATION(Ed2kCommandTest);

void Ed2kCommandTest::testServerSourceDiscoveryFlow()
{
  auto option = createOption();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto dctx = createEd2kContext();
  auto group = createRequestGroup(option, dctx);
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  auto serverAddr = listenSocket.getAddrInfo();

  ed2k::Endpoint server;
  server.host = "127.0.0.1";
  server.port = serverAddr.port;
  auto attrs = getEd2kAttrs(dctx);
  attrs->servers.push_back(server);

  std::vector<std::unique_ptr<Command>> commands;
  schedulePendingEd2kServers(commands, group.get(), &engine);
  CPPUNIT_ASSERT_EQUAL((size_t)1, commands.size());
  engine.addCommand(std::move(commands));

  runEngineTicks(engine, 1);
  auto serverSocket = acceptPeer(listenSocket, engine);
  runEngineTicks(engine, 1);

  auto login = readPacket(serverSocket, engine);
  auto loginHeader = packetHeaderOf(login);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EDONKEY, loginHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_LOGINREQUEST, loginHeader.opcode);

  serverSocket->writeData(
      ed2k::createPacket(ed2k::PROTO_EDONKEY, ed2k::OP_IDCHANGE,
                         ed2k::packUInt32(0x04030201) +
                             ed2k::packUInt32(ed2k::SRV_TCPFLG_LARGEFILES) +
                             ed2k::packUInt32(0)));
  runEngineTicks(engine, 1);

  auto getSources = readPacket(serverSocket, engine);
  auto getSourcesHeader = packetHeaderOf(getSources);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EDONKEY, getSourcesHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_GETSOURCES, getSourcesHeader.opcode);
  auto getSourcesBody = packetBodyOf(getSources);
  CPPUNIT_ASSERT(getSourcesBody.size() >= ed2k::HASH_LENGTH + 4);
  CPPUNIT_ASSERT_EQUAL(attrs->link.hash,
                       getSourcesBody.substr(0, ed2k::HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(attrs->link.size),
                       ed2k::readUInt32(getSourcesBody.data() +
                                         ed2k::HASH_LENGTH));

  std::vector<ed2k::Endpoint> sources;
  ed2k::Endpoint source;
  source.host = "203.0.113.20";
  source.port = 4662;
  sources.push_back(source);
  serverSocket->writeData(
      ed2k::createPacket(ed2k::PROTO_EDONKEY, ed2k::OP_FOUNDSOURCES,
                         ed2k::createFoundSourcesPayload(attrs->link.hash,
                                                         sources)));
  runEngineTicks(engine, 1);

  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->peers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.20"), attrs->peers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, attrs->peers[0].port);
  auto state = getEd2kServerState(attrs, server);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->handshakeCompleted);
  CPPUNIT_ASSERT(!state->connecting);
  CPPUNIT_ASSERT(!state->connected);
  engine.requestHalt();
}

void Ed2kCommandTest::testPeerHandshakeQueuesFileRequestAndQueueRank()
{
  auto option = createOption();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto dctx = createEd2kContext();
  auto group = createRequestGroup(option, dctx);
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  auto peerAddr = listenSocket.getAddrInfo();

  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = peerAddr.port;
  auto attrs = getEd2kAttrs(dctx);
  addEd2kPeer(attrs, peer, ed2k::PEER_SOURCE_INLINE);

  engine.addCommand(make_unique<Ed2kCommand>(engine.newCUID(), group.get(),
                                             &engine, peer, false));

  runEngineTicks(engine, 1);
  auto peerSocket = acceptPeer(listenSocket, engine);
  runEngineTicks(engine, 1);

  auto hello = readPacket(peerSocket, engine);
  auto helloHeader = packetHeaderOf(hello);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EDONKEY, helloHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_HELLO, helloHeader.opcode);

  auto remoteInfo = ed2k::createLocalEmulePeerInfo();
  auto helloAnswerPayload = ed2k::createPeerHelloPayload(
      std::string(ed2k::HASH_LENGTH, '\x22'), 0x04030201,
      peerAddr.port, ed2k::Endpoint(), "test-peer", remoteInfo, false);
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_HELLOANSWER,
                                           helloAnswerPayload));
  runEngineTicks(engine, 1);

  auto emuleInfo = readPacket(peerSocket, engine);
  auto emuleInfoHeader = packetHeaderOf(emuleInfo);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EMULE, emuleInfoHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_EMULEINFO, emuleInfoHeader.opcode);

  auto fileRequest = readPacket(peerSocket, engine);
  auto fileRequestHeader = packetHeaderOf(fileRequest);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EDONKEY, fileRequestHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_REQUESTFILENAME, fileRequestHeader.opcode);
  CPPUNIT_ASSERT_EQUAL(attrs->link.hash,
                       packetBodyOf(fileRequest).substr(0,
                                                        ed2k::HASH_LENGTH));

  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_REQFILENAMEANSWER,
      attrs->link.hash + std::string("ed2k-command-test.bin")));
  runEngineTicks(engine, 1);

  auto statusRequest = readPacket(peerSocket, engine);
  auto statusRequestHeader = packetHeaderOf(statusRequest);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EDONKEY, statusRequestHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_SETREQFILEID, statusRequestHeader.opcode);
  CPPUNIT_ASSERT_EQUAL(attrs->link.hash, packetBodyOf(statusRequest));

  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_FILESTATUS,
      ed2k::createFileStatusPayload(attrs->link.hash,
                                    std::vector<bool>{true, true})));
  runEngineTicks(engine, 1);

  auto hashSetRequest = readPacket(peerSocket, engine);
  auto hashSetRequestHeader = packetHeaderOf(hashSetRequest);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EDONKEY, hashSetRequestHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_HASHSETREQUEST, hashSetRequestHeader.opcode);
  CPPUNIT_ASSERT_EQUAL(attrs->link.hash, packetBodyOf(hashSetRequest));

  const auto pieceHashes = createPieceHashes();
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_HASHSETANSWER,
      ed2k::createHashSetAnswerPayload(attrs->link.hash, pieceHashes)));
  runEngineTicks(engine, 1);

  auto sourceExchange = readPacket(peerSocket, engine);
  auto sourceExchangeHeader = packetHeaderOf(sourceExchange);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EMULE, sourceExchangeHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_REQUESTSOURCES2,
                       sourceExchangeHeader.opcode);
  CPPUNIT_ASSERT_EQUAL(ed2k::createRequestSources2Payload(attrs->link.hash),
                       packetBodyOf(sourceExchange));

  auto startUpload = readPacket(peerSocket, engine);
  auto startUploadHeader = packetHeaderOf(startUpload);
  CPPUNIT_ASSERT_EQUAL(ed2k::PROTO_EDONKEY, startUploadHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_STARTUPLOADREQ, startUploadHeader.opcode);
  CPPUNIT_ASSERT_EQUAL(attrs->link.hash, packetBodyOf(startUpload));

  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_QUEUERANK,
      ed2k::createQueueRankPayload(7)));
  runEngineTicks(engine, MAX_ENGINE_TICKS);

  auto state = getEd2kPeerState(attrs, peer);
  CPPUNIT_ASSERT(state);
  CPPUNIT_ASSERT(state->queued);
  CPPUNIT_ASSERT_EQUAL((uint16_t)7, state->queueRank);
  CPPUNIT_ASSERT(!state->connecting);
  engine.requestHalt();
}

void Ed2kCommandTest::testPeerCommandFinishesGroupAfterLastPart()
{
  auto option = createOption();
  const std::string data = "finished ed2k command data";
  const std::string outfile = A2_TEST_OUT_DIR "/ed2k-command-finished.bin";
  File(outfile).remove();
  File(outfile + DefaultBtProgressInfoFile::getSuffix()).remove();

  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, static_cast<int64_t>(data.size()), outfile);
  auto attrs = make_unique<Ed2kAttribute>();
  attrs->link.type = ed2k::LinkType::FILE;
  attrs->link.name = "ed2k-command-finished.bin";
  attrs->link.size = data.size();
  attrs->link.hash = ed2k::md4Digest(data);
  attrs->clientHash = normalizeEd2kClientHash(std::string(ed2k::HASH_LENGTH,
                                                          '\x42'));
  attrs->pieceHashes.push_back(attrs->link.hash);
  dctx->setAttribute(CTX_ATTR_ED2K, std::move(attrs));

  auto group = createRequestGroup(option, dctx);
  group->initPieceStorage();
  group->getPieceStorage()->getDiskAdaptor()->openFile();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  SocketCore listenSocket;
  listenSocket.bind(0);
  listenSocket.beginListen();
  listenSocket.setBlockingMode();
  auto peerAddr = listenSocket.getAddrInfo();

  ed2k::Endpoint peer;
  peer.host = "127.0.0.1";
  peer.port = peerAddr.port;
  addEd2kPeer(getEd2kAttrs(dctx), peer, ed2k::PEER_SOURCE_INLINE);
  engine.addCommand(make_unique<Ed2kCommand>(engine.newCUID(), group.get(),
                                             &engine, peer, false));
  auto listener = make_unique<Ed2kListenCommand>(engine.newCUID(), &engine,
                                                 AF_INET);
  CPPUNIT_ASSERT(listener->bindPort(0));
  engine.addCommand(std::move(listener));

  runEngineTicks(engine, 1);
  auto peerSocket = acceptPeer(listenSocket, engine);
  runEngineTicks(engine, 1);

  readPacket(peerSocket, engine);
  auto remoteInfo = ed2k::createLocalEmulePeerInfo();
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_HELLOANSWER,
      ed2k::createPeerHelloPayload(
          std::string(ed2k::HASH_LENGTH, '\x22'), 0x04030201,
          peerAddr.port, ed2k::Endpoint(), "test-peer", remoteInfo, false)));
  runEngineTicks(engine, 1);

  readPacket(peerSocket, engine);
  readPacket(peerSocket, engine);
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_REQFILENAMEANSWER,
      getEd2kAttrs(dctx)->link.hash + std::string("ed2k-command-finished.bin")));
  runEngineTicks(engine, 1);

  readPacket(peerSocket, engine);
  peerSocket->writeData(ed2k::createPacket(
      ed2k::PROTO_EDONKEY, ed2k::OP_FILESTATUS,
      ed2k::createFileStatusPayload(getEd2kAttrs(dctx)->link.hash,
                                    std::vector<bool>{true})));
  runEngineTicks(engine, 1);

  readPacket(peerSocket, engine);
  readPacket(peerSocket, engine);
  peerSocket->writeData(ed2k::createPacket(ed2k::PROTO_EDONKEY,
                                           ed2k::OP_ACCEPTUPLOADREQ,
                                           std::string()));
  runEngineTicks(engine, 1);

  auto partRequest = readPacket(peerSocket, engine);
  CPPUNIT_ASSERT_EQUAL(ed2k::OP_REQUESTPARTS,
                       packetHeaderOf(partRequest).opcode);
  const size_t split = 8;
  peerSocket->writeData(
      ed2k::createPacket(ed2k::PROTO_EDONKEY, ed2k::OP_SENDINGPART,
                         getEd2kAttrs(dctx)->link.hash +
                             ed2k::packUInt32(0) +
                             ed2k::packUInt32(split) +
                             data.substr(0, split)));
  runEngineTicks(engine, 1);

  peerSocket->writeData(
      ed2k::createPacket(ed2k::PROTO_EDONKEY, ed2k::OP_SENDINGPART,
                         getEd2kAttrs(dctx)->link.hash +
                             ed2k::packUInt32(split) +
                             ed2k::packUInt32(data.size()) +
                             data.substr(split)));
  engine.setRefreshInterval(std::chrono::milliseconds(0));
  for (int i = 0; i < MAX_ENGINE_TICKS &&
                  engine.getRequestGroupMan()->getDownloadResults().empty();
       ++i) {
    engine.run(true);
  }

  CPPUNIT_ASSERT(group->downloadFinished());
  CPPUNIT_ASSERT_EQUAL((int32_t)0, group->getNumCommand());
  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       engine.getRequestGroupMan()->getDownloadResults().size());
  CPPUNIT_ASSERT_EQUAL(error_code::FINISHED,
                       engine.getRequestGroupMan()->getDownloadResults()[0]
                           ->result);
}

void Ed2kCommandTest::testPendingConnectDrainsAfterHalt()
{
  auto option = createOption();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto dctx = createEd2kContext();
  auto group = createRequestGroup(option, dctx);
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  ed2k::Endpoint endpoint;
  endpoint.host = "192.0.2.1";
  endpoint.port = 4661;
  engine.addCommand(make_unique<Ed2kCommand>(engine.newCUID(), group.get(),
                                             &engine, endpoint, true, false));

  runEngineTicks(engine, 2);
  CPPUNIT_ASSERT_EQUAL((int32_t)1, group->getNumCommand());

  group->setHaltRequested(true, RequestGroup::USER_REQUEST);
  engine.setRefreshInterval(std::chrono::milliseconds(0));
  for (int i = 0; i < MAX_ENGINE_TICKS && group->getNumCommand() != 0; ++i) {
    engine.run(true);
  }

  CPPUNIT_ASSERT_EQUAL((int32_t)0, group->getNumCommand());
}

void Ed2kCommandTest::testForceHaltDrainsIdleKadGroup()
{
  auto option = createOption();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto dctx = createEd2kContext();
  auto group = createRequestGroup(option, dctx);
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  engine.addCommand(make_unique<Ed2kKadCommand>(engine.newCUID(), group.get(),
                                                &engine));
  runEngineTicks(engine, 1);
  CPPUNIT_ASSERT_EQUAL((int32_t)1, group->getNumCommand());

  engine.getRequestGroupMan()->clearQueueCheck();
  group->setForceHaltRequested(true, RequestGroup::USER_REQUEST);
  CPPUNIT_ASSERT(!engine.getRequestGroupMan()->queueCheckRequested());
  runEngineTicks(engine, 1);
  CPPUNIT_ASSERT_EQUAL((int32_t)0, group->getNumCommand());
  engine.getRequestGroupMan()->removeStoppedGroup(&engine);
  CPPUNIT_ASSERT_EQUAL((size_t)0,
                       engine.getRequestGroupMan()->countRequestGroup());
}

void Ed2kCommandTest::testKadBootstrapSourceSearchAddsPeer()
{
  auto option = createOption();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto dctx = createEd2kContext();
  auto group = createRequestGroup(option, dctx);
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->setKeepRunning(true);
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  SocketCore routerSocket(SOCK_DGRAM);
  routerSocket.bind("127.0.0.1", 0, AF_INET);
  routerSocket.setBlockingMode();
  auto routerAddr = routerSocket.getAddrInfo();

  auto attrs = getEd2kAttrs(dctx);
  const auto kadFileId = ed2k::ed2kHashToKadId(attrs->link.hash);
  attrs->kadRoutingTable =
      std::make_shared<ed2k::KadRoutingTable>(std::string(16, '\x55'));
  ed2k::Endpoint routerEndpoint;
  routerEndpoint.host = "127.0.0.1";
  routerEndpoint.port = routerAddr.port;
  attrs->kadRoutingTable->addRouterNode(routerEndpoint);

  auto command = make_unique<Ed2kKadCommand>(engine.newCUID(), group.get(),
                                             &engine);
  auto commandPtr = command.get();
  engine.addCommand(std::move(command));

  auto bootstrapReq = readDatagram(routerSocket, engine);
  const auto kadUdpPort = commandPtr->getLocalUdpPort();
  auto bootstrapReqHeader = datagramHeaderOf(bootstrapReq);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_PROTOCOL, bootstrapReqHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_BOOTSTRAP_REQ, bootstrapReqHeader.opcode);

  const auto remoteId = std::string(16, '\x33');
  writeDatagram(
      routerSocket,
      ed2k::createDatagram(
          ed2k::KAD_PROTOCOL, ed2k::KAD_BOOTSTRAP_RES,
          ed2k::createKadBootstrapResponsePayload(
              remoteId, 4662, 8,
              std::vector<ed2k::KadContact>{
                  createKadContact(remoteId, routerAddr.port)})),
      kadUdpPort);
  runEngineTicks(engine, 1);

  auto kadReq = decodeKadDatagram(readDatagram(routerSocket, engine), remoteId);
  auto kadReqHeader = datagramHeaderOf(kadReq);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_PROTOCOL, kadReqHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_REQ, kadReqHeader.opcode);
  ed2k::KadRequest parsedKadReq;
  CPPUNIT_ASSERT(ed2k::parseKadRequestPayload(parsedKadReq,
                                              datagramBodyOf(kadReq)));
  CPPUNIT_ASSERT_EQUAL((uint8_t)ed2k::KAD_FIND_VALUE,
                       parsedKadReq.searchType);
  CPPUNIT_ASSERT_EQUAL(kadFileId, parsedKadReq.targetId);

  writeDatagram(
      routerSocket,
      ed2k::createDatagram(ed2k::KAD_PROTOCOL, ed2k::KAD_RES,
                           ed2k::createKadResponsePayload(
                               kadFileId,
                               std::vector<ed2k::KadContact>())),
      kadUdpPort);
  runEngineTicks(engine, 1);

  auto sourceSearchDatagram =
      readKadDatagramWithOpcode(routerSocket, engine,
                                ed2k::KAD_SEARCH_SOURCES_REQ, remoteId);
  CPPUNIT_ASSERT_EQUAL(kadUdpPort, sourceSearchDatagram.sender.port);
  auto sourceSearchHeader = datagramHeaderOf(sourceSearchDatagram.data);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_PROTOCOL, sourceSearchHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_SEARCH_SOURCES_REQ,
                       sourceSearchHeader.opcode);
  ed2k::KadSearchSourcesRequest searchRequest;
  CPPUNIT_ASSERT(ed2k::parseKadSearchSourcesRequestPayload(
      searchRequest, datagramBodyOf(sourceSearchDatagram.data)));
  CPPUNIT_ASSERT_EQUAL(kadFileId, searchRequest.targetId);

  ed2k::Endpoint source;
  source.host = "203.0.113.44";
  source.port = 4662;
  const auto sourceId = std::string(16, '\x44');
  ed2k::KadPublishSourceRequest publishRequest;
  CPPUNIT_ASSERT(ed2k::parseKadPublishSourceRequestPayload(
      publishRequest,
      ed2k::createKadPublishSourceRequestPayload(attrs->link.hash, source,
                                                 sourceId,
                                                 attrs->link.size)));
  ed2k::KadSearchResult localResult;
  CPPUNIT_ASSERT(ed2k::parseKadSearchResultPayload(
      localResult,
      ed2k::createKadSearchResultPayload(
          remoteId, kadFileId,
          std::vector<ed2k::KadSearchEntry>{publishRequest.source})));
  auto localPeers = ed2k::extractKadSourceEndpoints(localResult);
  CPPUNIT_ASSERT_EQUAL((size_t)1, localPeers.size());
  writeDatagram(
      routerSocket,
      ed2k::createDatagram(
          ed2k::KAD_PROTOCOL, ed2k::KAD_SEARCH_RES,
          ed2k::createKadSearchResultPayload(
              remoteId, kadFileId,
              std::vector<ed2k::KadSearchEntry>{publishRequest.source})),
      sourceSearchDatagram.sender.port);
  for (int i = 0; i < MAX_ENGINE_TICKS && attrs->peers.empty(); ++i) {
    engine.run(true);
  }

  CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->peers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.44"), attrs->peers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, attrs->peers[0].port);
  engine.requestHalt();
}

void Ed2kCommandTest::testKadDecodesReceiverKeyObfuscatedResponse()
{
  auto option = createOption();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto dctx = createEd2kContext();
  auto group = createRequestGroup(option, dctx);
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->setKeepRunning(true);
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  SocketCore routerSocket(SOCK_DGRAM);
  routerSocket.bind("127.0.0.1", 0, AF_INET);
  routerSocket.setBlockingMode();
  auto routerAddr = routerSocket.getAddrInfo();

  const auto remoteId = std::string(16, '\x33');
  auto attrs = getEd2kAttrs(dctx);
  attrs->kadRoutingTable =
      std::make_shared<ed2k::KadRoutingTable>(std::string(16, '\x55'));
  attrs->kadRoutingTable->addRouterNode(
      createKadContact(remoteId, routerAddr.port));

  auto command = make_unique<Ed2kKadCommand>(engine.newCUID(), group.get(),
                                             &engine);
  auto commandPtr = command.get();
  engine.addCommand(std::move(command));

  auto bootstrapReq = readDatagramFrom(routerSocket, engine);
  ed2k::KadObfuscatedDatagram parsedReq;
  CPPUNIT_ASSERT(ed2k::parseKadObfuscatedDatagram(parsedReq, bootstrapReq.data,
                                                  remoteId));
  CPPUNIT_ASSERT(parsedReq.senderVerifyKey != 0);

  writeDatagram(
      routerSocket,
      ed2k::createKadObfuscatedDatagram(
          ed2k::createDatagram(
              ed2k::KAD_PROTOCOL, ed2k::KAD_BOOTSTRAP_RES,
              ed2k::createKadBootstrapResponsePayload(
                  remoteId, 4662, 8,
                  std::vector<ed2k::KadContact>{
                      createKadContact(remoteId, routerAddr.port)})),
          parsedReq.senderVerifyKey, 0x55667788, 0x1234),
      commandPtr->getLocalUdpPort());
  runEngineTicks(engine, 1);

  auto kadReq = decodeKadDatagram(readDatagram(routerSocket, engine), remoteId);
  auto kadReqHeader = datagramHeaderOf(kadReq);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_PROTOCOL, kadReqHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_REQ, kadReqHeader.opcode);
  engine.requestHalt();
}

void Ed2kCommandTest::testKadDecodesSelfIdObfuscatedResponse()
{
  auto option = createOption();
  DownloadEngine engine(make_unique<SelectEventPoll>());
  engine.setOption(option.get());
  auto dctx = createEd2kContext();
  auto group = createRequestGroup(option, dctx);
  engine.setRequestGroupMan(
      make_unique<RequestGroupMan>(
          std::vector<std::shared_ptr<RequestGroup>>{group}, 5,
          option.get()));
  engine.getRequestGroupMan()->setKeepRunning(true);
  engine.getRequestGroupMan()->addRequestGroup(group);
  group->setRequestGroupMan(engine.getRequestGroupMan().get());

  SocketCore routerSocket(SOCK_DGRAM);
  routerSocket.bind("127.0.0.1", 0, AF_INET);
  routerSocket.setBlockingMode();
  auto routerAddr = routerSocket.getAddrInfo();

  const auto remoteId = std::string(16, '\x33');
  auto attrs = getEd2kAttrs(dctx);
  const auto localKadId = ed2k::ed2kHashToKadId(attrs->clientHash);
  attrs->kadRoutingTable =
      std::make_shared<ed2k::KadRoutingTable>(std::string(16, '\x55'));
  attrs->kadRoutingTable->addRouterNode(
      createKadContact(remoteId, routerAddr.port));

  auto command = make_unique<Ed2kKadCommand>(engine.newCUID(), group.get(),
                                             &engine);
  auto commandPtr = command.get();
  engine.addCommand(std::move(command));

  readDatagramFrom(routerSocket, engine);
  writeDatagram(
      routerSocket,
      ed2k::createKadObfuscatedDatagram(
          ed2k::createDatagram(
              ed2k::KAD_PROTOCOL, ed2k::KAD_BOOTSTRAP_RES,
              ed2k::createKadBootstrapResponsePayload(
                  remoteId, 4662, 8,
                  std::vector<ed2k::KadContact>{
                      createKadContact(remoteId, routerAddr.port)})),
          localKadId, 0x1234),
      commandPtr->getLocalUdpPort());
  runEngineTicks(engine, 1);

  auto kadReq = decodeKadDatagram(readDatagram(routerSocket, engine), remoteId);
  auto kadReqHeader = datagramHeaderOf(kadReq);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_PROTOCOL, kadReqHeader.protocol);
  CPPUNIT_ASSERT_EQUAL(ed2k::KAD_REQ, kadReqHeader.opcode);
  engine.requestHalt();
}

} // namespace aria2
