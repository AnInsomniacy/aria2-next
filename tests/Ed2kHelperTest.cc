#include "ed2k_helper.h"
#include "Ed2kKadState.h"

#include <algorithm>
#include <cppunit/extensions/HelperMacros.h>
#include <cstring>
#include <zlib.h>

#include "Exception.h"
#include "base32.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

class Ed2kHelperTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(Ed2kHelperTest);
  CPPUNIT_TEST(testParseFileLink);
  CPPUNIT_TEST(testParseFileLinkWithOptions);
  CPPUNIT_TEST(testParseFileLinkWithSourceCryptOptions);
  CPPUNIT_TEST(testParseServerLink);
  CPPUNIT_TEST(testParseSearchLink);
  CPPUNIT_TEST(testParseRejectsMalformedLinks);
  CPPUNIT_TEST(testSerializeFileLink);
  CPPUNIT_TEST(testPacketHelpers);
  CPPUNIT_TEST(testTagParser);
  CPPUNIT_TEST(testProtocolPayloads);
  CPPUNIT_TEST(testServerPayloadParsers);
  CPPUNIT_TEST(testSearchRequestPayload);
  CPPUNIT_TEST(testSearchResultPayload);
  CPPUNIT_TEST(testKadKeywordTarget);
  CPPUNIT_TEST(testKadSearchEntriesToSearchResults);
  CPPUNIT_TEST(testSourceExchange2Payloads);
  CPPUNIT_TEST(testCompressedPartPayloads);
  CPPUNIT_TEST(testInflateCompressedPartData);
  CPPUNIT_TEST(testInflatePackedPacketPayload);
  CPPUNIT_TEST(testEmuleInfoPayload);
  CPPUNIT_TEST(testLocalEmulePeerInfoCapabilities);
  CPPUNIT_TEST(testPeerHelloPayload);
  CPPUNIT_TEST(testUdpReaskPayloads);
  CPPUNIT_TEST(testAichPayloads);
  CPPUNIT_TEST(testAichRecoveryData);
  CPPUNIT_TEST(testAichHashTree);
  CPPUNIT_TEST(testAichHashTreeKeepsPartLevel);
  CPPUNIT_TEST(testKadPacketPayloads);
  CPPUNIT_TEST(testKadSearchPublishAndFirewallPayloads);
  CPPUNIT_TEST(testKadRoutingStatePayload);
  CPPUNIT_TEST(testServerStatePayload);
  CPPUNIT_TEST(testNodesDatParser);
  CPPUNIT_TEST(testServerMetParser);
  CPPUNIT_TEST(testMd4Digest);
  CPPUNIT_TEST(testRootHash);
  CPPUNIT_TEST_SUITE_END();

public:
  void testParseFileLink();
  void testParseFileLinkWithOptions();
  void testParseFileLinkWithSourceCryptOptions();
  void testParseServerLink();
  void testParseSearchLink();
  void testParseRejectsMalformedLinks();
  void testSerializeFileLink();
  void testPacketHelpers();
  void testTagParser();
  void testProtocolPayloads();
  void testServerPayloadParsers();
  void testSearchRequestPayload();
  void testSearchResultPayload();
  void testKadKeywordTarget();
  void testKadSearchEntriesToSearchResults();
  void testSourceExchange2Payloads();
  void testCompressedPartPayloads();
  void testInflateCompressedPartData();
  void testInflatePackedPacketPayload();
  void testEmuleInfoPayload();
  void testLocalEmulePeerInfoCapabilities();
  void testPeerHelloPayload();
  void testUdpReaskPayloads();
  void testAichPayloads();
  void testAichRecoveryData();
  void testAichHashTree();
  void testAichHashTreeKeepsPartLevel();
  void testKadPacketPayloads();
  void testKadSearchPublishAndFirewallPayloads();
  void testKadRoutingStatePayload();
  void testServerStatePayload();
  void testNodesDatParser();
  void testServerMetParser();
  void testMd4Digest();
  void testRootHash();
};

CPPUNIT_TEST_SUITE_REGISTRATION(Ed2kHelperTest);

void Ed2kHelperTest::testParseFileLink()
{
  auto link = parseLink(
      "ed2k://|file|aria2%20next.bin|12345|"
      "0123456789ABCDEF0123456789ABCDEF|/");

  CPPUNIT_ASSERT_EQUAL(LinkType::FILE, link.type);
  CPPUNIT_ASSERT_EQUAL(std::string("aria2 next.bin"), link.name);
  CPPUNIT_ASSERT_EQUAL((int64_t)12345, link.size);
  CPPUNIT_ASSERT_EQUAL(
      std::string("0123456789abcdef0123456789abcdef"),
      util::toHex(link.hash));

  auto unsafeName = parseLink(
      "ed2k://|file|aria2%2Fnext%5Ctest.bin|12345|"
      "0123456789ABCDEF0123456789ABCDEF|/");
  CPPUNIT_ASSERT_EQUAL(std::string("aria2_next_test.bin"), unsafeName.name);

  auto encodedSeparators = parseLink(
      "ed2k://%7Cfile%7Caria2%20next.bin%7C12345%7C"
      "0123456789ABCDEF0123456789ABCDEF%7C/");
  CPPUNIT_ASSERT_EQUAL(LinkType::FILE, encodedSeparators.type);
  CPPUNIT_ASSERT_EQUAL(std::string("aria2 next.bin"), encodedSeparators.name);
}

void Ed2kHelperTest::testParseFileLinkWithOptions()
{
  auto link = parseLink(
      "ed2k://|file|movie.mkv|9728001|"
      "0123456789abcdef0123456789abcdef|"
      "p=11111111111111111111111111111111:"
      "22222222222222222222222222222222|"
      "h=ABCDEFGHIJKLMNOPQRSTUVWXYZ234567|"
      "sources,192.0.2.1:4662,198.51.100.7:7777|/");

  CPPUNIT_ASSERT_EQUAL((size_t)2, link.pieceHashes.size());
  CPPUNIT_ASSERT_EQUAL(std::string(16, '\x11'), link.pieceHashes[0]);
  CPPUNIT_ASSERT_EQUAL(std::string(16, '\x22'), link.pieceHashes[1]);
  std::string aichRoot("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
  CPPUNIT_ASSERT_EQUAL(base32::decode(aichRoot.begin(), aichRoot.end()),
                       link.aichHash);
  CPPUNIT_ASSERT_EQUAL((size_t)2, link.sources.size());
  CPPUNIT_ASSERT_EQUAL(std::string("192.0.2.1"), link.sources[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, link.sources[0].port);
  CPPUNIT_ASSERT_EQUAL(std::string("198.51.100.7"), link.sources[1].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)7777, link.sources[1].port);
}

void Ed2kHelperTest::testParseFileLinkWithSourceCryptOptions()
{
  auto link = parseLink(
      "ed2k://|file|shared.bin|123|"
      "0123456789abcdef0123456789abcdef|/|"
      "sources,203.0.113.1:4662:131:"
      "11111111111111111111111111111111,"
      "peer.example.test:7777:1|/");

  CPPUNIT_ASSERT_EQUAL((size_t)2, link.sources.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.1"), link.sources[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, link.sources[0].port);
  CPPUNIT_ASSERT_EQUAL((uint16_t)131, link.sources[0].cryptOptions);
  CPPUNIT_ASSERT_EQUAL(std::string(16, '\x11'), link.sources[0].userHash);
  CPPUNIT_ASSERT_EQUAL(std::string("peer.example.test"), link.sources[1].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)7777, link.sources[1].port);
  CPPUNIT_ASSERT_EQUAL((uint16_t)1, link.sources[1].cryptOptions);
  CPPUNIT_ASSERT(link.sources[1].userHash.empty());

  auto reparsed = parseLink(toFileLink(link));
  CPPUNIT_ASSERT_EQUAL((uint16_t)131, reparsed.sources[0].cryptOptions);
  CPPUNIT_ASSERT_EQUAL(std::string(16, '\x11'), reparsed.sources[0].userHash);
  CPPUNIT_ASSERT_EQUAL((uint16_t)1, reparsed.sources[1].cryptOptions);
}

void Ed2kHelperTest::testParseServerLink()
{
  auto server = parseLink("ed2k://|server|203.0.113.10|4232|/");

  CPPUNIT_ASSERT_EQUAL(LinkType::SERVER, server.type);
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), server.server.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4232, server.server.port);

  auto serverList =
      parseLink("ed2k://|serverlist|http%3A%2F%2Fexample.test%2Fserver.met|/");
  CPPUNIT_ASSERT_EQUAL(LinkType::SERVER_LIST, serverList.type);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.test/server.met"),
                       serverList.url);

  auto nodes =
      parseLink("ed2k://|nodeslist|https%3A%2F%2Fexample.test%2Fnodes.dat|/");
  CPPUNIT_ASSERT_EQUAL(LinkType::NODES_LIST, nodes.type);
  CPPUNIT_ASSERT_EQUAL(std::string("https://example.test/nodes.dat"),
                       nodes.url);
}

void Ed2kHelperTest::testParseSearchLink()
{
  auto search = parseLink("ed2k://|search|linux%20iso|/");

  CPPUNIT_ASSERT_EQUAL(LinkType::SEARCH, search.type);
  CPPUNIT_ASSERT_EQUAL(std::string("linux iso"), search.name);
}

void Ed2kHelperTest::testParseRejectsMalformedLinks()
{
  CPPUNIT_ASSERT_THROW(parseLink("http://example.test/file"),
                       RecoverableException);
  CPPUNIT_ASSERT_THROW(
      parseLink("ed2k://|file|bad.bin|x|0123456789abcdef0123456789abcdef|/"),
      RecoverableException);
  CPPUNIT_ASSERT_THROW(parseLink("ed2k://|file|bad.bin|1|not-a-hash|/"),
                       RecoverableException);
  CPPUNIT_ASSERT_THROW(
      parseLink("ed2k://|file|empty.bin|0|0123456789abcdef0123456789abcdef|/"),
      RecoverableException);
  CPPUNIT_ASSERT_THROW(
      parseLink("ed2k://|file|huge.bin|274877906944|"
                "0123456789abcdef0123456789abcdef|/"),
      RecoverableException);
  CPPUNIT_ASSERT_THROW(
      parseLink("ed2k://|file|bad-parts.bin|1|"
                "0123456789abcdef0123456789abcdef|p=|/"),
      RecoverableException);
  CPPUNIT_ASSERT_THROW(
      parseLink("ed2k://|file|bad-aich.bin|1|"
                "0123456789abcdef0123456789abcdef|h=ABC|/"),
      RecoverableException);
  CPPUNIT_ASSERT_THROW(parseLink("ed2k://|server|127.0.0.1|70000|/"),
                       RecoverableException);
}

