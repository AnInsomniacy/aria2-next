#include "ed2k_helper.h"
#include "ed2k_endpoint.h"
#include "Ed2kKadState.h"

#include <algorithm>
#include "a2doctest.h"
#include <cstring>
#include <zlib.h>

#include "Exception.h"
#include "base32.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

class Ed2kHelperTest {


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
  void testKadSourceEndpointPreservesUdpAndCryptMetadata();
  void testSourceExchange2Payloads();
  void testMultipacketPayloads();
  void testCompressedPartPayloads();
  void testInflateCompressedPartData();
  void testCompressedPartInflaterKeepsBlockOwnerAcrossChunks();
  void testInflatePackedPacketPayload();
  void testEmuleInfoPayload();
  void testLocalEmulePeerInfoCapabilities();
  void testPeerHelloPayload();
  void testUdpReaskPayloads();
  void testAichPayloads();
  void testAichRecoveryData();
  void testAichHashTree();
  void testAichHashTreeKeepsPartLevel();
  void testKadUInt128ConversionMatchesAMule();
  void testKadPacketPayloads();
  void testKadDirectCallbackPayload();
  void testKadBuddyCallbackPayload();
  void testKadObfuscatedPacketRoundTrip();
  void testKadSearchPublishAndFirewallPayloads();
  void testKadRoutingStatePayload();
  void testServerStatePayload();
  void testNodesDatParser();
  void testServerMetParser();
  void testMd4Digest();
  void testRootHash();
  void testHashSetPartCount();
};

A2_TEST(Ed2kHelperTest, testParseFileLink)
A2_TEST(Ed2kHelperTest, testParseFileLinkWithOptions)
A2_TEST(Ed2kHelperTest, testParseFileLinkWithSourceCryptOptions)
A2_TEST(Ed2kHelperTest, testParseServerLink)
A2_TEST(Ed2kHelperTest, testParseSearchLink)
A2_TEST(Ed2kHelperTest, testParseRejectsMalformedLinks)
A2_TEST(Ed2kHelperTest, testSerializeFileLink)
A2_TEST(Ed2kHelperTest, testPacketHelpers)
A2_TEST(Ed2kHelperTest, testTagParser)
A2_TEST(Ed2kHelperTest, testProtocolPayloads)
A2_TEST(Ed2kHelperTest, testServerPayloadParsers)
A2_TEST(Ed2kHelperTest, testSearchRequestPayload)
A2_TEST(Ed2kHelperTest, testSearchResultPayload)
A2_TEST(Ed2kHelperTest, testKadKeywordTarget)
A2_TEST(Ed2kHelperTest, testKadSearchEntriesToSearchResults)
A2_TEST(Ed2kHelperTest, testKadSourceEndpointPreservesUdpAndCryptMetadata)
A2_TEST(Ed2kHelperTest, testSourceExchange2Payloads)
A2_TEST(Ed2kHelperTest, testMultipacketPayloads)
A2_TEST(Ed2kHelperTest, testCompressedPartPayloads)
A2_TEST(Ed2kHelperTest, testInflateCompressedPartData)
A2_TEST(Ed2kHelperTest, testCompressedPartInflaterKeepsBlockOwnerAcrossChunks)
A2_TEST(Ed2kHelperTest, testInflatePackedPacketPayload)
A2_TEST(Ed2kHelperTest, testEmuleInfoPayload)
A2_TEST(Ed2kHelperTest, testLocalEmulePeerInfoCapabilities)
A2_TEST(Ed2kHelperTest, testPeerHelloPayload)
A2_TEST(Ed2kHelperTest, testUdpReaskPayloads)
A2_TEST(Ed2kHelperTest, testAichPayloads)
A2_TEST(Ed2kHelperTest, testAichRecoveryData)
A2_TEST(Ed2kHelperTest, testAichHashTree)
A2_TEST(Ed2kHelperTest, testAichHashTreeKeepsPartLevel)
A2_TEST(Ed2kHelperTest, testKadUInt128ConversionMatchesAMule)
A2_TEST(Ed2kHelperTest, testKadPacketPayloads)
A2_TEST(Ed2kHelperTest, testKadDirectCallbackPayload)
A2_TEST(Ed2kHelperTest, testKadBuddyCallbackPayload)
A2_TEST(Ed2kHelperTest, testKadObfuscatedPacketRoundTrip)
A2_TEST(Ed2kHelperTest, testKadSearchPublishAndFirewallPayloads)
A2_TEST(Ed2kHelperTest, testKadRoutingStatePayload)
A2_TEST(Ed2kHelperTest, testServerStatePayload)
A2_TEST(Ed2kHelperTest, testNodesDatParser)
A2_TEST(Ed2kHelperTest, testServerMetParser)
A2_TEST(Ed2kHelperTest, testMd4Digest)
A2_TEST(Ed2kHelperTest, testRootHash)
A2_TEST(Ed2kHelperTest, testHashSetPartCount)

