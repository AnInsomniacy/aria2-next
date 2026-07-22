#include "HandshakeExtensionMessage.h"

#include <iostream>

#include "a2doctest.h"

#include "Peer.h"
#include "Exception.h"
#include "FileEntry.h"
#include "DownloadContext.h"
#include "bittorrent_helper.h"
#include "Option.h"
#include "RequestGroup.h"

namespace aria2 {

class HandshakeExtensionMessageTest {


public:
  void setUp() {}

  void tearDown() {}

  void testGetExtensionMessageID();
  void testGetExtensionName();
  void testGetBencodedData();
  void testToString();
  void testDoReceivedAction();
  void testCreate();
  void testCreate_stringnum();
};

A2_TEST(HandshakeExtensionMessageTest, testGetExtensionMessageID)
A2_TEST(HandshakeExtensionMessageTest, testGetExtensionName)
A2_TEST(HandshakeExtensionMessageTest, testGetBencodedData)
A2_TEST(HandshakeExtensionMessageTest, testToString)
A2_TEST(HandshakeExtensionMessageTest, testDoReceivedAction)
A2_TEST(HandshakeExtensionMessageTest, testCreate)
A2_TEST(HandshakeExtensionMessageTest, testCreate_stringnum)

void HandshakeExtensionMessageTest::testGetExtensionMessageID()
{
  HandshakeExtensionMessage msg;
  REQUIRE_EQ((uint8_t)0, msg.getExtensionMessageID());
}

void HandshakeExtensionMessageTest::testGetExtensionName()
{
  HandshakeExtensionMessage msg;
  REQUIRE_EQ(std::string("handshake"),
                       std::string(msg.getExtensionName()));
}

void HandshakeExtensionMessageTest::testGetBencodedData()
{
  HandshakeExtensionMessage msg;
  msg.setClientVersion("aria2");
  msg.setTCPPort(6889);
  msg.setExtension(ExtensionMessageRegistry::UT_PEX, 1);
  msg.setExtension(ExtensionMessageRegistry::UT_METADATA, 2);
  msg.setMetadataSize(1_k);
  REQUIRE_EQ(std::string("d"
                                   "1:md11:ut_metadatai2e6:ut_pexi1ee"
                                   "13:metadata_sizei1024e"
                                   "1:pi6889e"
                                   "1:v5:aria2"
                                   "e"),
                       msg.getPayload());

  msg.setMetadataSize(0);
  REQUIRE(msg.getPayload().find("metadata_size") == std::string::npos);
}

void HandshakeExtensionMessageTest::testToString()
{
  HandshakeExtensionMessage msg;
  msg.setClientVersion("aria2");
  msg.setTCPPort(6889);
  msg.setExtension(ExtensionMessageRegistry::UT_PEX, 1);
  msg.setExtension(ExtensionMessageRegistry::UT_METADATA, 2);
  msg.setMetadataSize(1_k);
  REQUIRE_EQ(
      std::string("handshake client=aria2, tcpPort=6889, metadataSize=1024,"
                  " ut_metadata=2, ut_pex=1"),
      msg.toString());
}

void HandshakeExtensionMessageTest::testDoReceivedAction()
{
  auto dctx = std::make_shared<DownloadContext>(METADATA_PIECE_SIZE, 0);
  auto op = std::make_shared<Option>();
  RequestGroup rg(GroupId::create(), op);
  rg.setDownloadContext(dctx);

  dctx->setAttribute(CTX_ATTR_BT, make_unique<TorrentAttribute>());
  dctx->markTotalLengthIsUnknown();

  auto peer = std::make_shared<Peer>("192.168.0.1", 0, true);
  peer->allocateSessionResource(1_k, 1_m);
  HandshakeExtensionMessage msg;
  msg.setClientVersion("aria2");
  msg.setTCPPort(6889);
  msg.setExtension(ExtensionMessageRegistry::UT_PEX, 1);
  msg.setExtension(ExtensionMessageRegistry::UT_METADATA, 3);
  msg.setMetadataSize(1_k);
  msg.setPeer(peer);
  msg.setDownloadContext(dctx.get());

  msg.doReceivedAction();

  REQUIRE_EQ(std::string("aria2"), peer->getClientName());
  REQUIRE_EQ((uint16_t)6889, peer->getPort());
  REQUIRE(!peer->isIncomingPeer());
  REQUIRE(peer->isIncomingConnection());
  REQUIRE_EQ((uint8_t)1, peer->getExtensionMessageID(
                                       ExtensionMessageRegistry::UT_PEX));
  REQUIRE_EQ((uint8_t)3, peer->getExtensionMessageID(
                                       ExtensionMessageRegistry::UT_METADATA));
  REQUIRE(peer->isSeeder());
  auto attrs = bittorrent::getTorrentAttrs(dctx);
  REQUIRE_EQ((size_t)1_k, attrs->metadataSize);
  REQUIRE_EQ((int64_t)1_k, dctx->getTotalLength());
  REQUIRE(dctx->knowsTotalLength());

  // See Peer is not marked as seeder if !attrs->metadata.empty()
  peer->allocateSessionResource(1_k, 1_m);
  attrs->metadataSize = 1_k;
  attrs->metadata = std::string('0', attrs->metadataSize);
  msg.doReceivedAction();
  REQUIRE(!peer->isSeeder());
}

void HandshakeExtensionMessageTest::testCreate()
{
  std::string in =
      "0d1:pi6881e1:v5:aria21:md5:a2dhti2e6:ut_pexi1ee13:metadata_sizei1024ee";
  std::shared_ptr<HandshakeExtensionMessage> m(
      HandshakeExtensionMessage::create(
          reinterpret_cast<const unsigned char*>(in.c_str()), in.size()));
  REQUIRE_EQ(std::string("aria2"), m->getClientVersion());
  REQUIRE_EQ((uint16_t)6881, m->getTCPPort());
  REQUIRE_EQ(
      (uint8_t)1, m->getExtensionMessageID(ExtensionMessageRegistry::UT_PEX));
  REQUIRE_EQ((size_t)1_k, m->getMetadataSize());
  try {
    // bad payload format
    std::string in = "011:hello world";
    HandshakeExtensionMessage::create(
        reinterpret_cast<const unsigned char*>(in.c_str()), in.size());
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
  try {
    // malformed dencoded message
    std::string in = "011:hello";
    HandshakeExtensionMessage::create(
        reinterpret_cast<const unsigned char*>(in.c_str()), in.size());
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
  try {
    // 0 length data
    std::string in = "";
    HandshakeExtensionMessage::create(
        reinterpret_cast<const unsigned char*>(in.c_str()), in.size());
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

void HandshakeExtensionMessageTest::testCreate_stringnum()
{
  std::string in = "0d1:p4:68811:v5:aria21:md6:ut_pex1:1ee";
  std::shared_ptr<HandshakeExtensionMessage> m(
      HandshakeExtensionMessage::create(
          reinterpret_cast<const unsigned char*>(in.c_str()), in.size()));
  REQUIRE_EQ(std::string("aria2"), m->getClientVersion());
  // port number in string is not allowed
  REQUIRE_EQ((uint16_t)0, m->getTCPPort());
  // extension ID in string is not allowed
  REQUIRE_EQ(
      (uint8_t)0, m->getExtensionMessageID(ExtensionMessageRegistry::UT_PEX));
}

} // namespace aria2