void Ed2kHelperTest::testSerializeFileLink()
{
  auto link = parseLink(
      "ed2k://|file|aria2%20next.bin|9728001|"
      "0123456789abcdef0123456789abcdef|"
      "p=11111111111111111111111111111111:"
      "22222222222222222222222222222222|"
      "h=ABCDEFGHIJKLMNOPQRSTUVWXYZ234567|"
      "sources,192.0.2.1:4662|/");

  auto reparsed = parseLink(toFileLink(link));

  CPPUNIT_ASSERT_EQUAL(link.name, reparsed.name);
  CPPUNIT_ASSERT_EQUAL(link.size, reparsed.size);
  CPPUNIT_ASSERT_EQUAL(link.hash, reparsed.hash);
  CPPUNIT_ASSERT_EQUAL(link.pieceHashes[0], reparsed.pieceHashes[0]);
  CPPUNIT_ASSERT_EQUAL(link.aichHash, reparsed.aichHash);
  CPPUNIT_ASSERT_EQUAL(std::string("192.0.2.1"), reparsed.sources[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, reparsed.sources[0].port);
}

void Ed2kHelperTest::testPacketHelpers()
{
  std::string payload;
  payload += "abc";
  auto packet = createPacket(PROTO_EDONKEY, OP_GETSOURCES, payload);

  CPPUNIT_ASSERT_EQUAL((size_t)9, packet.size());
  CPPUNIT_ASSERT_EQUAL((unsigned char)PROTO_EDONKEY,
                       (unsigned char)packet[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("04000000"), util::toHex(packet.substr(1, 4)));
  CPPUNIT_ASSERT_EQUAL((unsigned char)OP_GETSOURCES,
                       (unsigned char)packet[5]);
  CPPUNIT_ASSERT_EQUAL(std::string("abc"), packet.substr(6));

  PacketHeader header;
  CPPUNIT_ASSERT(readPacketHeader(header, packet.data(), packet.size()));
  CPPUNIT_ASSERT_EQUAL((uint8_t)PROTO_EDONKEY, header.protocol);
  CPPUNIT_ASSERT_EQUAL((uint32_t)4, header.size);
  CPPUNIT_ASSERT_EQUAL((uint8_t)OP_GETSOURCES, header.opcode);
  CPPUNIT_ASSERT_EQUAL((size_t)3, header.payloadSize());

  CPPUNIT_ASSERT_EQUAL(std::string("78563412"), util::toHex(packUInt32(0x12345678)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x12345678,
                       readUInt32(std::string("\x78\x56\x34\x12", 4).data()));
}

void Ed2kHelperTest::testTagParser()
{
  std::string payload;
  payload += packUInt32(4);
  payload.push_back(static_cast<char>(0x02 | 0x80));
  payload.push_back('\x01');
  payload += packUInt16(9);
  payload += "video.mkv";
  payload.push_back(static_cast<char>(0x03 | 0x80));
  payload.push_back('\x15');
  payload += packUInt32(77);
  payload.push_back(static_cast<char>(0x0b | 0x80));
  payload.push_back('\x02');
  payload += packUInt64(0x100000005LL);
  payload.push_back(static_cast<char>(0x13 | 0x80));
  payload.push_back('\x03');
  payload += "Vid";

  std::vector<Tag> tags;
  CPPUNIT_ASSERT(parseTagList(tags, payload));
  CPPUNIT_ASSERT_EQUAL((size_t)4, tags.size());
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x01, tags[0].id);
  CPPUNIT_ASSERT_EQUAL(std::string("video.mkv"), tags[0].stringValue);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x15, tags[1].id);
  CPPUNIT_ASSERT_EQUAL((uint64_t)77, tags[1].intValue);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x02, tags[2].id);
  CPPUNIT_ASSERT_EQUAL((uint64_t)0x100000005LL, tags[2].intValue);
  CPPUNIT_ASSERT_EQUAL(std::string("Vid"), tags[3].stringValue);
}

void Ed2kHelperTest::testProtocolPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::string clientHashHex("11111111111111111111111111111111");
  auto clientHash =
      util::fromHex(clientHashHex.begin(), clientHashHex.end());

  auto login = createLoginRequestPayload(clientHash, 0x04030201, 0,
                                         "aria2-next");
  CPPUNIT_ASSERT_EQUAL(clientHash, login.substr(0, HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL(std::string("010203040000"),
                       util::toHex(login.substr(16, 6)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)4, readUInt32(login.data() + 22));

  auto source32 = createGetSourcesPayload(fileHash, 9728001);
  CPPUNIT_ASSERT_EQUAL((size_t)20, source32.size());
  CPPUNIT_ASSERT_EQUAL(fileHash, source32.substr(0, HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL((uint32_t)9728001, readUInt32(source32.data() + 16));

  auto source64 = createGetSourcesPayload(fileHash, 0x100000001LL);
  CPPUNIT_ASSERT_EQUAL((size_t)28, source64.size());
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, readUInt32(source64.data() + 16));
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, readUInt32(source64.data() + 20));
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, readUInt32(source64.data() + 24));

  auto globSources = createGlobGetSourcesPayload(fileHash, 9728001, false);
  CPPUNIT_ASSERT_EQUAL((size_t)16, globSources.size());
  CPPUNIT_ASSERT_EQUAL(fileHash, globSources);
  CPPUNIT_ASSERT_EQUAL(source32,
                       createGlobGetSourcesPayload(fileHash, 9728001, true));

  std::vector<Endpoint> sources;
  Endpoint source;
  source.host = "1.2.3.4";
  source.port = 4662;
  sources.push_back(source);
  auto found = createFoundSourcesPayload(fileHash, sources);
  auto parsedSources = parseFoundSourcesPayload(found);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsedSources.size());
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), parsedSources[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, parsedSources[0].port);
  CPPUNIT_ASSERT(parseFoundSourcesPayload(parsedSources, found, fileHash));
  CPPUNIT_ASSERT(!parseFoundSourcesPayload(parsedSources, found, clientHash));
  std::vector<FoundSource> foundSources;
  CPPUNIT_ASSERT(parseFoundSourcesPayload(foundSources, found, fileHash));
  CPPUNIT_ASSERT_EQUAL((size_t)1, foundSources.size());
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x04030201, foundSources[0].clientId);
  CPPUNIT_ASSERT(!foundSources[0].lowId);

  Endpoint lowId;
  lowId.host = "120.0.0.0";
  lowId.port = 4662;
  auto lowIdPayload =
      createFoundSourcesPayload(fileHash, std::vector<Endpoint>{lowId});
  CPPUNIT_ASSERT(parseFoundSourcesPayload(foundSources, lowIdPayload,
                                          fileHash));
  CPPUNIT_ASSERT_EQUAL((uint32_t)120, foundSources[0].clientId);
  CPPUNIT_ASSERT(foundSources[0].lowId);

  auto obfuPayload = found;
  obfuPayload.push_back(static_cast<char>(0x81));
  obfuPayload += clientHash;
  CPPUNIT_ASSERT(parseFoundSourcesPayload(foundSources, obfuPayload, fileHash,
                                          true));
  CPPUNIT_ASSERT_EQUAL((size_t)1, foundSources.size());
  CPPUNIT_ASSERT_EQUAL((uint16_t)0x81,
                       foundSources[0].endpoint.cryptOptions);
  CPPUNIT_ASSERT_EQUAL(clientHash, foundSources[0].endpoint.userHash);
  CPPUNIT_ASSERT(!parseFoundSourcesPayload(foundSources, obfuPayload, fileHash));

  Endpoint source2;
  source2.host = "5.6.7.8";
  source2.port = 4662;
  auto packedFound =
      createFoundSourcesPayload(clientHash, std::vector<Endpoint>{source}) +
      createDatagram(PROTO_EDONKEY, OP_GLOBFOUNDSOURCES,
                     createFoundSourcesPayload(fileHash,
                                               std::vector<Endpoint>{source2}));
  std::vector<Endpoint> packedSources;
  CPPUNIT_ASSERT(parsePackedFoundSourcesPayloads(packedSources, packedFound,
                                                 fileHash));
  CPPUNIT_ASSERT_EQUAL((size_t)1, packedSources.size());
  CPPUNIT_ASSERT_EQUAL(std::string("5.6.7.8"), packedSources[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, packedSources[0].port);
  std::vector<FoundSource> packedFoundSources;
  CPPUNIT_ASSERT(parsePackedFoundSourcesPayloads(packedFoundSources,
                                                 packedFound, fileHash));
  CPPUNIT_ASSERT_EQUAL((size_t)1, packedFoundSources.size());
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x08070605,
                       packedFoundSources[0].clientId);
  CPPUNIT_ASSERT(!packedFoundSources[0].lowId);
  auto packedWithBadTail =
      createFoundSourcesPayload(fileHash, std::vector<Endpoint>{source}) +
      createDatagram(PROTO_EDONKEY, OP_GLOBFOUNDSOURCES,
                     createFoundSourcesPayload(clientHash,
                                               std::vector<Endpoint>{source2})) +
      std::string("\xe3\x90", 2);
  CPPUNIT_ASSERT(parsePackedFoundSourcesPayloads(
      packedFoundSources, packedWithBadTail, fileHash));
  CPPUNIT_ASSERT_EQUAL((size_t)1, packedFoundSources.size());
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x04030201,
                       packedFoundSources[0].clientId);

  auto callbackRequest = createCallbackRequestPayload(120);
  CPPUNIT_ASSERT_EQUAL(std::string("78000000"),
                       util::toHex(callbackRequest));
  Endpoint callbackEndpoint;
  CPPUNIT_ASSERT(parseCallbackRequestIncomingPayload(
      callbackEndpoint, packUInt32(0x04030201) + packUInt16(4662)));
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), callbackEndpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, callbackEndpoint.port);
  CPPUNIT_ASSERT(parseCallbackRequestIncomingPayload(
      callbackEndpoint, packUInt32(0x04030201) + packUInt16(4662) +
                            std::string(1, '\x83') + clientHash));
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), callbackEndpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, callbackEndpoint.port);
  CPPUNIT_ASSERT_EQUAL((uint16_t)0x83, callbackEndpoint.cryptOptions);
  CPPUNIT_ASSERT_EQUAL(clientHash, callbackEndpoint.userHash);
  CPPUNIT_ASSERT(parseCallbackRequestIncomingPayload(
      callbackEndpoint, packUInt32(0x04030201) + packUInt16(4662) +
                            std::string(1, '\x83') + clientHash +
                            std::string("ignored")));

  std::vector<bool> bitfield;
  bitfield.push_back(true);
  bitfield.push_back(false);
  bitfield.push_back(true);
  auto status = createFileStatusPayload(fileHash, bitfield);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x05, static_cast<uint8_t>(status[18]));
  std::vector<bool> parsedBitfield;
  CPPUNIT_ASSERT(parseFileStatusPayload(parsedBitfield, status, fileHash));
  CPPUNIT_ASSERT_EQUAL((size_t)3, parsedBitfield.size());
  CPPUNIT_ASSERT(parsedBitfield[0]);
  CPPUNIT_ASSERT(!parsedBitfield[1]);
  CPPUNIT_ASSERT(parsedBitfield[2]);

  std::vector<PartRange> ranges;
  PartRange range;
  range.begin = 0;
  range.end = 10;
  ranges.push_back(range);
  range.begin = 20;
  range.end = 30;
  ranges.push_back(range);
  auto requestParts = createRequestPartsPayload(fileHash, ranges, false);
  CPPUNIT_ASSERT_EQUAL((size_t)40, requestParts.size());
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, readUInt32(requestParts.data() + 16));
  CPPUNIT_ASSERT_EQUAL((uint32_t)20, readUInt32(requestParts.data() + 20));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, readUInt32(requestParts.data() + 24));
  CPPUNIT_ASSERT_EQUAL((uint32_t)10, readUInt32(requestParts.data() + 28));
  CPPUNIT_ASSERT_EQUAL((uint32_t)30, readUInt32(requestParts.data() + 32));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, readUInt32(requestParts.data() + 36));

  auto requestParts64 = createRequestPartsPayload(fileHash, ranges, true);
  CPPUNIT_ASSERT_EQUAL((size_t)64, requestParts64.size());
  CPPUNIT_ASSERT_EQUAL((uint64_t)0, readUInt64(requestParts64.data() + 16));
  CPPUNIT_ASSERT_EQUAL((uint64_t)20, readUInt64(requestParts64.data() + 24));
  CPPUNIT_ASSERT_EQUAL((uint64_t)0, readUInt64(requestParts64.data() + 32));
  CPPUNIT_ASSERT_EQUAL((uint64_t)10, readUInt64(requestParts64.data() + 40));
  CPPUNIT_ASSERT_EQUAL((uint64_t)30, readUInt64(requestParts64.data() + 48));
  CPPUNIT_ASSERT_EQUAL((uint64_t)0, readUInt64(requestParts64.data() + 56));
}