void Ed2kHelperTest::testParseFileLink()
{
  auto link = parseLink(
      "ed2k://|file|aria2%20next.bin|12345|"
      "0123456789ABCDEF0123456789ABCDEF|/");

  REQUIRE_EQ(LinkType::FILE, link.type);
  REQUIRE_EQ(std::string("aria2 next.bin"), link.name);
  REQUIRE_EQ((int64_t)12345, link.size);
  REQUIRE_EQ(
      std::string("0123456789abcdef0123456789abcdef"),
      util::toHex(link.hash));

  auto unsafeName = parseLink(
      "ed2k://|file|aria2%2Fnext%5Ctest.bin|12345|"
      "0123456789ABCDEF0123456789ABCDEF|/");
  REQUIRE_EQ(std::string("aria2_next_test.bin"), unsafeName.name);

  auto encodedSeparators = parseLink(
      "ed2k://%7Cfile%7Caria2%20next.bin%7C12345%7C"
      "0123456789ABCDEF0123456789ABCDEF%7C/");
  REQUIRE_EQ(LinkType::FILE, encodedSeparators.type);
  REQUIRE_EQ(std::string("aria2 next.bin"), encodedSeparators.name);
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

  REQUIRE_EQ((size_t)2, link.pieceHashes.size());
  REQUIRE_EQ(std::string(16, '\x11'), link.pieceHashes[0]);
  REQUIRE_EQ(std::string(16, '\x22'), link.pieceHashes[1]);
  std::string aichRoot("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
  REQUIRE_EQ(base32::decode(aichRoot.begin(), aichRoot.end()),
                       link.aichHash);
  REQUIRE_EQ((size_t)2, link.sources.size());
  REQUIRE_EQ(std::string("192.0.2.1"), link.sources[0].host);
  REQUIRE_EQ((uint16_t)4662, link.sources[0].port);
  REQUIRE_EQ(std::string("198.51.100.7"), link.sources[1].host);
  REQUIRE_EQ((uint16_t)7777, link.sources[1].port);
}

void Ed2kHelperTest::testParseFileLinkWithSourceCryptOptions()
{
  auto link = parseLink(
      "ed2k://|file|shared.bin|123|"
      "0123456789abcdef0123456789abcdef|/|"
      "sources,203.0.113.1:4662:131:"
      "11111111111111111111111111111111,"
      "peer.example.test:7777:1|/");

  REQUIRE_EQ((size_t)2, link.sources.size());
  REQUIRE_EQ(std::string("203.0.113.1"), link.sources[0].host);
  REQUIRE_EQ((uint16_t)4662, link.sources[0].port);
  REQUIRE_EQ((uint16_t)131, link.sources[0].cryptOptions);
  REQUIRE_EQ(std::string(16, '\x11'), link.sources[0].userHash);
  REQUIRE_EQ(std::string("peer.example.test"), link.sources[1].host);
  REQUIRE_EQ((uint16_t)7777, link.sources[1].port);
  REQUIRE_EQ((uint16_t)1, link.sources[1].cryptOptions);
  REQUIRE(link.sources[1].userHash.empty());

  auto reparsed = parseLink(toFileLink(link));
  REQUIRE_EQ((uint16_t)131, reparsed.sources[0].cryptOptions);
  REQUIRE_EQ(std::string(16, '\x11'), reparsed.sources[0].userHash);
  REQUIRE_EQ((uint16_t)1, reparsed.sources[1].cryptOptions);
}

void Ed2kHelperTest::testParseServerLink()
{
  auto server = parseLink("ed2k://|server|203.0.113.10|4232|/");

  REQUIRE_EQ(LinkType::SERVER, server.type);
  REQUIRE_EQ(std::string("203.0.113.10"), server.server.host);
  REQUIRE_EQ((uint16_t)4232, server.server.port);

  auto serverList =
      parseLink("ed2k://|serverlist|http%3A%2F%2Fexample.test%2Fserver.met|/");
  REQUIRE_EQ(LinkType::SERVER_LIST, serverList.type);
  REQUIRE_EQ(std::string("http://example.test/server.met"),
                       serverList.url);

  auto nodes =
      parseLink("ed2k://|nodeslist|https%3A%2F%2Fexample.test%2Fnodes.dat|/");
  REQUIRE_EQ(LinkType::NODES_LIST, nodes.type);
  REQUIRE_EQ(std::string("https://example.test/nodes.dat"),
                       nodes.url);
}

void Ed2kHelperTest::testParseSearchLink()
{
  auto search = parseLink("ed2k://|search|linux%20iso|/");

  REQUIRE_EQ(LinkType::SEARCH, search.type);
  REQUIRE_EQ(std::string("linux iso"), search.name);
}

void Ed2kHelperTest::testParseRejectsMalformedLinks()
{
  REQUIRE_THROWS_AS(parseLink("http://example.test/file"),
                       RecoverableException);
  REQUIRE_THROWS_AS(
      parseLink("ed2k://|file|bad.bin|x|0123456789abcdef0123456789abcdef|/"),
      RecoverableException);
  REQUIRE_THROWS_AS(parseLink("ed2k://|file|bad.bin|1|not-a-hash|/"),
                       RecoverableException);
  REQUIRE_THROWS_AS(
      parseLink("ed2k://|file|empty.bin|0|0123456789abcdef0123456789abcdef|/"),
      RecoverableException);
  REQUIRE_THROWS_AS(
      parseLink("ed2k://|file|huge.bin|274877906944|"
                "0123456789abcdef0123456789abcdef|/"),
      RecoverableException);
  REQUIRE_THROWS_AS(
      parseLink("ed2k://|file|bad-parts.bin|1|"
                "0123456789abcdef0123456789abcdef|p=|/"),
      RecoverableException);
  REQUIRE_THROWS_AS(
      parseLink("ed2k://|file|bad-aich.bin|1|"
                "0123456789abcdef0123456789abcdef|h=ABC|/"),
      RecoverableException);
  REQUIRE_THROWS_AS(parseLink("ed2k://|server|127.0.0.1|70000|/"),
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

  REQUIRE_EQ(link.name, reparsed.name);
  REQUIRE_EQ(link.size, reparsed.size);
  REQUIRE_EQ(link.hash, reparsed.hash);
  REQUIRE_EQ(link.pieceHashes[0], reparsed.pieceHashes[0]);
  REQUIRE_EQ(link.aichHash, reparsed.aichHash);
  REQUIRE_EQ(std::string("192.0.2.1"), reparsed.sources[0].host);
  REQUIRE_EQ((uint16_t)4662, reparsed.sources[0].port);
}

void Ed2kHelperTest::testPacketHelpers()
{
  std::string payload;
  payload += "abc";
  auto packet = createPacket(PROTO_EDONKEY, OP_GETSOURCES, payload);

  REQUIRE_EQ((size_t)9, packet.size());
  REQUIRE_EQ((unsigned char)PROTO_EDONKEY,
                       (unsigned char)packet[0]);
  REQUIRE_EQ(std::string("04000000"), util::toHex(packet.substr(1, 4)));
  REQUIRE_EQ((unsigned char)OP_GETSOURCES,
                       (unsigned char)packet[5]);
  REQUIRE_EQ(std::string("abc"), packet.substr(6));

  PacketHeader header;
  REQUIRE(readPacketHeader(header, packet.data(), packet.size()));
  REQUIRE_EQ((uint8_t)PROTO_EDONKEY, header.protocol);
  REQUIRE_EQ((uint32_t)4, header.size);
  REQUIRE_EQ((uint8_t)OP_GETSOURCES, header.opcode);
  REQUIRE_EQ((size_t)3, header.payloadSize());

  REQUIRE_EQ(std::string("78563412"), util::toHex(packUInt32(0x12345678)));
  REQUIRE_EQ((uint32_t)0x12345678,
                       readUInt32(std::string("\x78\x56\x34\x12", 4).data()));
}

void Ed2kHelperTest::testTagParser()
{
  std::string payload;
  payload += packUInt32(5);
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
  payload.push_back(0x03);
  payload += packUInt16(1);
  payload.push_back('\xfe');
  payload += packUInt32(0x6f71c138);

  std::vector<Tag> tags;
  REQUIRE(parseTagList(tags, payload));
  REQUIRE_EQ((size_t)5, tags.size());
  REQUIRE_EQ((uint8_t)0x01, tags[0].id);
  REQUIRE_EQ(std::string("video.mkv"), tags[0].stringValue);
  REQUIRE_EQ((uint8_t)0x15, tags[1].id);
  REQUIRE_EQ((uint64_t)77, tags[1].intValue);
  REQUIRE_EQ((uint8_t)0x02, tags[2].id);
  REQUIRE_EQ((uint64_t)0x100000005LL, tags[2].intValue);
  REQUIRE_EQ(std::string("Vid"), tags[3].stringValue);
  REQUIRE_EQ((uint8_t)0xfe, tags[4].id);
  REQUIRE_EQ((uint64_t)0x6f71c138, tags[4].intValue);
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
  REQUIRE_EQ(clientHash, login.substr(0, HASH_LENGTH));
  REQUIRE_EQ(std::string("010203040000"),
                       util::toHex(login.substr(16, 6)));
  REQUIRE_EQ((uint32_t)4, readUInt32(login.data() + 22));

  auto source32 = createGetSourcesPayload(fileHash, 9728001);
  REQUIRE_EQ((size_t)20, source32.size());
  REQUIRE_EQ(fileHash, source32.substr(0, HASH_LENGTH));
  REQUIRE_EQ((uint32_t)9728001, readUInt32(source32.data() + 16));

  auto source64 = createGetSourcesPayload(fileHash, 0x100000001LL);
  REQUIRE_EQ((size_t)28, source64.size());
  REQUIRE_EQ((uint32_t)0, readUInt32(source64.data() + 16));
  REQUIRE_EQ((uint32_t)1, readUInt32(source64.data() + 20));
  REQUIRE_EQ((uint32_t)1, readUInt32(source64.data() + 24));

  auto globSources = createGlobGetSourcesPayload(fileHash, 9728001, false);
  REQUIRE_EQ((size_t)16, globSources.size());
  REQUIRE_EQ(fileHash, globSources);
  REQUIRE_EQ(source32,
                       createGlobGetSourcesPayload(fileHash, 9728001, true));

  std::vector<Endpoint> sources;
  Endpoint source;
  source.host = "1.2.3.4";
  source.port = 4662;
  sources.push_back(source);
  auto found = createFoundSourcesPayload(fileHash, sources);
  auto parsedSources = parseFoundSourcesPayload(found);
  REQUIRE_EQ((size_t)1, parsedSources.size());
  REQUIRE_EQ(std::string("1.2.3.4"), parsedSources[0].host);
  REQUIRE_EQ((uint16_t)4662, parsedSources[0].port);
  REQUIRE(parseFoundSourcesPayload(parsedSources, found, fileHash));
  REQUIRE(!parseFoundSourcesPayload(parsedSources, found, clientHash));
  std::vector<FoundSource> foundSources;
  REQUIRE(parseFoundSourcesPayload(foundSources, found, fileHash));
  REQUIRE_EQ((size_t)1, foundSources.size());
  REQUIRE_EQ((uint32_t)0x04030201, foundSources[0].clientId);
  REQUIRE(!foundSources[0].lowId);

  Endpoint lowId;
  lowId.host = "120.0.0.0";
  lowId.port = 4662;
  auto lowIdPayload =
      createFoundSourcesPayload(fileHash, std::vector<Endpoint>{lowId});
  REQUIRE(parseFoundSourcesPayload(foundSources, lowIdPayload,
                                          fileHash));
  REQUIRE_EQ((uint32_t)120, foundSources[0].clientId);
  REQUIRE(foundSources[0].lowId);

  auto obfuPayload = found;
  obfuPayload.push_back(static_cast<char>(0x81));
  obfuPayload += clientHash;
  REQUIRE(parseFoundSourcesPayload(foundSources, obfuPayload, fileHash,
                                          true));
  REQUIRE_EQ((size_t)1, foundSources.size());
  REQUIRE_EQ((uint16_t)0x81,
                       foundSources[0].endpoint.cryptOptions);
  REQUIRE_EQ(clientHash, foundSources[0].endpoint.userHash);
  REQUIRE(!parseFoundSourcesPayload(foundSources, obfuPayload, fileHash));

  Endpoint source2;
  source2.host = "5.6.7.8";
  source2.port = 4662;
  auto packedFound =
      createFoundSourcesPayload(clientHash, std::vector<Endpoint>{source}) +
      createDatagram(PROTO_EDONKEY, OP_GLOBFOUNDSOURCES,
                     createFoundSourcesPayload(fileHash,
                                               std::vector<Endpoint>{source2}));
  std::vector<Endpoint> packedSources;
  REQUIRE(parsePackedFoundSourcesPayloads(packedSources, packedFound,
                                                 fileHash));
  REQUIRE_EQ((size_t)1, packedSources.size());
  REQUIRE_EQ(std::string("5.6.7.8"), packedSources[0].host);
  REQUIRE_EQ((uint16_t)4662, packedSources[0].port);
  std::vector<FoundSource> packedFoundSources;
  REQUIRE(parsePackedFoundSourcesPayloads(packedFoundSources,
                                                 packedFound, fileHash));
  REQUIRE_EQ((size_t)1, packedFoundSources.size());
  REQUIRE_EQ((uint32_t)0x08070605,
                       packedFoundSources[0].clientId);
  REQUIRE(!packedFoundSources[0].lowId);
  auto packedWithBadTail =
      createFoundSourcesPayload(fileHash, std::vector<Endpoint>{source}) +
      createDatagram(PROTO_EDONKEY, OP_GLOBFOUNDSOURCES,
                     createFoundSourcesPayload(clientHash,
                                               std::vector<Endpoint>{source2})) +
      std::string("\xe3\x90", 2);
  REQUIRE(parsePackedFoundSourcesPayloads(
      packedFoundSources, packedWithBadTail, fileHash));
  REQUIRE_EQ((size_t)1, packedFoundSources.size());
  REQUIRE_EQ((uint32_t)0x04030201,
                       packedFoundSources[0].clientId);

  auto callbackRequest = createCallbackRequestPayload(120);
  REQUIRE_EQ(std::string("78000000"),
                       util::toHex(callbackRequest));
  Endpoint callbackEndpoint;
  REQUIRE(parseCallbackRequestIncomingPayload(
      callbackEndpoint, packUInt32(0x04030201) + packUInt16(4662)));
  REQUIRE_EQ(std::string("1.2.3.4"), callbackEndpoint.host);
  REQUIRE_EQ((uint16_t)4662, callbackEndpoint.port);
  REQUIRE(parseCallbackRequestIncomingPayload(
      callbackEndpoint, packUInt32(0x04030201) + packUInt16(4662) +
                            std::string(1, '\x83') + clientHash));
  REQUIRE_EQ(std::string("1.2.3.4"), callbackEndpoint.host);
  REQUIRE_EQ((uint16_t)4662, callbackEndpoint.port);
  REQUIRE_EQ((uint16_t)0x83, callbackEndpoint.cryptOptions);
  REQUIRE_EQ(clientHash, callbackEndpoint.userHash);
  REQUIRE(parseCallbackRequestIncomingPayload(
      callbackEndpoint, packUInt32(0x04030201) + packUInt16(4662) +
                            std::string(1, '\x83') + clientHash +
                            std::string("ignored")));

  std::vector<bool> bitfield;
  bitfield.push_back(true);
  bitfield.push_back(false);
  bitfield.push_back(true);
  auto status = createFileStatusPayload(fileHash, bitfield);
  REQUIRE_EQ((uint8_t)0x05, static_cast<uint8_t>(status[18]));
  std::vector<bool> parsedBitfield;
  REQUIRE(parseFileStatusPayload(parsedBitfield, status, fileHash));
  REQUIRE_EQ((size_t)3, parsedBitfield.size());
  REQUIRE(parsedBitfield[0]);
  REQUIRE(!parsedBitfield[1]);
  REQUIRE(parsedBitfield[2]);

  std::vector<PartRange> ranges;
  PartRange range;
  range.begin = 0;
  range.end = 10;
  ranges.push_back(range);
  range.begin = 20;
  range.end = 30;
  ranges.push_back(range);
  auto requestParts = createRequestPartsPayload(fileHash, ranges, false);
  REQUIRE_EQ((size_t)40, requestParts.size());
  REQUIRE_EQ((uint32_t)0, readUInt32(requestParts.data() + 16));
  REQUIRE_EQ((uint32_t)20, readUInt32(requestParts.data() + 20));
  REQUIRE_EQ((uint32_t)0, readUInt32(requestParts.data() + 24));
  REQUIRE_EQ((uint32_t)10, readUInt32(requestParts.data() + 28));
  REQUIRE_EQ((uint32_t)30, readUInt32(requestParts.data() + 32));
  REQUIRE_EQ((uint32_t)0, readUInt32(requestParts.data() + 36));

  auto requestParts64 = createRequestPartsPayload(fileHash, ranges, true);
  REQUIRE_EQ((size_t)64, requestParts64.size());
  REQUIRE_EQ((uint64_t)0, readUInt64(requestParts64.data() + 16));
  REQUIRE_EQ((uint64_t)20, readUInt64(requestParts64.data() + 24));
  REQUIRE_EQ((uint64_t)0, readUInt64(requestParts64.data() + 32));
  REQUIRE_EQ((uint64_t)10, readUInt64(requestParts64.data() + 40));
  REQUIRE_EQ((uint64_t)30, readUInt64(requestParts64.data() + 48));
  REQUIRE_EQ((uint64_t)0, readUInt64(requestParts64.data() + 56));

  range.begin = 0x100000000LL;
  range.end = 0x100000100LL;
  REQUIRE_THROWS_AS(createRequestPartsPayload(
                           fileHash, std::vector<PartRange>(1, range), false),
                       DlAbortEx);
  REQUIRE_NOTHROW(createRequestPartsPayload(
      fileHash, std::vector<PartRange>(1, range), true));
}

void Ed2kHelperTest::testServerPayloadParsers()
{
  ServerIdChange idChange;
  REQUIRE(parseServerIdChangePayload(idChange, packUInt32(0x04030201)));
  REQUIRE_EQ((uint32_t)0x04030201, idChange.clientId);
  REQUIRE(idChange.highId);
  REQUIRE_EQ(std::string("1.2.3.4"), idChange.ipAddress);
  REQUIRE_EQ((uint32_t)0, idChange.tcpFlags);

  REQUIRE(parseServerIdChangePayload(idChange, packUInt32(120)));
  REQUIRE_EQ((uint32_t)120, idChange.clientId);
  REQUIRE(!idChange.highId);
  REQUIRE(idChange.ipAddress.empty());
  REQUIRE(parseServerIdChangePayload(
      idChange, packUInt32(120) + packUInt32(0x55aa) + packUInt32(4661)));
  REQUIRE_EQ((uint32_t)120, idChange.clientId);
  REQUIRE_EQ((uint32_t)0x55aa, idChange.tcpFlags);
  REQUIRE_EQ((uint32_t)4661, idChange.auxPort);
  REQUIRE(parseServerIdChangePayload(
      idChange, packUInt32(0x04030201) + packUInt32(SRV_TCPFLG_TCPOBFUSCATION) +
                    packUInt32(4661) + packUInt32(0x04030201) +
                    packUInt32(4666)));
  REQUIRE_EQ((uint16_t)4666, idChange.tcpObfuscationPort);
  REQUIRE(parseServerIdChangePayload(
      idChange, packUInt32(0x04030201) + packUInt32(0x55aa) +
                    packUInt32(4661) + packUInt32(0x04030201)));
  REQUIRE_EQ((uint32_t)0x04030201, idChange.clientId);
  REQUIRE_EQ((uint32_t)0x55aa, idChange.tcpFlags);
  REQUIRE_EQ((uint32_t)4661, idChange.auxPort);
  REQUIRE_EQ((uint16_t)0, idChange.tcpObfuscationPort);

  REQUIRE(parseServerIdChangePayload(
      idChange, packUInt32(0x04030201) + packUInt32(0x55aa) +
                    packUInt32(4661) + packUInt32(0x04030201) +
                    packUInt32(4666) + packUInt32(0xdeadbeef)));
  REQUIRE_EQ((uint16_t)4666, idChange.tcpObfuscationPort);

  ServerStatus status;
  REQUIRE(parseServerStatusPayload(status,
                                          packUInt32(1234) + packUInt32(5678)));
  REQUIRE_EQ((uint32_t)1234, status.users);
  REQUIRE_EQ((uint32_t)5678, status.files);
  REQUIRE_EQ((uint32_t)0, status.challenge);

  REQUIRE(parseServerStatusPayload(
      status, packUInt32(1234) + packUInt32(5678) + packUInt32(9000)));
  REQUIRE_EQ((uint32_t)1234, status.users);
  REQUIRE_EQ((uint32_t)5678, status.files);
  REQUIRE_EQ((uint32_t)0, status.challenge);

  REQUIRE(parseServerStatusPayload(
      status, packUInt32(0x55aa0011) + packUInt32(1234) +
                  packUInt32(5678) + packUInt32(9000)));
  REQUIRE_EQ((uint32_t)0x55aa0011, status.users);
  REQUIRE_EQ((uint32_t)1234, status.files);
  REQUIRE_EQ((uint32_t)0, status.challenge);

  REQUIRE(parseServerUdpStatusPayload(
      status, packUInt32(0x55aa0011) + packUInt32(1234) +
                  packUInt32(5678) + packUInt32(9000) + packUInt32(100) +
                  packUInt32(200) + packUInt32(0x01020304) +
                  packUInt32(77) + packUInt16(4665) + packUInt16(4666) +
                  packUInt32(0x11223344)));
  REQUIRE_EQ((uint32_t)0x55aa0011, status.challenge);
  REQUIRE_EQ((uint32_t)1234, status.users);
  REQUIRE_EQ((uint32_t)5678, status.files);
  REQUIRE_EQ((uint32_t)9000, status.maxUsers);
  REQUIRE_EQ((uint32_t)100, status.softFiles);
  REQUIRE_EQ((uint32_t)200, status.hardFiles);
  REQUIRE_EQ((uint32_t)0x01020304, status.udpFlags);
  REQUIRE_EQ((uint32_t)77, status.lowIdUsers);
  REQUIRE_EQ((uint16_t)4665, status.udpObfuscationPort);
  REQUIRE_EQ((uint16_t)4666, status.tcpObfuscationPort);
  REQUIRE_EQ((uint32_t)0x11223344, status.udpKey);
  REQUIRE(parseServerUdpStatusPayload(
      status, packUInt32(0x55aa0011) + packUInt32(1234) +
                  packUInt32(5678) + packUInt32(9000) + packUInt32(100) +
                  packUInt32(200) + packUInt32(0x01020304) +
                  packUInt32(77) + packUInt16(4665) + packUInt16(4666) +
                  packUInt32(0x11223344) + packUInt16(0)));
  REQUIRE_EQ((uint32_t)0x55aa0011, status.challenge);
  REQUIRE_EQ((uint16_t)4665, status.udpObfuscationPort);
  REQUIRE_EQ((uint16_t)4666, status.tcpObfuscationPort);
  REQUIRE_EQ((uint32_t)0x11223344, status.udpKey);

  std::string messagePayload = packUInt16(5) + "hello";
  std::string message;
  REQUIRE(parseServerMessagePayload(message, messagePayload));
  REQUIRE_EQ(std::string("hello"), message);

  std::string identPayload(16, '\x11');
  identPayload += packUInt32(0x04030201);
  identPayload += packUInt16(4661);
  identPayload += packUInt32(2);
  identPayload += createStringTag(0x01, "server name");
  identPayload += createStringTag(0x0b, "server description");
  identPayload += packUInt16(0);
  ServerIdent ident;
  REQUIRE(parseServerIdentPayload(ident, identPayload));
  REQUIRE_EQ(std::string("1.2.3.4"), ident.endpoint.host);
  REQUIRE_EQ((uint16_t)4661, ident.endpoint.port);
  REQUIRE_EQ(std::string("server name"), ident.name);
  REQUIRE_EQ(std::string("server description"), ident.description);

  std::vector<Endpoint> servers;
  std::string serverList;
  serverList.push_back('\x02');
  serverList += packUInt32(0x04030201);
  serverList += packUInt16(4661);
  serverList += packUInt32(0x08070605);
  serverList += packUInt16(4662);
  serverList += packUInt16(0);
  REQUIRE(parseServerListPayload(servers, serverList));
  REQUIRE_EQ((size_t)2, servers.size());
  REQUIRE_EQ(std::string("1.2.3.4"), servers[0].host);
  REQUIRE_EQ((uint16_t)4661, servers[0].port);
  REQUIRE_EQ(std::string("5.6.7.8"), servers[1].host);
  REQUIRE_EQ((uint16_t)4662, servers[1].port);
}

void Ed2kHelperTest::testSearchResultPayload()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());

  std::string tags;
  tags += packUInt32(7);
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
  tags.push_back('\x02');
  tags += packUInt16(5);
  tags += "codec";
  tags += packUInt16(4);
  tags += "H264";

  std::string payload;
  payload += packUInt32(1);
  payload += fileHash;
  payload += packUInt32(0x04030201);
  payload += packUInt16(4662);
  payload += tags;
  payload.push_back('\x01');

  SearchResult result;
  REQUIRE(parseSearchResultPayload(result, payload, "server"));
  REQUIRE(result.moreResults);
  REQUIRE_EQ((size_t)1, result.entries.size());
  REQUIRE_EQ(fileHash, result.entries[0].hash);
  REQUIRE_EQ(std::string("video.mkv"), result.entries[0].name);
  REQUIRE_EQ((int64_t)0x100000005LL, result.entries[0].size);
  REQUIRE_EQ(std::string("Video"), result.entries[0].fileType);
  REQUIRE_EQ((uint32_t)42, result.entries[0].sourceCount);
  REQUIRE_EQ((uint32_t)7, result.entries[0].completeSourceCount);
  REQUIRE_EQ(std::string("H264"), result.entries[0].mediaCodec);
  REQUIRE_EQ(std::string("server"), result.entries[0].sourceNetwork);
  REQUIRE_EQ((size_t)1, result.entries[0].sources.size());
  REQUIRE_EQ(std::string("1.2.3.4"),
                       result.entries[0].sources[0].host);
  REQUIRE_EQ((uint16_t)4662, result.entries[0].sources[0].port);
  REQUIRE_EQ(
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

  REQUIRE_EQ(util::toHex(expected), util::toHex(payload));

  auto clamped = createSearchRequestPayload(query, false);
  REQUIRE_EQ(static_cast<char>(0x03), clamped[28]);
  REQUIRE_EQ((uint32_t)0xffffffffu, readUInt32(clamped.data() + 29));
}

void Ed2kHelperTest::testKadKeywordTarget()
{
  REQUIRE_EQ(std::string("oxymoronaccelerator"),
                       pickKadKeyword("The oxymoronaccelerator 2"));
  REQUIRE_EQ(std::string("ubuntu"),
                       pickKadKeyword("Ubuntu-22.04 ISO"));
  REQUIRE_EQ(std::string(), pickKadKeyword("a 12 ()"));

  auto target = createKadKeywordTarget("The oxymoronaccelerator 2");
  REQUIRE_EQ(
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

  REQUIRE_EQ((size_t)1, results.size());
  REQUIRE_EQ(fileId, results[0].hash);
  REQUIRE_EQ(std::string("video.mkv"), results[0].name);
  REQUIRE_EQ((int64_t)0x100000005LL, results[0].size);
  REQUIRE_EQ(std::string("Video"), results[0].fileType);
  REQUIRE_EQ(std::string("mkv"), results[0].extension);
  REQUIRE_EQ((uint32_t)3, results[0].sourceCount);
  REQUIRE_EQ((uint32_t)2, results[0].completeSourceCount);
  REQUIRE_EQ(std::string("kad"), results[0].sourceNetwork);
  REQUIRE_EQ(
      std::string("ed2k://|file|video.mkv|4294967301|"
                  "0123456789abcdef0123456789abcdef|/"),
      results[0].ed2kLink);
}

void Ed2kHelperTest::testKadSourceEndpointPreservesUdpAndCryptMetadata()
{
  KadSearchEntry entry;
  entry.id = std::string(HASH_LENGTH, '\x44');

  Tag sourceType;
  sourceType.id = 0xff;
  sourceType.valueType = TagValueType::UINT;
  sourceType.intValue = 1;
  entry.tags.push_back(sourceType);

  Tag sourceIp;
  sourceIp.id = 0xfe;
  sourceIp.valueType = TagValueType::UINT;
  sourceIp.intValue = 0xdc84b534;
  entry.tags.push_back(sourceIp);

  Tag sourcePort;
  sourcePort.id = 0xfd;
  sourcePort.valueType = TagValueType::UINT;
  sourcePort.intValue = 4662;
  entry.tags.push_back(sourcePort);

  Tag sourceUdpPort;
  sourceUdpPort.id = 0xfc;
  sourceUdpPort.valueType = TagValueType::UINT;
  sourceUdpPort.intValue = 4672;
  entry.tags.push_back(sourceUdpPort);

  Tag encryption;
  encryption.id = 0xf3;
  encryption.valueType = TagValueType::UINT;
  encryption.intValue = 0x03;
  entry.tags.push_back(encryption);

  Endpoint endpoint;
  REQUIRE(extractKadSourceEndpoint(endpoint, entry));
  REQUIRE_EQ(std::string("220.132.181.52"), endpoint.host);
  REQUIRE_EQ((uint16_t)4662, endpoint.port);
  REQUIRE_EQ(std::string(HASH_LENGTH, '\x44'), endpoint.userHash);
  REQUIRE_EQ((uint16_t)0x03, endpoint.cryptOptions);

  entry.id = util::fromHex(std::begin("0c7fab2a8d37bed47b551391d0d8241d"),
                           std::end("0c7fab2a8d37bed47b551391d0d8241d") - 1);
  REQUIRE(extractKadSourceEndpoint(endpoint, entry));
  REQUIRE_EQ(
      std::string("0c7fab2a8d37bed47b551391d0d8241d"),
      util::toHex(endpoint.userHash));

  KadSourceEndpoint source;
  REQUIRE(extractKadSourceEndpoint(source, entry));
  REQUIRE_EQ(std::string("220.132.181.52"), source.endpoint.host);
  REQUIRE_EQ((uint16_t)4662, source.endpoint.port);
  REQUIRE_EQ((uint16_t)4672, source.udpPort);
  REQUIRE_EQ((uint8_t)1, source.sourceType);
  REQUIRE_EQ((uint16_t)0x03, source.endpoint.cryptOptions);

  sourceType.intValue = 3;
  entry.tags[0] = sourceType;
  auto buddyHashTagPayload =
      createStringTag(0xf8, "11111111111111111111111111111111");
  size_t buddyHashTagOffset = 0;
  entry.tags.push_back(readTag(buddyHashTagPayload, buddyHashTagOffset));
  REQUIRE(extractKadSourceEndpoint(source, entry));
  REQUIRE_EQ((uint8_t)3, source.sourceType);
  REQUIRE_EQ((uint16_t)4672, source.udpPort);
  REQUIRE_EQ(
      std::string("\x11\x11\x11\x11\x11\x11\x11\x11"
                  "\x11\x11\x11\x11\x11\x11\x11\x11",
                  HASH_LENGTH),
      source.buddyId);

  sourceType.intValue = 5;
  entry.tags[0] = sourceType;
  REQUIRE(extractKadSourceEndpoint(source, entry));
  REQUIRE_EQ((uint8_t)5, source.sourceType);

  sourceType.intValue = 6;
  entry.tags[0] = sourceType;
  REQUIRE(extractKadSourceEndpoint(source, entry));
  REQUIRE_EQ((uint8_t)6, source.sourceType);

  KadSearchResult result;
  result.entries.push_back(entry);
  auto endpoints = extractKadSourceEndpoints(result);
  REQUIRE_EQ((size_t)0, endpoints.size());

  sourceType.intValue = 4;
  result.entries[0].tags[0] = sourceType;
  endpoints = extractKadSourceEndpoints(result);
  REQUIRE_EQ((size_t)1, endpoints.size());
  REQUIRE_EQ(std::string("220.132.181.52"), endpoints[0].host);
  REQUIRE_EQ((uint16_t)4662, endpoints[0].port);
}

void Ed2kHelperTest::testSourceExchange2Payloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::string userHashHex("11111111111111111111111111111111");
  auto userHash = util::fromHex(userHashHex.begin(), userHashHex.end());

  auto request = createRequestSources2Payload(fileHash);
  REQUIRE_EQ((size_t)19, request.size());
  REQUIRE_EQ((uint8_t)4, static_cast<uint8_t>(request[0]));
  REQUIRE_EQ((uint16_t)0, readUInt16(request.data() + 1));
  REQUIRE_EQ(fileHash, request.substr(3, HASH_LENGTH));
  uint8_t requestVersion = 0;
  REQUIRE(parseRequestSources2Payload(requestVersion, fileHash,
                                            fileHash));
  REQUIRE_EQ((uint8_t)1, requestVersion);
  REQUIRE(parseRequestSources2Payload(requestVersion, request,
                                            fileHash));
  REQUIRE_EQ((uint8_t)4, requestVersion);

  EmulePeerInfo sx2Peer;
  sx2Peer.miscOptions2.supportsSourceExchange2 = true;
  auto selectedRequest = createRequestSourcesPayload(fileHash, sx2Peer);
  REQUIRE_EQ((uint8_t)OP_REQUESTSOURCES2, selectedRequest.opcode);
  REQUIRE_EQ(request, selectedRequest.payload);

  EmulePeerInfo sx1Peer;
  sx1Peer.miscOptions.sourceExchange1Version = 3;
  selectedRequest = createRequestSourcesPayload(fileHash, sx1Peer);
  REQUIRE_EQ((uint8_t)OP_REQUESTSOURCES, selectedRequest.opcode);
  REQUIRE_EQ(fileHash, selectedRequest.payload);

  sx1Peer.miscOptions.sourceExchange1Version = 1;
  selectedRequest = createRequestSourcesPayload(fileHash, sx1Peer);
  REQUIRE_EQ((uint8_t)0, selectedRequest.opcode);
  REQUIRE(selectedRequest.payload.empty());

  selectedRequest = createRequestSourcesPayload(fileHash, EmulePeerInfo());
  REQUIRE_EQ((uint8_t)0, selectedRequest.opcode);
  REQUIRE(selectedRequest.payload.empty());

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
  REQUIRE(parseAnswerSources2Payload(parsed, answer, fileHash));
  REQUIRE_EQ((uint8_t)4, parsed.version);
  REQUIRE_EQ((size_t)1, parsed.entries.size());
  REQUIRE_EQ(std::string("203.0.113.9"),
                       parsed.entries[0].endpoint.host);
  REQUIRE_EQ((uint16_t)4662, parsed.entries[0].endpoint.port);
  REQUIRE_EQ(std::string("198.51.100.2"),
                       parsed.entries[0].server.host);
  REQUIRE_EQ((uint16_t)4661, parsed.entries[0].server.port);
  REQUIRE_EQ(userHash, parsed.entries[0].userHash);
  REQUIRE_EQ((uint8_t)0x83, parsed.entries[0].cryptOptions);

  auto sx1 = createAnswerSourcesPayload(fileHash, 1, entries);
  REQUIRE(parseAnswerSourcesPayload(parsed, sx1, fileHash, 1));
  REQUIRE_EQ((uint8_t)1, parsed.version);
  REQUIRE_EQ((size_t)1, parsed.entries.size());
  REQUIRE_EQ(std::string("203.0.113.9"),
                       parsed.entries[0].endpoint.host);
  REQUIRE(parsed.entries[0].userHash.empty());

  auto sx4 = createAnswerSourcesPayload(fileHash, 4, entries);
  REQUIRE(parseAnswerSourcesPayload(parsed, sx4, fileHash, 4));
  REQUIRE_EQ((uint8_t)4, parsed.version);
  REQUIRE_EQ(userHash, parsed.entries[0].userHash);
  REQUIRE_EQ((uint8_t)0x83, parsed.entries[0].cryptOptions);
}

void Ed2kHelperTest::testMultipacketPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::string aichRootHex("1111111111111111111111111111111111111111");
  auto aichRoot = util::fromHex(aichRootHex.begin(), aichRootHex.end());
  std::vector<bool> localParts{true, false};
  EmulePeerInfo remoteInfo;
  remoteInfo.miscOptions.aichVersion = 1;
  remoteInfo.miscOptions.sourceExchange1Version = 3;
  remoteInfo.miscOptions.extendedRequestsVersion = 2;
  remoteInfo.miscOptions.multiPacket = true;
  remoteInfo.miscOptions2.supportsSourceExchange2 = true;
  remoteInfo.miscOptions2.supportsExtendedMultipacket = true;

  auto request = createMultipacketFileRequestPayload(
      fileHash, ed2k::PIECE_LENGTH + 1, localParts, remoteInfo, true);

  REQUIRE_EQ(fileHash, request.substr(0, HASH_LENGTH));
  REQUIRE_EQ(static_cast<uint64_t>(ed2k::PIECE_LENGTH + 1),
                       readUInt64(request.data() + HASH_LENGTH));
  size_t offset = HASH_LENGTH + 8;
  REQUIRE_EQ((uint8_t)OP_REQUESTFILENAME,
                       static_cast<uint8_t>(request[offset++]));
  REQUIRE_EQ((uint16_t)2, readUInt16(request.data() + offset));
  offset += 2;
  REQUIRE_EQ((uint8_t)0x01, static_cast<uint8_t>(request[offset++]));
  REQUIRE_EQ((uint16_t)0, readUInt16(request.data() + offset));
  offset += 2;
  REQUIRE_EQ((uint8_t)OP_SETREQFILEID,
                       static_cast<uint8_t>(request[offset++]));
  REQUIRE_EQ((uint8_t)OP_REQUESTSOURCES2,
                       static_cast<uint8_t>(request[offset++]));
  REQUIRE_EQ((uint8_t)SOURCE_EXCHANGE2_VERSION,
                       static_cast<uint8_t>(request[offset++]));
  REQUIRE_EQ((uint16_t)0, readUInt16(request.data() + offset));
  offset += 2;
  REQUIRE_EQ((uint8_t)OP_AICHFILEHASHREQ,
                       static_cast<uint8_t>(request[offset++]));
  REQUIRE_EQ(request.size(), offset);

  auto answerPayload = fileHash;
  answerPayload.push_back(static_cast<char>(OP_REQFILENAMEANSWER));
  answerPayload += packUInt16(8);
  answerPayload += "test.iso";
  answerPayload.push_back(static_cast<char>(OP_FILESTATUS));
  answerPayload += packUInt16(2);
  answerPayload.push_back(static_cast<char>(0x03));
  answerPayload.push_back(static_cast<char>(OP_AICHFILEHASHANS));
  answerPayload += aichRoot;

  MultipacketAnswer answer;
  REQUIRE(parseMultipacketAnswerPayload(answer, answerPayload,
                                              fileHash));
  REQUIRE_EQ(std::string("test.iso"), answer.fileName);
  REQUIRE(answer.hasFileStatus);
  REQUIRE_EQ((size_t)2, answer.partStatus.size());
  REQUIRE(answer.partStatus[0]);
  REQUIRE(answer.partStatus[1]);
  REQUIRE(answer.hasAichRootHash);
  REQUIRE_EQ(aichRoot, answer.aichRootHash);

  std::vector<bool> completeStatus;
  REQUIRE(parseFileStatusPayload(
      completeStatus, fileHash + packUInt16(0), fileHash, 2));
  REQUIRE_EQ((size_t)2, completeStatus.size());
  REQUIRE(completeStatus[0]);
  REQUIRE(completeStatus[1]);
}