void Ed2kHelperTest::testServerPayloadParsers()
{
  ServerIdChange idChange;
  CPPUNIT_ASSERT(parseServerIdChangePayload(idChange, packUInt32(0x04030201)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x04030201, idChange.clientId);
  CPPUNIT_ASSERT(idChange.highId);
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), idChange.ipAddress);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, idChange.tcpFlags);

  CPPUNIT_ASSERT(parseServerIdChangePayload(idChange, packUInt32(120)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)120, idChange.clientId);
  CPPUNIT_ASSERT(!idChange.highId);
  CPPUNIT_ASSERT(idChange.ipAddress.empty());
  CPPUNIT_ASSERT(parseServerIdChangePayload(
      idChange, packUInt32(120) + packUInt32(0x55aa) + packUInt32(4661)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)120, idChange.clientId);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, idChange.tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)4661, idChange.auxPort);
  CPPUNIT_ASSERT(parseServerIdChangePayload(
      idChange, packUInt32(0x04030201) + packUInt32(SRV_TCPFLG_TCPOBFUSCATION) +
                    packUInt32(4661) + packUInt32(0x04030201) +
                    packUInt32(4666)));
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, idChange.tcpObfuscationPort);
  CPPUNIT_ASSERT(parseServerIdChangePayload(
      idChange, packUInt32(0x04030201) + packUInt32(0x55aa) +
                    packUInt32(4661) + packUInt32(0x04030201)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x04030201, idChange.clientId);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, idChange.tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)4661, idChange.auxPort);
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, idChange.tcpObfuscationPort);

  CPPUNIT_ASSERT(parseServerIdChangePayload(
      idChange, packUInt32(0x04030201) + packUInt32(0x55aa) +
                    packUInt32(4661) + packUInt32(0x04030201) +
                    packUInt32(4666) + packUInt32(0xdeadbeef)));
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, idChange.tcpObfuscationPort);

  ServerStatus status;
  CPPUNIT_ASSERT(parseServerStatusPayload(status,
                                          packUInt32(1234) + packUInt32(5678)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, status.users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, status.files);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, status.challenge);

  CPPUNIT_ASSERT(parseServerStatusPayload(
      status, packUInt32(1234) + packUInt32(5678) + packUInt32(9000)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, status.users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, status.files);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, status.challenge);

  CPPUNIT_ASSERT(parseServerStatusPayload(
      status, packUInt32(0x55aa0011) + packUInt32(1234) +
                  packUInt32(5678) + packUInt32(9000)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa0011, status.users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, status.files);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0, status.challenge);

  CPPUNIT_ASSERT(parseServerUdpStatusPayload(
      status, packUInt32(0x55aa0011) + packUInt32(1234) +
                  packUInt32(5678) + packUInt32(9000) + packUInt32(100) +
                  packUInt32(200) + packUInt32(0x01020304) +
                  packUInt32(77) + packUInt16(4665) + packUInt16(4666) +
                  packUInt32(0x11223344)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa0011, status.challenge);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, status.users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, status.files);
  CPPUNIT_ASSERT_EQUAL((uint32_t)9000, status.maxUsers);
  CPPUNIT_ASSERT_EQUAL((uint32_t)100, status.softFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)200, status.hardFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x01020304, status.udpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)77, status.lowIdUsers);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4665, status.udpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, status.tcpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x11223344, status.udpKey);
  CPPUNIT_ASSERT(parseServerUdpStatusPayload(
      status, packUInt32(0x55aa0011) + packUInt32(1234) +
                  packUInt32(5678) + packUInt32(9000) + packUInt32(100) +
                  packUInt32(200) + packUInt32(0x01020304) +
                  packUInt32(77) + packUInt16(4665) + packUInt16(4666) +
                  packUInt32(0x11223344) + packUInt16(0)));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa0011, status.challenge);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4665, status.udpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, status.tcpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x11223344, status.udpKey);

  std::string messagePayload = packUInt16(5) + "hello";
  std::string message;
  CPPUNIT_ASSERT(parseServerMessagePayload(message, messagePayload));
  CPPUNIT_ASSERT_EQUAL(std::string("hello"), message);

  std::string identPayload(16, '\x11');
  identPayload += packUInt32(0x04030201);
  identPayload += packUInt16(4661);
  identPayload += packUInt32(2);
  identPayload += createStringTag(0x01, "server name");
  identPayload += createStringTag(0x0b, "server description");
  identPayload += packUInt16(0);
  ServerIdent ident;
  CPPUNIT_ASSERT(parseServerIdentPayload(ident, identPayload));
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), ident.endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, ident.endpoint.port);
  CPPUNIT_ASSERT_EQUAL(std::string("server name"), ident.name);
  CPPUNIT_ASSERT_EQUAL(std::string("server description"), ident.description);

  std::vector<Endpoint> servers;
  std::string serverList;
  serverList.push_back('\x02');
  serverList += packUInt32(0x04030201);
  serverList += packUInt16(4661);
  serverList += packUInt32(0x08070605);
  serverList += packUInt16(4662);
  serverList += packUInt16(0);
  CPPUNIT_ASSERT(parseServerListPayload(servers, serverList));
  CPPUNIT_ASSERT_EQUAL((size_t)2, servers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), servers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, servers[0].port);
  CPPUNIT_ASSERT_EQUAL(std::string("5.6.7.8"), servers[1].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, servers[1].port);
}

void Ed2kHelperTest::testSearchResultPayload()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());

  std::string tags;
  tags += packUInt32(6);
  tags.push_back(static_cast<char>(0x02 | 0x80));
  tags.push_back('\x01');
  tags += packUInt16(9);
  tags += "video.mkv";
  tags.push_back(static_cast<char>(0x03 | 0x80));
  tags.push_back('\x02');
  tags += packUInt32(5);
  tags.push_back(static_cast<char>(0x03 | 0x80));
  tags.push_back('\x3a');
  tags += packUInt32(1);
  tags.push_back(static_cast<char>(0x02 | 0x80));
  tags.push_back('\x03');
  tags += packUInt16(5);
  tags += "Video";
  tags.push_back(static_cast<char>(0x03 | 0x80));
  tags.push_back('\x15');
  tags += packUInt32(42);
  tags.push_back(static_cast<char>(0x03 | 0x80));
  tags.push_back('\x30');
  tags += packUInt32(7);

  std::string payload;
  payload += packUInt32(1);
  payload += fileHash;
  payload += packUInt32(0x04030201);
  payload += packUInt16(4662);
  payload += tags;
  payload.push_back('\x01');

  SearchResult result;
  CPPUNIT_ASSERT(parseSearchResultPayload(result, payload, "server"));
  CPPUNIT_ASSERT(result.moreResults);
  CPPUNIT_ASSERT_EQUAL((size_t)1, result.entries.size());
  CPPUNIT_ASSERT_EQUAL(fileHash, result.entries[0].hash);
  CPPUNIT_ASSERT_EQUAL(std::string("video.mkv"), result.entries[0].name);
  CPPUNIT_ASSERT_EQUAL((int64_t)0x100000005LL, result.entries[0].size);
  CPPUNIT_ASSERT_EQUAL(std::string("Video"), result.entries[0].fileType);
  CPPUNIT_ASSERT_EQUAL((uint32_t)42, result.entries[0].sourceCount);
  CPPUNIT_ASSERT_EQUAL((uint32_t)7, result.entries[0].completeSourceCount);
  CPPUNIT_ASSERT_EQUAL(std::string("server"), result.entries[0].sourceNetwork);
  CPPUNIT_ASSERT_EQUAL((size_t)1, result.entries[0].sources.size());
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"),
                       result.entries[0].sources[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, result.entries[0].sources[0].port);
  CPPUNIT_ASSERT_EQUAL(
      std::string("ed2k://|file|video.mkv|4294967301|"
                  "0123456789abcdef0123456789abcdef|"
                  "sources,1.2.3.4:4662|/"),
      result.entries[0].ed2kLink);
}