void Ed2kHelperTest::testCompressedPartPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());

  auto payload32 = fileHash + packUInt32(184320) + packUInt32(4) + "data";
  CompressedPartHeader header;
  std::string compressedData;
  REQUIRE(parseCompressedPartPayload(header, compressedData, payload32,
                                           fileHash, false));
  REQUIRE_EQ((int64_t)184320, header.begin);
  REQUIRE_EQ((uint32_t)4, header.totalCompressedLength);
  REQUIRE_EQ(std::string("data"), compressedData);

  auto payload64 = fileHash + packUInt64(0x100000001LL) + packUInt32(7) + "xy";
  REQUIRE(parseCompressedPartPayload(header, compressedData, payload64,
                                           fileHash, true));
  REQUIRE_EQ((int64_t)0x100000001LL, header.begin);
  REQUIRE_EQ((uint32_t)7, header.totalCompressedLength);
  REQUIRE_EQ(std::string("xy"), compressedData);

  auto bad = fileHash + packUInt32(0) + packUInt32(3) + "tiny";
  REQUIRE(!parseCompressedPartPayload(header, compressedData, bad,
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
  REQUIRE_EQ(Z_OK, deflateInit(&strm, Z_DEFAULT_COMPRESSION));
  strm.avail_in = input.size();
  strm.next_in = reinterpret_cast<unsigned char*>(&input[0]);
  std::string compressed(compressBound(input.size()), '\0');
  strm.avail_out = compressed.size();
  strm.next_out = reinterpret_cast<unsigned char*>(&compressed[0]);
  REQUIRE_EQ(Z_STREAM_END, deflate(&strm, Z_FINISH));
  compressed.resize(compressed.size() - strm.avail_out);
  REQUIRE_EQ(Z_OK, deflateEnd(&strm));

  std::string inflated;
  REQUIRE(inflateCompressedPartData(inflated, compressed, input.size()));
  REQUIRE_EQ(input, inflated);

  REQUIRE(!inflateCompressedPartData(inflated, compressed,
                                           input.size() - 1));
  REQUIRE(!inflateCompressedPartData(inflated, "not zlib", input.size()));
}

void Ed2kHelperTest::testCompressedPartInflaterKeepsBlockOwnerAcrossChunks()
{
  std::string input;
  for (int i = 0; i < 220000; ++i) {
    input.push_back(static_cast<char>('A' + (i % 5)));
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  REQUIRE_EQ(Z_OK, deflateInit(&strm, Z_DEFAULT_COMPRESSION));
  strm.avail_in = 98304;
  strm.next_in = reinterpret_cast<unsigned char*>(&input[0]);
  std::string compressed(compressBound(input.size()), '\0');
  strm.avail_out = compressed.size();
  strm.next_out = reinterpret_cast<unsigned char*>(&compressed[0]);
  REQUIRE_EQ(Z_OK, deflate(&strm, Z_SYNC_FLUSH));
  const auto firstCompressedLength = compressed.size() - strm.avail_out;
  strm.avail_in = input.size() - 98304;
  strm.next_in = reinterpret_cast<unsigned char*>(&input[98304]);
  REQUIRE_EQ(Z_STREAM_END, deflate(&strm, Z_FINISH));
  compressed.resize(compressed.size() - strm.avail_out);
  REQUIRE_EQ(Z_OK, deflateEnd(&strm));

  CompressedPartInflater inflater;
  std::string first;
  REQUIRE(inflater.inflateChunk(
      first, compressed.substr(0, firstCompressedLength), 0, 98304));
  REQUIRE(inflater.active());
  REQUIRE_EQ(static_cast<int64_t>(0), inflater.blockBegin());
  REQUIRE_EQ(static_cast<int64_t>(first.size()),
                       inflater.inflatedLength());
  REQUIRE_EQ(input.substr(0, 98304), first);

  std::string second;
  REQUIRE(inflater.inflateChunk(
      second, compressed.substr(firstCompressedLength), 0,
      input.size() - first.size()));
  REQUIRE(!inflater.active());
  REQUIRE_EQ(input, first + second);
}

void Ed2kHelperTest::testInflatePackedPacketPayload()
{
  std::string input;
  for (int i = 0; i < 2048; ++i) {
    input.push_back(static_cast<char>('a' + (i % 7)));
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  REQUIRE_EQ(Z_OK, deflateInit(&strm, Z_DEFAULT_COMPRESSION));
  strm.avail_in = input.size();
  strm.next_in = reinterpret_cast<unsigned char*>(&input[0]);
  std::string compressed(compressBound(input.size()), '\0');
  strm.avail_out = compressed.size();
  strm.next_out = reinterpret_cast<unsigned char*>(&compressed[0]);
  REQUIRE_EQ(Z_STREAM_END, deflate(&strm, Z_FINISH));
  compressed.resize(compressed.size() - strm.avail_out);
  REQUIRE_EQ(Z_OK, deflateEnd(&strm));

  std::string inflated;
  REQUIRE(inflatePackedPacketPayload(inflated, compressed,
                                            input.size() + 100));
  REQUIRE_EQ(input, inflated);

  REQUIRE(!inflatePackedPacketPayload(inflated, compressed,
                                            input.size() - 1));
  REQUIRE(!inflatePackedPacketPayload(inflated, "not zlib", input.size()));
}

void Ed2kHelperTest::testEmuleInfoPayload()
{
  EmulePeerInfo info;
  info.version = 0x3c;
  info.protocolVersion = 0x01;
  info.miscOptions.aichVersion = 2;
  info.miscOptions.unicode = true;
  info.miscOptions.udpVersion = 4;
  info.miscOptions.dataCompressionVersion = 1;
  info.miscOptions.sourceExchange1Version = 3;
  info.miscOptions.extendedRequestsVersion = 2;
  info.miscOptions.multiPacket = true;
  info.miscOptions2.supportsSourceExchange2 = true;
  info.miscOptions2.supportsLargeFiles = true;
  info.udpPort = 4672;

  auto payload = createEmuleInfoPayload(info);
  std::vector<Tag> muleTags;
  REQUIRE(parseTagList(muleTags, payload.substr(2)));
  auto hasUintTag = [&](uint8_t id) {
    return std::find_if(muleTags.begin(), muleTags.end(),
                        [&](const Tag& tag) {
                          return tag.id == id &&
                                 tag.valueType == TagValueType::UINT;
                        }) != muleTags.end();
  };
  auto hasStringTag = [&](uint8_t id) {
    return std::find_if(muleTags.begin(), muleTags.end(),
                        [&](const Tag& tag) {
                          return tag.id == id &&
                                 tag.valueType == TagValueType::STRING;
                        }) != muleTags.end();
  };
  REQUIRE(hasUintTag(0x20));
  REQUIRE(hasUintTag(0x21));
  REQUIRE(hasUintTag(0x22));
  REQUIRE(hasUintTag(0x23));
  REQUIRE(hasUintTag(0x24));
  REQUIRE(hasUintTag(0x25));
  REQUIRE(hasUintTag(0x26));
  REQUIRE(hasUintTag(0x27));
  REQUIRE(hasStringTag(0x55));
  REQUIRE(!hasUintTag(0xfb));
  REQUIRE(!hasUintTag(0xfa));
  REQUIRE(!hasUintTag(0xfe));

  EmulePeerInfo parsed;
  REQUIRE(parseEmuleInfoPayload(parsed, payload));
  REQUIRE_EQ((uint8_t)0x3c, parsed.version);
  REQUIRE_EQ((uint8_t)0x01, parsed.protocolVersion);
  REQUIRE_EQ((uint16_t)4672, parsed.udpPort);
  REQUIRE_EQ((uint8_t)0, parsed.miscOptions.aichVersion);
  REQUIRE(!parsed.miscOptions.unicode);
  REQUIRE_EQ((uint8_t)1, parsed.miscOptions.dataCompressionVersion);
  REQUIRE_EQ((uint8_t)3, parsed.miscOptions.sourceExchange1Version);
  REQUIRE_EQ((uint8_t)2, parsed.miscOptions.extendedRequestsVersion);
  REQUIRE(!parsed.miscOptions.multiPacket);
  REQUIRE(!parsed.miscOptions2.supportsSourceExchange2);
  REQUIRE(!parsed.miscOptions2.supportsLargeFiles);

  std::string remotePayload;
  remotePayload.push_back(static_cast<char>(0x3c));
  remotePayload.push_back(static_cast<char>(0x01));
  remotePayload += packUInt32(2);
  remotePayload += createUInt32Tag(0x21, 4672);
  remotePayload += createUInt32Tag(0x22, 4);
  REQUIRE(parseEmuleInfoPayload(parsed, remotePayload));
  REQUIRE_EQ((uint16_t)4672, parsed.udpPort);
  REQUIRE_EQ((uint8_t)4, parsed.miscOptions.udpVersion);
}

void Ed2kHelperTest::testLocalEmulePeerInfoCapabilities()
{
  auto info = createLocalEmulePeerInfo();

  REQUIRE_EQ((uint8_t)1, info.miscOptions.aichVersion);
  REQUIRE(info.miscOptions.unicode);
  REQUIRE_EQ((uint8_t)1,
                       info.miscOptions.dataCompressionVersion);
  REQUIRE_EQ((uint8_t)3,
                       info.miscOptions.sourceExchange1Version);
  REQUIRE_EQ((uint8_t)2,
                       info.miscOptions.extendedRequestsVersion);
  REQUIRE(info.miscOptions2.supportsLargeFiles);
  REQUIRE(info.miscOptions2.supportsSourceExchange2);
  REQUIRE_EQ((uint8_t)0, info.miscOptions.secureIdentVersion);
  REQUIRE(info.miscOptions.multiPacket);
  REQUIRE(info.miscOptions2.supportsExtendedMultipacket);
}

void Ed2kHelperTest::testPeerHelloPayload()
{
  std::string clientHashHex("0123456789abcdef0123456789abcdef");
  auto clientHash = util::fromHex(clientHashHex.begin(), clientHashHex.end());
  EmulePeerInfo info;
  info.version = 0x47;
  info.miscOptions.aichVersion = 1;
  info.miscOptions.unicode = true;
  info.miscOptions.udpVersion = 4;
  info.miscOptions.dataCompressionVersion = 1;
  info.miscOptions.sourceExchange1Version = 3;
  info.miscOptions.extendedRequestsVersion = 2;
  info.miscOptions.multiPacket = true;
  info.miscOptions2.supportsLargeFiles = true;
  info.miscOptions2.supportsExtendedMultipacket = true;
  info.miscOptions2.supportsSourceExchange2 = true;
  info.udpPort = 4672;

  Endpoint server;
  server.host = "1.2.3.4";
  server.port = 4661;
  auto payload = createPeerHelloPayload(clientHash, 0x0a000001, 4662,
                                        server, "aria2-next", info, true);
  REQUIRE_EQ((uint8_t)HASH_LENGTH,
                       static_cast<uint8_t>(payload[0]));
  REQUIRE_EQ(clientHash, payload.substr(1, HASH_LENGTH));
  REQUIRE_EQ((uint32_t)0x0a000001,
                       readUInt32(payload.data() + 1 + HASH_LENGTH));
  REQUIRE_EQ((uint16_t)4662,
                       readUInt16(payload.data() + 1 + HASH_LENGTH + 4));

  const auto tagOffset = 1 + HASH_LENGTH + 4 + 2;
  std::vector<Tag> tags;
  REQUIRE(parseTagList(tags, payload.substr(
                                        tagOffset,
                                        payload.size() - tagOffset - 6)));
  auto hasUintTag = [&](uint8_t id) {
    return std::find_if(tags.begin(), tags.end(), [&](const Tag& tag) {
             return tag.id == id && tag.valueType == TagValueType::UINT;
           }) != tags.end();
  };
  REQUIRE(hasUintTag(0x11));
  REQUIRE(hasUintTag(0xef));
  REQUIRE(hasUintTag(0xf9));
  REQUIRE(hasUintTag(0xfa));
  REQUIRE(hasUintTag(0xfb));
  REQUIRE(hasUintTag(0xfe));
  REQUIRE_EQ(std::string("010203043512"),
                       util::toHex(payload.substr(payload.size() - 6)));

  EmulePeerInfo parsed;
  REQUIRE(parsePeerHelloPayload(parsed, payload, true));
  REQUIRE_EQ(clientHash, parsed.userHash);
  REQUIRE_EQ((uint16_t)4672, parsed.udpPort);
  REQUIRE_EQ((uint8_t)4, parsed.miscOptions.udpVersion);
  REQUIRE_EQ((uint8_t)1, parsed.miscOptions.aichVersion);
  REQUIRE(parsed.miscOptions.unicode);
  REQUIRE_EQ((uint8_t)1, parsed.miscOptions.dataCompressionVersion);
  REQUIRE_EQ((uint8_t)3, parsed.miscOptions.sourceExchange1Version);
  REQUIRE_EQ((uint8_t)2, parsed.miscOptions.extendedRequestsVersion);
  REQUIRE(parsed.miscOptions.multiPacket);
  REQUIRE(parsed.miscOptions2.supportsLargeFiles);
  REQUIRE(parsed.miscOptions2.supportsExtendedMultipacket);
  REQUIRE(parsed.miscOptions2.supportsSourceExchange2);
}

void Ed2kHelperTest::testUdpReaskPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  auto ping = createUdpReaskFilePingPayload(fileHash, 7);
  UdpReask reask;
  REQUIRE(parseUdpReaskFilePingPayload(reask, ping));
  REQUIRE_EQ(fileHash, reask.fileHash);
  REQUIRE(reask.hasCompleteSources);
  REQUIRE_EQ((uint16_t)7, reask.completeSources);
  REQUIRE(parseUdpReaskFilePingPayload(reask, fileHash));
  REQUIRE(!reask.hasCompleteSources);

  auto ackPayload =
      createUdpReaskAckPayload(std::vector<bool>{true, false, true}, 42);
  UdpReaskAck ack;
  REQUIRE(parseUdpReaskAckPayload(ack, ackPayload));
  REQUIRE_EQ((size_t)3, ack.bitfield.size());
  REQUIRE(ack.bitfield[0]);
  REQUIRE(!ack.bitfield[1]);
  REQUIRE(ack.bitfield[2]);
  REQUIRE_EQ((uint16_t)42, ack.rank);
  REQUIRE(parseUdpReaskAckPayload(ack, createUdpReaskAckPayload(3)));
  REQUIRE(ack.bitfield.empty());
  REQUIRE_EQ((uint16_t)3, ack.rank);
  REQUIRE(!parseUdpReaskAckPayload(ack, std::string(3, '\0')));
}

void Ed2kHelperTest::testAichPayloads()
{
  std::string fileHashHex("0123456789abcdef0123456789abcdef");
  auto fileHash = util::fromHex(fileHashHex.begin(), fileHashHex.end());
  std::string aichRootHex("1111111111111111111111111111111111111111");
  auto aichRoot = util::fromHex(aichRootHex.begin(), aichRootHex.end());

  auto fileHashRequest = createAichFileHashRequestPayload(fileHash);
  REQUIRE_EQ(fileHash, fileHashRequest);

  AichFileHashAnswer fileHashAnswer;
  REQUIRE(parseAichFileHashAnswerPayload(
      fileHashAnswer, createAichFileHashAnswerPayload(fileHash, aichRoot),
      fileHash));
  REQUIRE_EQ(fileHash, fileHashAnswer.fileHash);
  REQUIRE_EQ(aichRoot, fileHashAnswer.rootHash);

  auto request = createAichRequestPayload(fileHash, 7, aichRoot);
  AichRequest parsedRequest;
  REQUIRE(parseAichRequestPayload(parsedRequest, request, fileHash));
  REQUIRE_EQ(fileHash, parsedRequest.fileHash);
  REQUIRE_EQ((uint16_t)7, parsedRequest.partIndex);
  REQUIRE_EQ(aichRoot, parsedRequest.rootHash);

  std::string recovery;
  recovery += packUInt16(3);
  recovery += std::string(20, '\x22');
  auto answer = createAichAnswerPayload(fileHash, 7, aichRoot, recovery);
  AichAnswer parsedAnswer;
  REQUIRE(parseAichAnswerPayload(parsedAnswer, answer, fileHash));
  REQUIRE(!parsedAnswer.failed);
  REQUIRE_EQ(fileHash, parsedAnswer.fileHash);
  REQUIRE_EQ((uint16_t)7, parsedAnswer.partIndex);
  REQUIRE_EQ(aichRoot, parsedAnswer.rootHash);
  REQUIRE_EQ(recovery, parsedAnswer.recoveryData);

  REQUIRE(parseAichAnswerPayload(parsedAnswer, fileHash, fileHash));
  REQUIRE(parsedAnswer.failed);
  REQUIRE_EQ(fileHash, parsedAnswer.fileHash);
  REQUIRE(parsedAnswer.rootHash.empty());
  REQUIRE(parsedAnswer.recoveryData.empty());

  REQUIRE(!parseAichRequestPayload(parsedRequest, request,
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
  REQUIRE(parseAichRecoveryData(parsed, recovery,
                                       block0.size() + block1.size() +
                                           block2.size(),
                                       false));
  REQUIRE(verifyAichRecoveryData(
      parsed, root, block0.size() + block1.size() + block2.size(), 0));
  AichRecoverySet recoverySet;
  REQUIRE(buildAichRecoverySet(
      recoverySet, parsed, root,
      block0.size() + block1.size() + block2.size(), 0));
  REQUIRE_EQ((size_t)3, recoverySet.blocks.size());
  REQUIRE_EQ(hash1, recoverySet.blocks[1].hash);
  recovery[4] ^= 0x01;
  REQUIRE(parseAichRecoveryData(parsed, recovery,
                                       block0.size() + block1.size() +
                                           block2.size(),
                                       false));
  REQUIRE(!verifyAichRecoveryData(
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
  REQUIRE_EQ((size_t)40, util::toHex(firstBlock).size());
  REQUIRE_EQ(firstBlock, aichRootHash(data.data(), EMBLOCK_LENGTH));
  auto left = aichHash(firstBlock + secondBlock);
  REQUIRE_EQ(aichHash(left + thirdBlock),
                       aichRootHash(data.data(), data.size()));

  std::vector<std::string> leaves{firstBlock, secondBlock, thirdBlock};
  REQUIRE_EQ(aichHash(left + thirdBlock), aichRootHash(leaves));

  REQUIRE_THROWS_AS(aichRootHash(std::vector<std::string>{std::string(19, '\0')}),
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

  REQUIRE_EQ(expected, flatRoot);
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
  REQUIRE(parseKadHelloPayload(parsedHello, hello));
  REQUIRE_EQ(nodeId, parsedHello.id);
  REQUIRE_EQ((uint16_t)4662, parsedHello.tcpPort);
  REQUIRE_EQ((uint8_t)5, parsedHello.version);

  auto bootstrap = createKadBootstrapResponsePayload(nodeId, 4662, 5,
                                                     std::vector<KadContact>{contact});
  KadBootstrapResponse parsedBootstrap;
  REQUIRE(parseKadBootstrapResponsePayload(parsedBootstrap, bootstrap));
  REQUIRE_EQ(nodeId, parsedBootstrap.id);
  REQUIRE_EQ((uint16_t)4662, parsedBootstrap.tcpPort);
  REQUIRE_EQ((uint8_t)5, parsedBootstrap.version);
  REQUIRE_EQ((size_t)1, parsedBootstrap.contacts.size());
  REQUIRE_EQ(std::string("0123456789abcdef0123456789abcdef097100cb"
                                   "4012361205"),
                       util::toHex(bootstrap.substr(HASH_LENGTH + 5)));
  REQUIRE_EQ(std::string("203.0.113.9"),
                       parsedBootstrap.contacts[0].host);
  REQUIRE_EQ((uint16_t)4672, parsedBootstrap.contacts[0].udpPort);
  REQUIRE_EQ((uint16_t)4662, parsedBootstrap.contacts[0].tcpPort);

  auto req = createKadRequestPayload(KAD_FIND_NODE, nodeId, nodeId);
  KadRequest parsedReq;
  REQUIRE(parseKadRequestPayload(parsedReq, req));
  REQUIRE_EQ((uint8_t)KAD_FIND_NODE, parsedReq.searchType);
  REQUIRE_EQ(nodeId, parsedReq.targetId);
  REQUIRE_EQ(nodeId, parsedReq.receiverId);

  auto res = createKadResponsePayload(nodeId, std::vector<KadContact>{contact});
  KadResponse parsedRes;
  REQUIRE(parseKadResponsePayload(parsedRes, res));
  REQUIRE_EQ(nodeId, parsedRes.targetId);
  REQUIRE_EQ((size_t)1, parsedRes.contacts.size());
  REQUIRE_EQ(std::string("0123456789abcdef0123456789abcdef01"
                                   "0123456789abcdef0123456789abcdef097100cb"
                                   "4012361205"),
                       util::toHex(res));
  REQUIRE_EQ(std::string("203.0.113.9"), parsedRes.contacts[0].host);
}

void Ed2kHelperTest::testKadUInt128ConversionMatchesAMule()
{
  const auto fileHash = util::fromHex(
      std::begin("2aab7f0cd4be378d9113557b1d24d8d0"),
      std::end("2aab7f0cd4be378d9113557b1d24d8d0") - 1);
  const auto kadId = ed2kHashToKadId(fileHash);

  REQUIRE_EQ(
      std::string("0c7fab2a8d37bed47b551391d0d8241d"),
      util::toHex(kadId));
  REQUIRE_EQ(fileHash, kadIdToEd2kHash(kadId));
}

void Ed2kHelperTest::testKadDirectCallbackPayload()
{
  const auto userHash = std::string(16, '\x42');

  auto payload = createDirectCallbackRequestPayload(4662, userHash, 0x03);

  REQUIRE_EQ((size_t)19, payload.size());
  REQUIRE_EQ((uint16_t)4662, readUInt16(payload.data()));
  REQUIRE_EQ(userHash, payload.substr(2, HASH_LENGTH));
  REQUIRE_EQ(static_cast<char>(0x03), payload[18]);

  DirectCallbackRequest request;
  REQUIRE(parseDirectCallbackRequestPayload(request, payload));
  REQUIRE_EQ((uint16_t)4662, request.tcpPort);
  REQUIRE_EQ(userHash, request.userHash);
  REQUIRE_EQ((uint8_t)0x03, request.connectOptions);

  REQUIRE(!parseDirectCallbackRequestPayload(request,
                                                   payload.substr(0, 18)));
}

void Ed2kHelperTest::testKadBuddyCallbackPayload()
{
  const auto buddyId = std::string(16, '\x31');
  const auto fileId = std::string(16, '\x42');
  Endpoint source;
  source.host = "203.0.113.44";
  source.port = 4662;

  auto payload = createBuddyCallbackPayload(buddyId, fileId, source);

  REQUIRE_EQ((size_t)38, payload.size());
  REQUIRE_EQ(buddyId, payload.substr(0, HASH_LENGTH));
  REQUIRE_EQ(fileId, payload.substr(HASH_LENGTH, HASH_LENGTH));
  REQUIRE_EQ(std::string("cb00712c3612"),
                       util::toHex(payload.substr(HASH_LENGTH * 2)));

  BuddyCallback callback;
  REQUIRE(parseBuddyCallbackPayload(callback, payload));
  REQUIRE_EQ(buddyId, callback.buddyId);
  REQUIRE_EQ(fileId, callback.fileId);
  REQUIRE_EQ(source.host, callback.endpoint.host);
  REQUIRE_EQ(source.port, callback.endpoint.port);
  REQUIRE(!parseBuddyCallbackPayload(callback, payload.substr(0, 37)));
}

void Ed2kHelperTest::testKadObfuscatedPacketRoundTrip()
{
  std::string nodeIdHex("0123456789abcdef0123456789abcdef");
  auto nodeId = util::fromHex(nodeIdHex.begin(), nodeIdHex.end());
  auto datagram = createDatagram(KAD_PROTOCOL, KAD_BOOTSTRAP_REQ, std::string());

  auto obfuscated = createKadObfuscatedDatagram(datagram, nodeId, 0x1234);

  REQUIRE_EQ((size_t)18, obfuscated.size());
  REQUIRE(static_cast<uint8_t>(obfuscated[0]) != KAD_PROTOCOL);
  REQUIRE_EQ(std::string("3412"),
                       util::toHex(obfuscated.substr(1, 2)));
  REQUIRE_EQ(std::string("0834123bfb9093bc2416f3250b68702029b8"),
                       util::toHex(obfuscated));

  KadObfuscatedDatagram parsed;
  REQUIRE(parseKadObfuscatedDatagram(parsed, obfuscated, nodeId));
  REQUIRE_EQ((uint32_t)0, parsed.receiverVerifyKey);
  REQUIRE_EQ((uint32_t)0, parsed.senderVerifyKey);
  REQUIRE_EQ(datagram, parsed.datagram);

  const auto amuleNodeKeyPacket = util::fromHex(
      std::begin("00fce06eda509dbfe1b8784806755e29e169"),
      std::end("00fce06eda509dbfe1b8784806755e29e169") - 1);
  const auto amuleNodeId = util::fromHex(
      std::begin("4115b891e3b4fafa7e5116332d508752"),
      std::end("4115b891e3b4fafa7e5116332d508752") - 1);
  REQUIRE(parseKadObfuscatedDatagram(parsed, amuleNodeKeyPacket,
                                           amuleNodeId));
  REQUIRE_EQ((uint32_t)0, parsed.receiverVerifyKey);
  REQUIRE_EQ((uint32_t)0x0c2bca82, parsed.senderVerifyKey);
  REQUIRE_EQ(createDatagram(KAD_PROTOCOL, 0x60, std::string()),
                       parsed.datagram);

  auto verifyKeyPacket =
      createKadObfuscatedDatagram(datagram, 0x11223344, 0x55667788, 0x1234);
  REQUIRE_EQ(std::string("0a34124907d4afead3f790b245955cf96823"),
                       util::toHex(verifyKeyPacket));
  REQUIRE(parseKadObfuscatedDatagram(parsed, verifyKeyPacket,
                                           (uint32_t)0x11223344));
  REQUIRE_EQ((uint32_t)0x11223344, parsed.receiverVerifyKey);
  REQUIRE_EQ((uint32_t)0x55667788, parsed.senderVerifyKey);
  REQUIRE_EQ(datagram, parsed.datagram);
  REQUIRE_EQ((uint32_t)0xc43989ee,
                       createKadUdpVerifyKey(0x61726961, "203.0.113.9"));
}

void Ed2kHelperTest::testKadSearchPublishAndFirewallPayloads()
{
  std::string fileIdHex("0123456789abcdef0123456789abcdef");
  auto fileId = util::fromHex(fileIdHex.begin(), fileIdHex.end());
  std::string sourceIdHex("11111111111111111111111111111111");
  auto sourceId = util::fromHex(sourceIdHex.begin(), sourceIdHex.end());

  auto sourceReq = createKadSearchSourcesRequestPayload(fileId, 3, 123456789);
  KadSearchSourcesRequest parsedSourceReq;
  REQUIRE(parseKadSearchSourcesRequestPayload(parsedSourceReq,
                                                    sourceReq));
  REQUIRE_EQ(fileId, parsedSourceReq.targetId);
  REQUIRE_EQ((uint16_t)3, parsedSourceReq.startPosition);
  REQUIRE_EQ((uint64_t)123456789, parsedSourceReq.size);

  auto keyReq = createKadSearchKeysRequestPayload(fileId, 7);
  REQUIRE_EQ((size_t)18, keyReq.size());
  REQUIRE_EQ((uint16_t)7, readUInt16(keyReq.data() + HASH_LENGTH));

  Endpoint source;
  source.host = "203.0.113.9";
  source.port = 4662;
  auto publish = createKadPublishSourceRequestPayload(fileId, source, sourceId);
  KadPublishSourceRequest parsedPublish;
  REQUIRE(parseKadPublishSourceRequestPayload(parsedPublish, publish));
  REQUIRE_EQ(fileId, parsedPublish.fileId);
  auto largePublish = createKadPublishSourceRequestPayload(
      fileId, source, sourceId, 0x100000001ULL);
  REQUIRE(parseKadPublishSourceRequestPayload(parsedPublish,
                                                     largePublish));
  auto sizeTag = std::find_if(parsedPublish.source.tags.begin(),
                              parsedPublish.source.tags.end(),
                              [](const Tag& tag) { return tag.id == 0xd3; });
  REQUIRE(sizeTag != parsedPublish.source.tags.end());
  REQUIRE_EQ((uint64_t)0x100000001ULL, sizeTag->intValue);
  auto sourceTypeTag = std::find_if(parsedPublish.source.tags.begin(),
                                    parsedPublish.source.tags.end(),
                                    [](const Tag& tag) {
                                      return tag.id == 0xff;
                                    });
  REQUIRE(sourceTypeTag != parsedPublish.source.tags.end());
  REQUIRE_EQ((uint64_t)4, sourceTypeTag->intValue);

  auto searchRes = createKadSearchResultPayload(
      sourceId, fileId,
      std::vector<KadSearchEntry>{parsedPublish.source});
  KadSearchResult result;
  REQUIRE(parseKadSearchResultPayload(result, searchRes));
  REQUIRE_EQ(sourceId, result.sourceId);
  REQUIRE_EQ(fileId, result.targetId);
  auto endpoints = extractKadSourceEndpoints(result);
  REQUIRE_EQ((size_t)1, endpoints.size());
  REQUIRE_EQ(std::string("203.0.113.9"), endpoints[0].host);
  REQUIRE_EQ((uint16_t)4662, endpoints[0].port);

  KadPublishResult publishResult;
  REQUIRE(parseKadPublishResultPayload(
      publishResult, createKadPublishResultPayload(fileId, 1)));
  REQUIRE_EQ(fileId, publishResult.fileId);
  REQUIRE_EQ((uint8_t)1, publishResult.count);

  KadFirewalledRequest fwReq;
  REQUIRE(parseKadFirewalledRequestPayload(
      fwReq, createKadFirewalledRequestPayload(4662, sourceId, 2)));
  REQUIRE_EQ((uint16_t)4662, fwReq.tcpPort);
  REQUIRE_EQ(sourceId, fwReq.id);
  REQUIRE_EQ((uint8_t)2, fwReq.options);

  KadFirewalledResponse fwRes;
  REQUIRE(parseKadFirewalledResponsePayload(
      fwRes, createKadFirewalledResponsePayload("203.0.113.9")));
  REQUIRE_EQ(std::string("203.0.113.9"), fwRes.ipAddress);

  KadCallbackRequest callbackReq;
  REQUIRE(parseKadCallbackRequestPayload(
      callbackReq,
      createKadCallbackRequestPayload(sourceId, fileId, 4662)));
  REQUIRE_EQ(sourceId, callbackReq.buddyId);
  REQUIRE_EQ(fileId, callbackReq.fileId);
  REQUIRE_EQ((uint16_t)4662, callbackReq.tcpPort);

  KadFirewalledUdp fwUdp;
  REQUIRE(parseKadFirewalledUdpPayload(
      fwUdp, createKadFirewalledUdpPayload(1, 4662)));
  REQUIRE_EQ((uint8_t)1, fwUdp.errorCode);
  REQUIRE_EQ((uint16_t)4662, fwUdp.tcpPort);
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
  snapshot.lastSourceSearch = 700;
  snapshot.sourceSearchCount = 3;
  snapshot.udpVerifyKey = 0x61726961;
  snapshot.firewalled = false;
  snapshot.observedAddresses.push_back("203.0.113.55");
  Endpoint router;
  router.host = "203.0.113.1";
  router.port = 4672;
  snapshot.routerNodes.push_back(router);
  KadContact routerContact;
  std::string routerIdHex("11111111111111111111111111111111");
  routerContact.id = util::fromHex(routerIdHex.begin(), routerIdHex.end());
  routerContact.host = "203.0.113.2";
  routerContact.udpPort = 4672;
  routerContact.tcpPort = 4662;
  routerContact.version = 8;
  routerContact.udpKey = 0x55667788;
  snapshot.routerContacts.push_back(routerContact);
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
  REQUIRE(parseKadRoutingStatePayload(parsed, payload));
  REQUIRE_EQ(selfId, parsed.selfId);
  REQUIRE_EQ((int64_t)100, parsed.lastBootstrap);
  REQUIRE_EQ((int64_t)200, parsed.lastRefresh);
  REQUIRE_EQ((int64_t)300, parsed.lastSelfRefresh);
  REQUIRE_EQ((int64_t)500, parsed.lastFirewalledCheck);
  REQUIRE_EQ((int64_t)600, parsed.lastSourcePublish);
  REQUIRE_EQ((int64_t)700, parsed.lastSourceSearch);
  REQUIRE_EQ((uint32_t)3, parsed.sourceSearchCount);
  REQUIRE_EQ((uint32_t)0x61726961, parsed.udpVerifyKey);
  REQUIRE(!parsed.firewalled);
  REQUIRE_EQ((size_t)1, parsed.observedAddresses.size());
  REQUIRE_EQ(std::string("203.0.113.55"),
                       parsed.observedAddresses[0]);
  REQUIRE_EQ((size_t)1, parsed.routerNodes.size());
  REQUIRE_EQ(std::string("203.0.113.1"), parsed.routerNodes[0].host);
  REQUIRE_EQ((uint16_t)4672, parsed.routerNodes[0].port);
  REQUIRE_EQ((size_t)1, parsed.routerContacts.size());
  REQUIRE_EQ(routerContact.id, parsed.routerContacts[0].id);
  REQUIRE_EQ(std::string("203.0.113.2"),
                       parsed.routerContacts[0].host);
  REQUIRE_EQ((uint8_t)8, parsed.routerContacts[0].version);
  REQUIRE_EQ((uint32_t)0x55667788,
                       parsed.routerContacts[0].udpKey);
  REQUIRE_EQ((size_t)2, parsed.buckets.size());
  REQUIRE_EQ((int64_t)400, parsed.buckets[1].lastActive);
  REQUIRE_EQ((size_t)1, parsed.buckets[1].live.size());
  REQUIRE(parsed.buckets[1].live[0].confirmed);
  REQUIRE(parsed.buckets[1].live[0].seed);
  REQUIRE_EQ((uint32_t)3, parsed.buckets[1].live[0].failCount);
  REQUIRE_EQ((int64_t)50, parsed.buckets[1].live[0].firstSeen);
  REQUIRE_EQ((int64_t)75, parsed.buckets[1].live[0].lastSeen);
  REQUIRE_EQ(std::string("198.51.100.2"),
                       parsed.buckets[1].live[0].contact.host);
  REQUIRE_EQ((size_t)1, parsed.buckets[1].replacements.size());
  REQUIRE(!parsed.buckets[1].replacements[0].confirmed);
  REQUIRE_EQ(std::string("198.51.100.3"),
                       parsed.buckets[1].replacements[0].contact.host);

  REQUIRE(!parseKadRoutingStatePayload(
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
  state.lastSourceResponseTime = 200;
  state.lastSourceCount = 3;
  state.lastUdpSourceRequestTime = 210;
  state.failCount = 2;
  state.lastFailureTime = 100;
  state.nextRetryTime = 160;
  state.lastMessage = "hello";

  auto payload = createServerStatePayload(state);
  ServerState parsed;
  REQUIRE(parseServerStatePayload(parsed, payload));
  REQUIRE_EQ(std::string("203.0.113.10"), parsed.endpoint.host);
  REQUIRE_EQ((uint16_t)4661, parsed.endpoint.port);
  REQUIRE_EQ(std::string("Peer Server"), parsed.name);
  REQUIRE_EQ(std::string("Primary ED2K server"),
                       parsed.description);
  REQUIRE(parsed.connected);
  REQUIRE(parsed.handshakeCompleted);
  REQUIRE_EQ((uint32_t)0x0a000001, parsed.clientId);
  REQUIRE(parsed.highId);
  REQUIRE_EQ(std::string("1.0.0.10"), parsed.ipAddress);
  REQUIRE_EQ((uint32_t)0x55aa, parsed.tcpFlags);
  REQUIRE_EQ((uint32_t)1234, parsed.users);
  REQUIRE_EQ((uint32_t)5678, parsed.files);
  REQUIRE_EQ((uint32_t)9000, parsed.maxUsers);
  REQUIRE_EQ((uint32_t)100, parsed.softFiles);
  REQUIRE_EQ((uint32_t)200, parsed.hardFiles);
  REQUIRE_EQ((uint32_t)0x01020304, parsed.udpFlags);
  REQUIRE_EQ((uint32_t)77, parsed.lowIdUsers);
  REQUIRE_EQ((uint16_t)4665, parsed.udpObfuscationPort);
  REQUIRE_EQ((uint16_t)4666, parsed.tcpObfuscationPort);
  REQUIRE_EQ((uint32_t)0x11223344, parsed.udpKey);
  REQUIRE_EQ((uint32_t)0x55aa0011, parsed.udpStatusChallenge);
  REQUIRE_EQ((int64_t)120, parsed.lastUdpStatusTime);
  REQUIRE_EQ((int64_t)180, parsed.nextSourceRequestTime);
  REQUIRE_EQ((int64_t)200, parsed.lastSourceResponseTime);
  REQUIRE_EQ((uint32_t)3, parsed.lastSourceCount);
  REQUIRE_EQ((int64_t)210, parsed.lastUdpSourceRequestTime);
  REQUIRE_EQ((uint32_t)2, parsed.failCount);
  REQUIRE_EQ((int64_t)100, parsed.lastFailureTime);
  REQUIRE_EQ((int64_t)160, parsed.nextRetryTime);
  REQUIRE_EQ(std::string("hello"), parsed.lastMessage);

  std::string v1Payload = payload;
  v1Payload.replace(sizeof("A2ED2KSRV") - 1, 4, packUInt32(1));
  v1Payload.erase(sizeof("A2ED2KSRV") - 1 + 4 + 2 + server.host.size() + 2,
                  2 + state.name.size() + 2 + state.description.size());
  v1Payload.erase(sizeof("A2ED2KSRV") - 1 + 4 + 2 + server.host.size() + 2 +
                  2 + 4 + 1 + 2 + state.ipAddress.size() + 4 + 4 + 4 + 4 +
                  4 + 4 + 4 + 4 + 2 + 2 + 4 + 4 + 8,
                  8 + 4 + 8 + 8);
  REQUIRE(parseServerStatePayload(parsed, v1Payload));
  REQUIRE_EQ((int64_t)0, parsed.nextSourceRequestTime);
  REQUIRE_EQ((int64_t)0, parsed.lastSourceResponseTime);
  REQUIRE_EQ((uint32_t)0, parsed.lastSourceCount);
  REQUIRE_EQ((int64_t)0, parsed.lastUdpSourceRequestTime);
  REQUIRE(parsed.name.empty());
  REQUIRE(parsed.description.empty());

  REQUIRE(!parseServerStatePayload(parsed,
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
  REQUIRE(parseNodesDat(nodes, payload));
  REQUIRE_EQ((uint32_t)3, nodes.version);
  REQUIRE_EQ((uint32_t)1, nodes.bootstrapEdition);
  REQUIRE_EQ((size_t)1, nodes.contacts.size());
  REQUIRE_EQ(std::string("203.0.113.1"), nodes.contacts[0].host);
  REQUIRE_EQ((uint16_t)4672, nodes.contacts[0].udpPort);
  REQUIRE_EQ((size_t)1, nodes.verified.size());
  REQUIRE(nodes.verified[0]);

  std::string amuleV2;
  amuleV2 += packUInt32(0);
  amuleV2 += packUInt32(2);
  amuleV2 += packUInt32(1);
  amuleV2 += nodeId;
  amuleV2 += std::string("\x05\x18\x9f\x01", 4);
  amuleV2 += packUInt16(4672);
  amuleV2 += packUInt16(4662);
  amuleV2.push_back('\x08');
  amuleV2 += packUInt64(0);
  amuleV2.push_back('\0');
  REQUIRE(parseNodesDat(nodes, amuleV2));
  REQUIRE_EQ((size_t)1, nodes.contacts.size());
  REQUIRE_EQ(std::string("1.159.24.5"), nodes.contacts[0].host);

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
  REQUIRE(parseNodesDat(nodes, normal));
  REQUIRE_EQ((uint32_t)2, nodes.version);
  REQUIRE_EQ((size_t)1, nodes.verified.size());
  REQUIRE(nodes.verified[0]);

  REQUIRE(!parseNodesDat(nodes, packUInt32(0) + packUInt32(2) +
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

  REQUIRE_EQ((size_t)1, servers.size());
  REQUIRE_EQ(std::string("1.2.3.4"), servers[0].host);
  REQUIRE_EQ((uint16_t)4661, servers[0].port);

  auto entries = parseServerMetEntries(data);

  REQUIRE_EQ((size_t)1, entries.size());
  REQUIRE_EQ(std::string("1.2.3.4"), entries[0].endpoint.host);
  REQUIRE_EQ((uint16_t)4661, entries[0].endpoint.port);
  REQUIRE_EQ(std::string("Peer Server"), entries[0].name);
  REQUIRE_EQ(std::string("Primary ED2K server"),
                       entries[0].description);
  REQUIRE_EQ((uint32_t)9000, entries[0].maxUsers);
  REQUIRE_EQ((uint32_t)100, entries[0].softFiles);
  REQUIRE_EQ((uint32_t)200, entries[0].hardFiles);
  REQUIRE_EQ((uint32_t)0x01020304, entries[0].udpFlags);
  REQUIRE_EQ((uint32_t)77, entries[0].lowIdUsers);
  REQUIRE_EQ((uint32_t)0x11223344, entries[0].udpKey);

  data[0] = static_cast<char>(0xe0);
  entries = parseServerMetEntries(data);

  REQUIRE_EQ((size_t)1, entries.size());
  REQUIRE_EQ(std::string("1.2.3.4"), entries[0].endpoint.host);
  REQUIRE_EQ((uint16_t)4661, entries[0].endpoint.port);
  REQUIRE_EQ(std::string("Peer Server"), entries[0].name);

  std::string hostnameEntry;
  hostnameEntry.push_back('\x0e');
  hostnameEntry += packUInt32(1);
  hostnameEntry += packUInt32(0);
  hostnameEntry += packUInt16(4661);
  hostnameEntry += packUInt32(2);
  hostnameEntry += createStringTag(0x85, "peer.example.org");
  hostnameEntry += createStringTag(0x01, "Hostname Server");

  entries = parseServerMetEntries(hostnameEntry);

  REQUIRE_EQ((size_t)1, entries.size());
  REQUIRE_EQ(std::string("peer.example.org"),
                       entries[0].endpoint.host);
  REQUIRE_EQ((uint16_t)4661, entries[0].endpoint.port);
  REQUIRE_EQ(std::string("Hostname Server"), entries[0].name);
}

void Ed2kHelperTest::testMd4Digest()
{
  REQUIRE_EQ(
      std::string("31d6cfe0d16ae931b73c59d7e0c089c0"),
      util::toHex(md4Digest("")));
  REQUIRE_EQ(
      std::string("bde52cb31de33e46245e05fbdbd6fb24"),
      util::toHex(md4Digest("a")));
  REQUIRE_EQ(
      std::string("a448017aaf21d8525fc10ae87aa6729d"),
      util::toHex(md4Digest("abc")));
  REQUIRE_EQ(
      std::string("d9130a8164549fe818874806e1c7014b"),
      util::toHex(md4Digest("message digest")));
}

void Ed2kHelperTest::testRootHash()
{
  std::vector<std::string> empty;
  REQUIRE_EQ(
      std::string("31d6cfe0d16ae931b73c59d7e0c089c0"),
      util::toHex(rootHash(empty)));

  std::vector<std::string> single{md4Digest("abc")};
  REQUIRE_EQ(util::toHex(md4Digest("abc")), util::toHex(rootHash(single)));

  std::vector<std::string> multiple{md4Digest("first part"),
                                    md4Digest("second part")};
  std::string concat = multiple[0] + multiple[1];
  REQUIRE_EQ(util::toHex(md4Digest(concat)),
                       util::toHex(rootHash(multiple)));
}

void Ed2kHelperTest::testHashSetPartCount()
{
  REQUIRE_EQ((size_t)0, hashSetPartCount(1));
  REQUIRE_EQ((size_t)0,
                       hashSetPartCount(static_cast<int64_t>(PIECE_LENGTH) -
                                        1));
  REQUIRE_EQ((size_t)2,
                       hashSetPartCount(static_cast<int64_t>(PIECE_LENGTH)));
  REQUIRE_EQ((size_t)2,
                       hashSetPartCount(static_cast<int64_t>(PIECE_LENGTH) +
                                        1));
  REQUIRE_EQ((size_t)3,
                       hashSetPartCount(static_cast<int64_t>(PIECE_LENGTH) *
                                        2));
}

} // namespace ed2k

} // namespace aria2