void Ed2kHelperTest::testSearchRequestPayload()
{
  SearchQuery query;
  query.keyword = "ubuntu iso";
  query.fileType = "Pro";
  query.extension = "iso";
  query.minSize = 0x100000001LL;
  query.maxSize = 0x200000001LL;
  query.minSourceCount = 5;
  query.minCompleteSourceCount = 2;

  auto payload = createSearchRequestPayload(query, true);

  std::string expected;
  expected.push_back('\0');
  expected.push_back('\0');
  expected.push_back('\x01');
  expected += packUInt16(10);
  expected += "ubuntu iso";
  expected.push_back('\0');
  expected.push_back('\0');
  expected.push_back('\x02');
  expected += packUInt16(3);
  expected += "Pro";
  expected += packUInt16(1);
  expected.push_back('\x03');
  expected.push_back('\0');
  expected.push_back('\0');
  expected.push_back('\x08');
  expected += packUInt64(0x100000001LL);
  expected.push_back('\x01');
  expected += packUInt16(1);
  expected.push_back('\x02');
  expected.push_back('\0');
  expected.push_back('\0');
  expected.push_back('\x08');
  expected += packUInt64(0x200000001LL);
  expected.push_back('\x02');
  expected += packUInt16(1);
  expected.push_back('\x02');
  expected.push_back('\0');
  expected.push_back('\0');
  expected.push_back('\x03');
  expected += packUInt32(5);
  expected.push_back('\x01');
  expected += packUInt16(1);
  expected.push_back('\x15');
  expected.push_back('\0');
  expected.push_back('\0');
  expected.push_back('\x03');
  expected += packUInt32(2);
  expected.push_back('\x01');
  expected += packUInt16(1);
  expected.push_back('\x30');
  expected.push_back('\x02');
  expected += packUInt16(3);
  expected += "iso";
  expected += packUInt16(1);
  expected.push_back('\x04');

  CPPUNIT_ASSERT_EQUAL(util::toHex(expected), util::toHex(payload));

  auto clamped = createSearchRequestPayload(query, false);
  CPPUNIT_ASSERT_EQUAL(static_cast<char>(0x03), clamped[28]);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0xffffffffu, readUInt32(clamped.data() + 29));
}

void Ed2kHelperTest::testKadKeywordTarget()
{
  CPPUNIT_ASSERT_EQUAL(std::string("oxymoronaccelerator"),
                       pickKadKeyword("The oxymoronaccelerator 2"));
  CPPUNIT_ASSERT_EQUAL(std::string("ubuntu"),
                       pickKadKeyword("Ubuntu-22.04 ISO"));
  CPPUNIT_ASSERT_EQUAL(std::string(), pickKadKeyword("a 12 ()"));

  auto target = createKadKeywordTarget("The oxymoronaccelerator 2");
  CPPUNIT_ASSERT_EQUAL(
      std::string("bfdc1e49ecaa72c4f57ed35998a5a40d"),
      util::toHex(target));
}

void Ed2kHelperTest::testKadSearchEntriesToSearchResults()
{
  std::string fileIdHex("0123456789abcdef0123456789abcdef");
  auto fileId = util::fromHex(fileIdHex.begin(), fileIdHex.end());
  KadSearchEntry entry;
  entry.id = fileId;
  Tag name;
  name.id = 0x01;
  name.valueType = TagValueType::STRING;
  name.stringValue = "video.mkv";
  entry.tags.push_back(name);
  Tag sizeLow;
  sizeLow.id = 0x02;
  sizeLow.valueType = TagValueType::UINT;
  sizeLow.intValue = 5;
  entry.tags.push_back(sizeLow);
  Tag sizeHigh;
  sizeHigh.id = 0x3a;
  sizeHigh.valueType = TagValueType::UINT;
  sizeHigh.intValue = 1;
  entry.tags.push_back(sizeHigh);
  Tag fileType;
  fileType.id = 0x03;
  fileType.valueType = TagValueType::STRING;
  fileType.stringValue = "Video";
  entry.tags.push_back(fileType);
  Tag extension;
  extension.id = 0x04;
  extension.valueType = TagValueType::STRING;
  extension.stringValue = "mkv";
  entry.tags.push_back(extension);
  Tag sources;
  sources.id = 0x15;
  sources.valueType = TagValueType::UINT;
  sources.intValue = 3;
  entry.tags.push_back(sources);
  Tag complete;
  complete.id = 0x30;
  complete.valueType = TagValueType::UINT;
  complete.intValue = 2;
  entry.tags.push_back(complete);

  auto results = kadSearchEntriesToSearchResults(std::vector<KadSearchEntry>{entry},
                                                 "kad");

  CPPUNIT_ASSERT_EQUAL((size_t)1, results.size());
  CPPUNIT_ASSERT_EQUAL(fileId, results[0].hash);
  CPPUNIT_ASSERT_EQUAL(std::string("video.mkv"), results[0].name);
  CPPUNIT_ASSERT_EQUAL((int64_t)0x100000005LL, results[0].size);
  CPPUNIT_ASSERT_EQUAL(std::string("Video"), results[0].fileType);
  CPPUNIT_ASSERT_EQUAL(std::string("mkv"), results[0].extension);
  CPPUNIT_ASSERT_EQUAL((uint32_t)3, results[0].sourceCount);
  CPPUNIT_ASSERT_EQUAL((uint32_t)2, results[0].completeSourceCount);
  CPPUNIT_ASSERT_EQUAL(std::string("kad"), results[0].sourceNetwork);
  CPPUNIT_ASSERT_EQUAL(
      std::string("ed2k://|file|video.mkv|4294967301|"
                  "0123456789abcdef0123456789abcdef|/"),
      results[0].ed2kLink);
}

void Ed2kHelperTest::testSourceExchange2Payloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::string userHashHex("11111111111111111111111111111111");
  auto userHash = util::fromHex(userHashHex.begin(), userHashHex.end());

  auto request = createRequestSources2Payload(fileHash);
  CPPUNIT_ASSERT_EQUAL((size_t)19, request.size());
  CPPUNIT_ASSERT_EQUAL((uint8_t)4, static_cast<uint8_t>(request[0]));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, readUInt16(request.data() + 1));
  CPPUNIT_ASSERT_EQUAL(fileHash, request.substr(3, HASH_LENGTH));
  uint8_t requestVersion = 0;
  CPPUNIT_ASSERT(parseRequestSources2Payload(requestVersion, fileHash,
                                            fileHash));
  CPPUNIT_ASSERT_EQUAL((uint8_t)1, requestVersion);
  CPPUNIT_ASSERT(parseRequestSources2Payload(requestVersion, request,
                                            fileHash));
  CPPUNIT_ASSERT_EQUAL((uint8_t)4, requestVersion);

  SourceExchangeEntry entry;
  entry.endpoint.host = "203.0.113.9";
  entry.endpoint.port = 4662;
  entry.server.host = "198.51.100.2";
  entry.server.port = 4661;
  entry.userHash = userHash;
  entry.cryptOptions = 0x83;
  std::vector<SourceExchangeEntry> entries{entry};

  auto answer = createAnswerSources2Payload(fileHash, entries);
  SourceExchangeAnswer parsed;
  CPPUNIT_ASSERT(parseAnswerSources2Payload(parsed, answer, fileHash));
  CPPUNIT_ASSERT_EQUAL((uint8_t)4, parsed.version);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsed.entries.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.9"),
                       parsed.entries[0].endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, parsed.entries[0].endpoint.port);
  CPPUNIT_ASSERT_EQUAL(std::string("198.51.100.2"),
                       parsed.entries[0].server.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, parsed.entries[0].server.port);
  CPPUNIT_ASSERT_EQUAL(userHash, parsed.entries[0].userHash);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x83, parsed.entries[0].cryptOptions);

  auto sx1 = createAnswerSourcesPayload(fileHash, 1, entries);
  CPPUNIT_ASSERT(parseAnswerSourcesPayload(parsed, sx1, fileHash, 1));
  CPPUNIT_ASSERT_EQUAL((uint8_t)1, parsed.version);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsed.entries.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.9"),
                       parsed.entries[0].endpoint.host);
  CPPUNIT_ASSERT(parsed.entries[0].userHash.empty());

  auto sx4 = createAnswerSourcesPayload(fileHash, 4, entries);
  CPPUNIT_ASSERT(parseAnswerSourcesPayload(parsed, sx4, fileHash, 4));
  CPPUNIT_ASSERT_EQUAL((uint8_t)4, parsed.version);
  CPPUNIT_ASSERT_EQUAL(userHash, parsed.entries[0].userHash);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x83, parsed.entries[0].cryptOptions);
}

void Ed2kHelperTest::testCompressedPartPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());

  auto payload32 = fileHash + packUInt32(184320) + packUInt32(4) + "data";
  CompressedPartHeader header;
  std::string compressedData;
  CPPUNIT_ASSERT(parseCompressedPartPayload(header, compressedData, payload32,
                                           fileHash, false));
  CPPUNIT_ASSERT_EQUAL((int64_t)184320, header.begin);
  CPPUNIT_ASSERT_EQUAL((uint32_t)4, header.compressedLength);
  CPPUNIT_ASSERT_EQUAL(std::string("data"), compressedData);

  auto payload64 = fileHash + packUInt64(0x100000001LL) + packUInt32(2) + "xy";
  CPPUNIT_ASSERT(parseCompressedPartPayload(header, compressedData, payload64,
                                           fileHash, true));
  CPPUNIT_ASSERT_EQUAL((int64_t)0x100000001LL, header.begin);
  CPPUNIT_ASSERT_EQUAL((uint32_t)2, header.compressedLength);
  CPPUNIT_ASSERT_EQUAL(std::string("xy"), compressedData);

  auto bad = fileHash + packUInt32(0) + packUInt32(10) + "tiny";
  CPPUNIT_ASSERT(!parseCompressedPartPayload(header, compressedData, bad,
                                            fileHash, false));
}

void Ed2kHelperTest::testInflateCompressedPartData()
{
  std::string input;
  for (int i = 0; i < 8192; ++i) {
    input.push_back(static_cast<char>('A' + (i % 23)));
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  CPPUNIT_ASSERT_EQUAL(Z_OK, deflateInit(&strm, Z_DEFAULT_COMPRESSION));
  strm.avail_in = input.size();
  strm.next_in = reinterpret_cast<unsigned char*>(&input[0]);
  std::string compressed(compressBound(input.size()), '\0');
  strm.avail_out = compressed.size();
  strm.next_out = reinterpret_cast<unsigned char*>(&compressed[0]);
  CPPUNIT_ASSERT_EQUAL(Z_STREAM_END, deflate(&strm, Z_FINISH));
  compressed.resize(compressed.size() - strm.avail_out);
  CPPUNIT_ASSERT_EQUAL(Z_OK, deflateEnd(&strm));

  std::string inflated;
  CPPUNIT_ASSERT(inflateCompressedPartData(inflated, compressed, input.size()));
  CPPUNIT_ASSERT_EQUAL(input, inflated);

  CPPUNIT_ASSERT(!inflateCompressedPartData(inflated, compressed,
                                           input.size() - 1));
  CPPUNIT_ASSERT(!inflateCompressedPartData(inflated, "not zlib", input.size()));
}

void Ed2kHelperTest::testInflatePackedPacketPayload()
{
  std::string input;
  for (int i = 0; i < 2048; ++i) {
    input.push_back(static_cast<char>('a' + (i % 7)));
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  CPPUNIT_ASSERT_EQUAL(Z_OK, deflateInit(&strm, Z_DEFAULT_COMPRESSION));
  strm.avail_in = input.size();
  strm.next_in = reinterpret_cast<unsigned char*>(&input[0]);
  std::string compressed(compressBound(input.size()), '\0');
  strm.avail_out = compressed.size();
  strm.next_out = reinterpret_cast<unsigned char*>(&compressed[0]);
  CPPUNIT_ASSERT_EQUAL(Z_STREAM_END, deflate(&strm, Z_FINISH));
  compressed.resize(compressed.size() - strm.avail_out);
  CPPUNIT_ASSERT_EQUAL(Z_OK, deflateEnd(&strm));

  std::string inflated;
  CPPUNIT_ASSERT(inflatePackedPacketPayload(inflated, compressed,
                                            input.size() + 100));
  CPPUNIT_ASSERT_EQUAL(input, inflated);

  CPPUNIT_ASSERT(!inflatePackedPacketPayload(inflated, compressed,
                                            input.size() - 1));
  CPPUNIT_ASSERT(!inflatePackedPacketPayload(inflated, "not zlib", input.size()));
}

void Ed2kHelperTest::testEmuleInfoPayload()
{
  EmulePeerInfo info;
  info.version = 0x3c;
  info.protocolVersion = 0x01;
  info.miscOptions.aichVersion = 2;
  info.miscOptions.unicode = true;
  info.miscOptions.dataCompressionVersion = 1;
  info.miscOptions.sourceExchange1Version = 3;
  info.miscOptions.extendedRequestsVersion = 2;
  info.miscOptions.multiPacket = true;
  info.miscOptions2.supportsSourceExchange2 = true;
  info.miscOptions2.supportsLargeFiles = true;

  auto payload = createEmuleInfoPayload(info);
  std::vector<Tag> muleTags;
  CPPUNIT_ASSERT(parseTagList(muleTags, payload.substr(2)));
  auto hasUintTag = [&](uint8_t id) {
    return std::find_if(muleTags.begin(), muleTags.end(),
                        [&](const Tag& tag) {
                          return tag.id == id &&
                                 tag.valueType == TagValueType::UINT;
                        }) != muleTags.end();
  };
  CPPUNIT_ASSERT(hasUintTag(0x20));
  CPPUNIT_ASSERT(hasUintTag(0x21));
  CPPUNIT_ASSERT(hasUintTag(0x22));
  CPPUNIT_ASSERT(hasUintTag(0x23));
  CPPUNIT_ASSERT(hasUintTag(0x24));
  CPPUNIT_ASSERT(hasUintTag(0x25));
  CPPUNIT_ASSERT(hasUintTag(0x26));
  CPPUNIT_ASSERT(hasUintTag(0x27));
  CPPUNIT_ASSERT(!hasUintTag(0xfb));
  CPPUNIT_ASSERT(!hasUintTag(0xfa));
  CPPUNIT_ASSERT(!hasUintTag(0xfe));

  EmulePeerInfo parsed;
  CPPUNIT_ASSERT(parseEmuleInfoPayload(parsed, payload));
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x3c, parsed.version);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0x01, parsed.protocolVersion);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0, parsed.miscOptions.aichVersion);
  CPPUNIT_ASSERT(!parsed.miscOptions.unicode);
  CPPUNIT_ASSERT_EQUAL((uint8_t)1, parsed.miscOptions.dataCompressionVersion);
  CPPUNIT_ASSERT_EQUAL((uint8_t)3, parsed.miscOptions.sourceExchange1Version);
  CPPUNIT_ASSERT_EQUAL((uint8_t)2, parsed.miscOptions.extendedRequestsVersion);
  CPPUNIT_ASSERT(!parsed.miscOptions.multiPacket);
  CPPUNIT_ASSERT(!parsed.miscOptions2.supportsSourceExchange2);
  CPPUNIT_ASSERT(!parsed.miscOptions2.supportsLargeFiles);
}

void Ed2kHelperTest::testLocalEmulePeerInfoCapabilities()
{
  auto info = createLocalEmulePeerInfo();

  CPPUNIT_ASSERT_EQUAL((uint8_t)1, info.miscOptions.aichVersion);
  CPPUNIT_ASSERT(info.miscOptions.unicode);
  CPPUNIT_ASSERT_EQUAL((uint8_t)1,
                       info.miscOptions.dataCompressionVersion);
  CPPUNIT_ASSERT_EQUAL((uint8_t)3,
                       info.miscOptions.sourceExchange1Version);
  CPPUNIT_ASSERT_EQUAL((uint8_t)2,
                       info.miscOptions.extendedRequestsVersion);
  CPPUNIT_ASSERT(info.miscOptions2.supportsLargeFiles);
  CPPUNIT_ASSERT(info.miscOptions2.supportsSourceExchange2);
  CPPUNIT_ASSERT_EQUAL((uint8_t)0, info.miscOptions.secureIdentVersion);
  CPPUNIT_ASSERT(!info.miscOptions.multiPacket);
  CPPUNIT_ASSERT(!info.miscOptions2.supportsExtendedMultipacket);
}

void Ed2kHelperTest::testPeerHelloPayload()
{
  std::string clientHashHex("0123456789abcdef0123456789abcdef");
  auto clientHash = util::fromHex(clientHashHex.begin(), clientHashHex.end());
  EmulePeerInfo info;
  info.version = 0x47;
  info.miscOptions.aichVersion = 1;
  info.miscOptions.unicode = true;
  info.miscOptions.dataCompressionVersion = 1;
  info.miscOptions.sourceExchange1Version = 3;
  info.miscOptions.extendedRequestsVersion = 2;
  info.miscOptions.multiPacket = true;
  info.miscOptions2.supportsLargeFiles = true;
  info.miscOptions2.supportsSourceExchange2 = true;

  Endpoint server;
  server.host = "1.2.3.4";
  server.port = 4661;
  auto payload = createPeerHelloPayload(clientHash, 0x0a000001, 4662,
                                        server, "aria2-next", info, true);
  CPPUNIT_ASSERT_EQUAL((uint8_t)HASH_LENGTH,
                       static_cast<uint8_t>(payload[0]));
  CPPUNIT_ASSERT_EQUAL(clientHash, payload.substr(1, HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x0a000001,
                       readUInt32(payload.data() + 1 + HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662,
                       readUInt16(payload.data() + 1 + HASH_LENGTH + 4));

  const auto tagOffset = 1 + HASH_LENGTH + 4 + 2;
  std::vector<Tag> tags;
  CPPUNIT_ASSERT(parseTagList(tags, payload.substr(
                                        tagOffset,
                                        payload.size() - tagOffset - 6)));
  auto hasUintTag = [&](uint8_t id) {
    return std::find_if(tags.begin(), tags.end(), [&](const Tag& tag) {
             return tag.id == id && tag.valueType == TagValueType::UINT;
           }) != tags.end();
  };
  CPPUNIT_ASSERT(hasUintTag(0x11));
  CPPUNIT_ASSERT(hasUintTag(0xef));
  CPPUNIT_ASSERT(hasUintTag(0xf9));
  CPPUNIT_ASSERT(hasUintTag(0xfa));
  CPPUNIT_ASSERT(hasUintTag(0xfb));
  CPPUNIT_ASSERT(hasUintTag(0xfe));
  CPPUNIT_ASSERT_EQUAL(std::string("010203043512"),
                       util::toHex(payload.substr(payload.size() - 6)));

  EmulePeerInfo parsed;
  CPPUNIT_ASSERT(parsePeerHelloPayload(parsed, payload, true));
  CPPUNIT_ASSERT_EQUAL(clientHash, parsed.userHash);
  CPPUNIT_ASSERT_EQUAL((uint8_t)1, parsed.miscOptions.aichVersion);
  CPPUNIT_ASSERT(parsed.miscOptions.unicode);
  CPPUNIT_ASSERT_EQUAL((uint8_t)1, parsed.miscOptions.dataCompressionVersion);
  CPPUNIT_ASSERT_EQUAL((uint8_t)3, parsed.miscOptions.sourceExchange1Version);
  CPPUNIT_ASSERT_EQUAL((uint8_t)2, parsed.miscOptions.extendedRequestsVersion);
  CPPUNIT_ASSERT(parsed.miscOptions.multiPacket);
  CPPUNIT_ASSERT(parsed.miscOptions2.supportsLargeFiles);
  CPPUNIT_ASSERT(parsed.miscOptions2.supportsSourceExchange2);
}

void Ed2kHelperTest::testUdpReaskPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  auto ping = createUdpReaskFilePingPayload(fileHash, 7);
  UdpReask reask;
  CPPUNIT_ASSERT(parseUdpReaskFilePingPayload(reask, ping));
  CPPUNIT_ASSERT_EQUAL(fileHash, reask.fileHash);
  CPPUNIT_ASSERT(reask.hasCompleteSources);
  CPPUNIT_ASSERT_EQUAL((uint16_t)7, reask.completeSources);
  CPPUNIT_ASSERT(parseUdpReaskFilePingPayload(reask, fileHash));
  CPPUNIT_ASSERT(!reask.hasCompleteSources);

  auto ackPayload =
      createUdpReaskAckPayload(std::vector<bool>{true, false, true}, 42);
  UdpReaskAck ack;
  CPPUNIT_ASSERT(parseUdpReaskAckPayload(ack, ackPayload));
  CPPUNIT_ASSERT_EQUAL((size_t)3, ack.bitfield.size());
  CPPUNIT_ASSERT(ack.bitfield[0]);
  CPPUNIT_ASSERT(!ack.bitfield[1]);
  CPPUNIT_ASSERT(ack.bitfield[2]);
  CPPUNIT_ASSERT_EQUAL((uint16_t)42, ack.rank);
  CPPUNIT_ASSERT(parseUdpReaskAckPayload(ack, createUdpReaskAckPayload(3)));
  CPPUNIT_ASSERT(ack.bitfield.empty());
  CPPUNIT_ASSERT_EQUAL((uint16_t)3, ack.rank);
  CPPUNIT_ASSERT(!parseUdpReaskAckPayload(ack, std::string(3, '\0')));
}

void Ed2kHelperTest::testAichPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::string aichRootHex("1111111111111111111111111111111111111111");
  auto aichRoot = util::fromHex(aichRootHex.begin(), aichRootHex.end());

  auto fileHashRequest = createAichFileHashRequestPayload(fileHash);
  CPPUNIT_ASSERT_EQUAL(fileHash, fileHashRequest);

  AichFileHashAnswer fileHashAnswer;
  CPPUNIT_ASSERT(parseAichFileHashAnswerPayload(
      fileHashAnswer, createAichFileHashAnswerPayload(fileHash, aichRoot),
      fileHash));
  CPPUNIT_ASSERT_EQUAL(fileHash, fileHashAnswer.fileHash);
  CPPUNIT_ASSERT_EQUAL(aichRoot, fileHashAnswer.rootHash);

  auto request = createAichRequestPayload(fileHash, 7, aichRoot);
  AichRequest parsedRequest;
  CPPUNIT_ASSERT(parseAichRequestPayload(parsedRequest, request, fileHash));
  CPPUNIT_ASSERT_EQUAL(fileHash, parsedRequest.fileHash);
  CPPUNIT_ASSERT_EQUAL((uint16_t)7, parsedRequest.partIndex);
  CPPUNIT_ASSERT_EQUAL(aichRoot, parsedRequest.rootHash);

  std::string recovery;
  recovery += packUInt16(3);
  recovery += std::string(20, '\x22');
  auto answer = createAichAnswerPayload(fileHash, 7, aichRoot, recovery);
  AichAnswer parsedAnswer;
  CPPUNIT_ASSERT(parseAichAnswerPayload(parsedAnswer, answer, fileHash));
  CPPUNIT_ASSERT(!parsedAnswer.failed);
  CPPUNIT_ASSERT_EQUAL(fileHash, parsedAnswer.fileHash);
  CPPUNIT_ASSERT_EQUAL((uint16_t)7, parsedAnswer.partIndex);
  CPPUNIT_ASSERT_EQUAL(aichRoot, parsedAnswer.rootHash);
  CPPUNIT_ASSERT_EQUAL(recovery, parsedAnswer.recoveryData);

  CPPUNIT_ASSERT(parseAichAnswerPayload(parsedAnswer, fileHash, fileHash));
  CPPUNIT_ASSERT(parsedAnswer.failed);
  CPPUNIT_ASSERT_EQUAL(fileHash, parsedAnswer.fileHash);
  CPPUNIT_ASSERT(parsedAnswer.rootHash.empty());
  CPPUNIT_ASSERT(parsedAnswer.recoveryData.empty());

  CPPUNIT_ASSERT(!parseAichRequestPayload(parsedRequest, request,
                                          std::string(16, '\0')));
}

void Ed2kHelperTest::testAichRecoveryData()
{
  std::string block0(EMBLOCK_LENGTH, 'a');
  std::string block1(EMBLOCK_LENGTH, 'b');
  std::string block2(100, 'c');
  const auto hash0 = aichHash(block0);
  const auto hash1 = aichHash(block1);
  const auto hash2 = aichHash(block2);
  const auto data = block0 + block1 + block2;
  const auto root = aichRootHash(data.data(), data.size());
  std::string recovery;
  recovery += packUInt16(3);
  recovery += packUInt16(7);
  recovery += hash0;
  recovery += packUInt16(6);
  recovery += hash1;
  recovery += packUInt16(2);
  recovery += hash2;
  recovery += packUInt16(0);

  AichRecoveryData parsed;
  CPPUNIT_ASSERT(parseAichRecoveryData(parsed, recovery,
                                       block0.size() + block1.size() +
                                           block2.size(),
                                       false));
  CPPUNIT_ASSERT(verifyAichRecoveryData(
      parsed, root, block0.size() + block1.size() + block2.size(), 0));
  AichRecoverySet recoverySet;
  CPPUNIT_ASSERT(buildAichRecoverySet(
      recoverySet, parsed, root,
      block0.size() + block1.size() + block2.size(), 0));
  CPPUNIT_ASSERT_EQUAL((size_t)3, recoverySet.blocks.size());
  CPPUNIT_ASSERT_EQUAL(hash1, recoverySet.blocks[1].hash);
  recovery[4] ^= 0x01;
  CPPUNIT_ASSERT(parseAichRecoveryData(parsed, recovery,
                                       block0.size() + block1.size() +
                                           block2.size(),
                                       false));
  CPPUNIT_ASSERT(!verifyAichRecoveryData(
      parsed, root, block0.size() + block1.size() + block2.size(), 0));
}

void Ed2kHelperTest::testAichHashTree()
{
  std::string data;
  for (int i = 0; i < 450000; ++i) {
    data.push_back(static_cast<char>('a' + (i % 26)));
  }

  auto firstBlock = aichHash(data.data(), EMBLOCK_LENGTH);
  auto secondBlock =
      aichHash(data.data() + EMBLOCK_LENGTH, EMBLOCK_LENGTH);
  auto thirdBlock =
      aichHash(data.data() + EMBLOCK_LENGTH * 2,
               data.size() - EMBLOCK_LENGTH * 2);
  CPPUNIT_ASSERT_EQUAL((size_t)40, util::toHex(firstBlock).size());
  CPPUNIT_ASSERT_EQUAL(firstBlock, aichRootHash(data.data(), EMBLOCK_LENGTH));
  auto left = aichHash(firstBlock + secondBlock);
  CPPUNIT_ASSERT_EQUAL(aichHash(left + thirdBlock),
                       aichRootHash(data.data(), data.size()));

  std::vector<std::string> leaves{firstBlock, secondBlock, thirdBlock};
  CPPUNIT_ASSERT_EQUAL(aichHash(left + thirdBlock), aichRootHash(leaves));

  CPPUNIT_ASSERT_THROW(aichRootHash(std::vector<std::string>{std::string(19, '\0')}),
                       RecoverableException);
}

void Ed2kHelperTest::testAichHashTreeKeepsPartLevel()
{
  std::string firstPart(PIECE_LENGTH, 'a');
  std::string secondPart(EMBLOCK_LENGTH, 'b');
  const auto expected =
      aichHash(aichRootHash(firstPart.data(), firstPart.size()) +
               aichRootHash(secondPart.data(), secondPart.size()));
  const auto data = firstPart + secondPart;
  const auto flatRoot = aichRootHash(data.data(), data.size());

  CPPUNIT_ASSERT_EQUAL(expected, flatRoot);
}

void Ed2kHelperTest::testKadPacketPayloads()
{
  std::string nodeIdHex("0123456789abcdef0123456789abcdef");
  auto nodeId = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  KadContact contact;
  contact.id = nodeId;
  contact.host = "203.0.113.9";
  contact.udpPort = 4672;
  contact.tcpPort = 4662;
  contact.version = 5;

  auto hello = createKadHelloPayload(nodeId, 4662, 5);
  KadHello parsedHello;
  CPPUNIT_ASSERT(parseKadHelloPayload(parsedHello, hello));
  CPPUNIT_ASSERT_EQUAL(nodeId, parsedHello.id);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, parsedHello.tcpPort);
  CPPUNIT_ASSERT_EQUAL((uint8_t)5, parsedHello.version);

  auto bootstrap = createKadBootstrapResponsePayload(nodeId, 4662, 5,
                                                     std::vector<KadContact>{contact});
  KadBootstrapResponse parsedBootstrap;
  CPPUNIT_ASSERT(parseKadBootstrapResponsePayload(parsedBootstrap, bootstrap));
  CPPUNIT_ASSERT_EQUAL(nodeId, parsedBootstrap.id);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, parsedBootstrap.tcpPort);
  CPPUNIT_ASSERT_EQUAL((uint8_t)5, parsedBootstrap.version);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsedBootstrap.contacts.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.9"),
                       parsedBootstrap.contacts[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672, parsedBootstrap.contacts[0].udpPort);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, parsedBootstrap.contacts[0].tcpPort);

  auto req = createKadRequestPayload(KAD_FIND_NODE, nodeId, nodeId);
  KadRequest parsedReq;
  CPPUNIT_ASSERT(parseKadRequestPayload(parsedReq, req));
  CPPUNIT_ASSERT_EQUAL((uint8_t)KAD_FIND_NODE, parsedReq.searchType);
  CPPUNIT_ASSERT_EQUAL(nodeId, parsedReq.targetId);
  CPPUNIT_ASSERT_EQUAL(nodeId, parsedReq.receiverId);

  auto res = createKadResponsePayload(nodeId, std::vector<KadContact>{contact});
  KadResponse parsedRes;
  CPPUNIT_ASSERT(parseKadResponsePayload(parsedRes, res));
  CPPUNIT_ASSERT_EQUAL(nodeId, parsedRes.targetId);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsedRes.contacts.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.9"), parsedRes.contacts[0].host);
}

void Ed2kHelperTest::testKadSearchPublishAndFirewallPayloads()
{
  std::string fileIdHex("0123456789abcdef0123456789abcdef");
  auto fileId = util::fromHex(fileIdHex.begin(), fileIdHex.end());
  std::string sourceIdHex("11111111111111111111111111111111");
  auto sourceId = util::fromHex(sourceIdHex.begin(), sourceIdHex.end());

  auto sourceReq = createKadSearchSourcesRequestPayload(fileId, 3, 123456789);
  KadSearchSourcesRequest parsedSourceReq;
  CPPUNIT_ASSERT(parseKadSearchSourcesRequestPayload(parsedSourceReq,
                                                    sourceReq));
  CPPUNIT_ASSERT_EQUAL(fileId, parsedSourceReq.targetId);
  CPPUNIT_ASSERT_EQUAL((uint16_t)3, parsedSourceReq.startPosition);
  CPPUNIT_ASSERT_EQUAL((uint64_t)123456789, parsedSourceReq.size);

  auto keyReq = createKadSearchKeysRequestPayload(fileId, 7);
  CPPUNIT_ASSERT_EQUAL((size_t)18, keyReq.size());
  CPPUNIT_ASSERT_EQUAL((uint16_t)7, readUInt16(keyReq.data() + HASH_LENGTH));

  Endpoint source;
  source.host = "203.0.113.9";
  source.port = 4662;
  auto publish = createKadPublishSourceRequestPayload(fileId, source, sourceId);
  KadPublishSourceRequest parsedPublish;
  CPPUNIT_ASSERT(parseKadPublishSourceRequestPayload(parsedPublish, publish));
  CPPUNIT_ASSERT_EQUAL(fileId, parsedPublish.fileId);
  auto largePublish = createKadPublishSourceRequestPayload(
      fileId, source, sourceId, 0x100000001ULL);
  CPPUNIT_ASSERT(parseKadPublishSourceRequestPayload(parsedPublish,
                                                     largePublish));
  auto sizeTag = std::find_if(parsedPublish.source.tags.begin(),
                              parsedPublish.source.tags.end(),
                              [](const Tag& tag) { return tag.id == 0xd3; });
  CPPUNIT_ASSERT(sizeTag != parsedPublish.source.tags.end());
  CPPUNIT_ASSERT_EQUAL((uint64_t)0x100000001ULL, sizeTag->intValue);

  auto searchRes = createKadSearchResultPayload(
      sourceId, fileId,
      std::vector<KadSearchEntry>{parsedPublish.source});
  KadSearchResult result;
  CPPUNIT_ASSERT(parseKadSearchResultPayload(result, searchRes));
  CPPUNIT_ASSERT_EQUAL(sourceId, result.sourceId);
  CPPUNIT_ASSERT_EQUAL(fileId, result.targetId);
  auto endpoints = extractKadSourceEndpoints(result);
  CPPUNIT_ASSERT_EQUAL((size_t)1, endpoints.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.9"), endpoints[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, endpoints[0].port);

  KadPublishResult publishResult;
  CPPUNIT_ASSERT(parseKadPublishResultPayload(
      publishResult, createKadPublishResultPayload(fileId, 1)));
  CPPUNIT_ASSERT_EQUAL(fileId, publishResult.fileId);
  CPPUNIT_ASSERT_EQUAL((uint8_t)1, publishResult.count);

  KadFirewalledRequest fwReq;
  CPPUNIT_ASSERT(parseKadFirewalledRequestPayload(
      fwReq, createKadFirewalledRequestPayload(4662, sourceId, 2)));
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, fwReq.tcpPort);
  CPPUNIT_ASSERT_EQUAL(sourceId, fwReq.id);
  CPPUNIT_ASSERT_EQUAL((uint8_t)2, fwReq.options);

  KadFirewalledResponse fwRes;
  CPPUNIT_ASSERT(parseKadFirewalledResponsePayload(
      fwRes, createKadFirewalledResponsePayload("203.0.113.9")));
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.9"), fwRes.ipAddress);

  KadFirewalledUdp fwUdp;
  CPPUNIT_ASSERT(parseKadFirewalledUdpPayload(
      fwUdp, createKadFirewalledUdpPayload(1, 4662)));
  CPPUNIT_ASSERT_EQUAL((uint8_t)1, fwUdp.errorCode);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4662, fwUdp.tcpPort);
}

void Ed2kHelperTest::testKadRoutingStatePayload()
{
  std::string selfIdHex("23a8ceff57a7a32d562d649ed7893796");
  auto selfId = util::fromHex(selfIdHex.begin(), selfIdHex.end());
  KadRoutingSnapshot snapshot;
  snapshot.selfId = selfId;
  snapshot.lastBootstrap = 100;
  snapshot.lastRefresh = 200;
  snapshot.lastSelfRefresh = 300;
  snapshot.lastFirewalledCheck = 500;
  snapshot.lastSourcePublish = 600;
  snapshot.firewalled = false;
  snapshot.observedAddresses.push_back("203.0.113.55");
  Endpoint router;
  router.host = "203.0.113.1";
  router.port = 4672;
  snapshot.routerNodes.push_back(router);
  snapshot.buckets.resize(2);
  snapshot.buckets[1].lastActive = 400;
  KadRoutingNode node;
  std::string nodeIdHex("31d6cfe0d16ae931b73c59d7e0c089c0");
  node.contact.id = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  node.contact.host = "198.51.100.2";
  node.contact.udpPort = 4672;
  node.contact.tcpPort = 4662;
  node.contact.version = 8;
  node.confirmed = true;
  node.seed = true;
  node.failCount = 3;
  node.firstSeen = 50;
  node.lastSeen = 75;
  snapshot.buckets[1].live.push_back(node);
  node.contact.host = "198.51.100.3";
  node.confirmed = false;
  node.seed = false;
  snapshot.buckets[1].replacements.push_back(node);

  auto payload = createKadRoutingStatePayload(snapshot);
  KadRoutingSnapshot parsed;
  CPPUNIT_ASSERT(parseKadRoutingStatePayload(parsed, payload));
  CPPUNIT_ASSERT_EQUAL(selfId, parsed.selfId);
  CPPUNIT_ASSERT_EQUAL((int64_t)100, parsed.lastBootstrap);
  CPPUNIT_ASSERT_EQUAL((int64_t)200, parsed.lastRefresh);
  CPPUNIT_ASSERT_EQUAL((int64_t)300, parsed.lastSelfRefresh);
  CPPUNIT_ASSERT_EQUAL((int64_t)500, parsed.lastFirewalledCheck);
  CPPUNIT_ASSERT_EQUAL((int64_t)600, parsed.lastSourcePublish);
  CPPUNIT_ASSERT(!parsed.firewalled);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsed.observedAddresses.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.55"),
                       parsed.observedAddresses[0]);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsed.routerNodes.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.1"), parsed.routerNodes[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672, parsed.routerNodes[0].port);
  CPPUNIT_ASSERT_EQUAL((size_t)2, parsed.buckets.size());
  CPPUNIT_ASSERT_EQUAL((int64_t)400, parsed.buckets[1].lastActive);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsed.buckets[1].live.size());
  CPPUNIT_ASSERT(parsed.buckets[1].live[0].confirmed);
  CPPUNIT_ASSERT(parsed.buckets[1].live[0].seed);
  CPPUNIT_ASSERT_EQUAL((uint32_t)3, parsed.buckets[1].live[0].failCount);
  CPPUNIT_ASSERT_EQUAL((int64_t)50, parsed.buckets[1].live[0].firstSeen);
  CPPUNIT_ASSERT_EQUAL((int64_t)75, parsed.buckets[1].live[0].lastSeen);
  CPPUNIT_ASSERT_EQUAL(std::string("198.51.100.2"),
                       parsed.buckets[1].live[0].contact.host);
  CPPUNIT_ASSERT_EQUAL((size_t)1, parsed.buckets[1].replacements.size());
  CPPUNIT_ASSERT(!parsed.buckets[1].replacements[0].confirmed);
  CPPUNIT_ASSERT_EQUAL(std::string("198.51.100.3"),
                       parsed.buckets[1].replacements[0].contact.host);

  CPPUNIT_ASSERT(!parseKadRoutingStatePayload(
      parsed, payload.substr(0, payload.size() - 1)));
}

void Ed2kHelperTest::testServerStatePayload()
{
  ServerState state;
  Endpoint server;
  server.host = "203.0.113.10";
  server.port = 4661;
  state.endpoint = server;
  state.name = "Peer Server";
  state.description = "Primary ED2K server";
  state.connected = true;
  state.handshakeCompleted = true;
  state.clientId = 0x0a000001;
  state.highId = true;
  state.ipAddress = "1.0.0.10";
  state.tcpFlags = 0x55aa;
  state.users = 1234;
  state.files = 5678;
  state.maxUsers = 9000;
  state.softFiles = 100;
  state.hardFiles = 200;
  state.udpFlags = 0x01020304;
  state.lowIdUsers = 77;
  state.udpObfuscationPort = 4665;
  state.tcpObfuscationPort = 4666;
  state.udpKey = 0x11223344;
  state.udpStatusChallenge = 0x55aa0011;
  state.lastUdpStatusTime = 120;
  state.nextSourceRequestTime = 180;
  state.failCount = 2;
  state.lastFailureTime = 100;
  state.nextRetryTime = 160;
  state.lastMessage = "hello";

  auto payload = createServerStatePayload(state);
  ServerState parsed;
  CPPUNIT_ASSERT(parseServerStatePayload(parsed, payload));
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.10"), parsed.endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, parsed.endpoint.port);
  CPPUNIT_ASSERT_EQUAL(std::string("Peer Server"), parsed.name);
  CPPUNIT_ASSERT_EQUAL(std::string("Primary ED2K server"),
                       parsed.description);
  CPPUNIT_ASSERT(parsed.connected);
  CPPUNIT_ASSERT(parsed.handshakeCompleted);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x0a000001, parsed.clientId);
  CPPUNIT_ASSERT(parsed.highId);
  CPPUNIT_ASSERT_EQUAL(std::string("1.0.0.10"), parsed.ipAddress);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa, parsed.tcpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1234, parsed.users);
  CPPUNIT_ASSERT_EQUAL((uint32_t)5678, parsed.files);
  CPPUNIT_ASSERT_EQUAL((uint32_t)9000, parsed.maxUsers);
  CPPUNIT_ASSERT_EQUAL((uint32_t)100, parsed.softFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)200, parsed.hardFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x01020304, parsed.udpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)77, parsed.lowIdUsers);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4665, parsed.udpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4666, parsed.tcpObfuscationPort);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x11223344, parsed.udpKey);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x55aa0011, parsed.udpStatusChallenge);
  CPPUNIT_ASSERT_EQUAL((int64_t)120, parsed.lastUdpStatusTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)180, parsed.nextSourceRequestTime);
  CPPUNIT_ASSERT_EQUAL((uint32_t)2, parsed.failCount);
  CPPUNIT_ASSERT_EQUAL((int64_t)100, parsed.lastFailureTime);
  CPPUNIT_ASSERT_EQUAL((int64_t)160, parsed.nextRetryTime);
  CPPUNIT_ASSERT_EQUAL(std::string("hello"), parsed.lastMessage);

  std::string v1Payload = payload;
  v1Payload.replace(sizeof("A2ED2KSRV") - 1, 4, packUInt32(1));
  v1Payload.erase(sizeof("A2ED2KSRV") - 1 + 4 + 2 + server.host.size() + 2,
                  2 + state.name.size() + 2 + state.description.size());
  v1Payload.erase(sizeof("A2ED2KSRV") - 1 + 4 + 2 + server.host.size() + 2 +
                  2 + 4 + 1 + 2 + state.ipAddress.size() + 4 + 4 + 4 + 4 +
                  4 + 4 + 4 + 4 + 2 + 2 + 4 + 4 + 8,
                  8);
  CPPUNIT_ASSERT(parseServerStatePayload(parsed, v1Payload));
  CPPUNIT_ASSERT_EQUAL((int64_t)0, parsed.nextSourceRequestTime);
  CPPUNIT_ASSERT(parsed.name.empty());
  CPPUNIT_ASSERT(parsed.description.empty());

  CPPUNIT_ASSERT(!parseServerStatePayload(parsed,
                                          payload.substr(0, payload.size() - 1)));
}

void Ed2kHelperTest::testNodesDatParser()
{
  std::string nodeIdHex("23a8ceff57a7a32d562d649ed7893796");
  auto nodeId = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  KadContact contact;
  contact.id = nodeId;
  contact.host = "203.0.113.1";
  contact.udpPort = 4672;
  contact.tcpPort = 4661;
  contact.version = 8;
  KadContact invalid = contact;
  invalid.host = "0.0.0.0";

  std::string payload;
  payload += packUInt32(0);
  payload += packUInt32(3);
  payload += packUInt32(1);
  payload += packUInt32(1);
  payload += createKadResponsePayload(nodeId, std::vector<KadContact>{contact})
                 .substr(HASH_LENGTH + 1);

  NodesDat nodes;
  CPPUNIT_ASSERT(parseNodesDat(nodes, payload));
  CPPUNIT_ASSERT_EQUAL((uint32_t)3, nodes.version);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1, nodes.bootstrapEdition);
  CPPUNIT_ASSERT_EQUAL((size_t)1, nodes.contacts.size());
  CPPUNIT_ASSERT_EQUAL(std::string("203.0.113.1"), nodes.contacts[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4672, nodes.contacts[0].udpPort);
  CPPUNIT_ASSERT_EQUAL((size_t)1, nodes.verified.size());
  CPPUNIT_ASSERT(nodes.verified[0]);

  std::string normal;
  normal += packUInt32(0);
  normal += packUInt32(2);
  normal += packUInt32(2);
  normal += createKadResponsePayload(nodeId,
                                     std::vector<KadContact>{contact, invalid})
                .substr(HASH_LENGTH + 1);
  normal += packUInt64(0);
  normal.push_back('\0');
  normal += packUInt64(0);
  normal.push_back('\x01');
  CPPUNIT_ASSERT(parseNodesDat(nodes, normal));
  CPPUNIT_ASSERT_EQUAL((uint32_t)2, nodes.version);
  CPPUNIT_ASSERT_EQUAL((size_t)1, nodes.verified.size());
  CPPUNIT_ASSERT(nodes.verified[0]);

  CPPUNIT_ASSERT(!parseNodesDat(nodes, packUInt32(0) + packUInt32(2) +
                                           packUInt32(0xffffffffu)));
}

void Ed2kHelperTest::testServerMetParser()
{
  std::string data;
  data.push_back('\x0e');
  data += packUInt32(1);
  data += packUInt32(0x04030201);
  data += packUInt16(4661);
  data += packUInt32(8);
  data.push_back('\x82');
  data.push_back('\x01');
  data += packUInt16(11);
  data += "Peer Server";
  data.push_back('\x82');
  data.push_back('\x0b');
  data += packUInt16(19);
  data += "Primary ED2K server";
  data += createUInt32Tag(0x87, 9000);
  data += createUInt32Tag(0x88, 100);
  data += createUInt32Tag(0x89, 200);
  data += createUInt32Tag(0x92, 0x01020304);
  data += createUInt32Tag(0x94, 77);
  data += createUInt32Tag(0x95, 0x11223344);

  auto servers = parseServerMet(data);

  CPPUNIT_ASSERT_EQUAL((size_t)1, servers.size());
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), servers[0].host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, servers[0].port);

  auto entries = parseServerMetEntries(data);

  CPPUNIT_ASSERT_EQUAL((size_t)1, entries.size());
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), entries[0].endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, entries[0].endpoint.port);
  CPPUNIT_ASSERT_EQUAL(std::string("Peer Server"), entries[0].name);
  CPPUNIT_ASSERT_EQUAL(std::string("Primary ED2K server"),
                       entries[0].description);
  CPPUNIT_ASSERT_EQUAL((uint32_t)9000, entries[0].maxUsers);
  CPPUNIT_ASSERT_EQUAL((uint32_t)100, entries[0].softFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)200, entries[0].hardFiles);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x01020304, entries[0].udpFlags);
  CPPUNIT_ASSERT_EQUAL((uint32_t)77, entries[0].lowIdUsers);
  CPPUNIT_ASSERT_EQUAL((uint32_t)0x11223344, entries[0].udpKey);

  data[0] = static_cast<char>(0xe0);
  entries = parseServerMetEntries(data);

  CPPUNIT_ASSERT_EQUAL((size_t)1, entries.size());
  CPPUNIT_ASSERT_EQUAL(std::string("1.2.3.4"), entries[0].endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, entries[0].endpoint.port);
  CPPUNIT_ASSERT_EQUAL(std::string("Peer Server"), entries[0].name);

  std::string hostnameEntry;
  hostnameEntry.push_back('\x0e');
  hostnameEntry += packUInt32(1);
  hostnameEntry += packUInt32(0);
  hostnameEntry += packUInt16(4661);
  hostnameEntry += packUInt32(2);
  hostnameEntry += createStringTag(0x85, "peer.example.org");
  hostnameEntry += createStringTag(0x01, "Hostname Server");

  entries = parseServerMetEntries(hostnameEntry);

  CPPUNIT_ASSERT_EQUAL((size_t)1, entries.size());
  CPPUNIT_ASSERT_EQUAL(std::string("peer.example.org"),
                       entries[0].endpoint.host);
  CPPUNIT_ASSERT_EQUAL((uint16_t)4661, entries[0].endpoint.port);
  CPPUNIT_ASSERT_EQUAL(std::string("Hostname Server"), entries[0].name);
}

void Ed2kHelperTest::testMd4Digest()
{
  CPPUNIT_ASSERT_EQUAL(
      std::string("31d6cfe0d16ae931b73c59d7e0c089c0"),
      util::toHex(md4Digest("")));
  CPPUNIT_ASSERT_EQUAL(
      std::string("bde52cb31de33e46245e05fbdbd6fb24"),
      util::toHex(md4Digest("a")));
  CPPUNIT_ASSERT_EQUAL(
      std::string("a448017aaf21d8525fc10ae87aa6729d"),
      util::toHex(md4Digest("abc")));
  CPPUNIT_ASSERT_EQUAL(
      std::string("d9130a8164549fe818874806e1c7014b"),
      util::toHex(md4Digest("message digest")));
}

void Ed2kHelperTest::testRootHash()
{
  std::vector<std::string> empty;
  CPPUNIT_ASSERT_EQUAL(
      std::string("31d6cfe0d16ae931b73c59d7e0c089c0"),
      util::toHex(rootHash(empty)));

  std::vector<std::string> single{md4Digest("abc")};
  CPPUNIT_ASSERT_EQUAL(util::toHex(md4Digest("abc")), util::toHex(rootHash(single)));

  std::vector<std::string> multiple{md4Digest("first part"),
                                    md4Digest("second part")};
  std::string concat = multiple[0] + multiple[1];
  CPPUNIT_ASSERT_EQUAL(util::toHex(md4Digest(concat)),
                       util::toHex(rootHash(multiple)));
}

} // namespace ed2k

} // namespace aria2
