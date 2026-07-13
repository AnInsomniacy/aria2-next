#include "BtHandshakeMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "util.h"
#include "BtConstants.h"

namespace aria2 {

class BtHandshakeMessageTest {


private:
public:
  void setUp() {}

  void testCreate();
  void testCreateMessage();
  void testToString();
  void testSetDHTEnabled();

  static std::string BTPSTR;
};

std::string BtHandshakeMessageTest::BTPSTR = "BitTorrent protocol";

A2_TEST(BtHandshakeMessageTest, testCreate)
A2_TEST(BtHandshakeMessageTest, testCreateMessage)
A2_TEST(BtHandshakeMessageTest, testToString)
A2_TEST(BtHandshakeMessageTest, testSetDHTEnabled)

void createHandshakeMessageData(unsigned char* msg)
{
  msg[0] = 19;
  memcpy(&msg[1], BtHandshakeMessageTest::BTPSTR.c_str(),
         BtHandshakeMessageTest::BTPSTR.size());
  unsigned char reserved[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x04};
  memcpy(&msg[20], reserved, sizeof(reserved));
  unsigned char infoHash[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  memcpy(&msg[28], infoHash, sizeof(infoHash));
  unsigned char peerId[] = {0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
                            0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
                            0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0};
  memcpy(&msg[48], peerId, sizeof(peerId));
}

void BtHandshakeMessageTest::testCreate()
{
  unsigned char msg[68];
  createHandshakeMessageData(msg);
  std::shared_ptr<BtHandshakeMessage> message =
      BtHandshakeMessage::create(&msg[0], sizeof(msg));
  REQUIRE_EQ((uint8_t)INT8_MAX, message->getId());
  REQUIRE_EQ((uint8_t)19, message->getPstrlen());
  REQUIRE_EQ(
      util::toHex((const unsigned char*)BTPSTR.c_str(), BTPSTR.size()),
      util::toHex(message->getPstr(), BtHandshakeMessage::PSTR_LENGTH));
  REQUIRE_EQ(
      std::string("0000000000100004"),
      util::toHex(message->getReserved(), BtHandshakeMessage::RESERVED_LENGTH));
  REQUIRE_EQ(std::string("ffffffffffffffffffffffffffffffffffffffff"),
                       util::toHex(message->getInfoHash(), INFO_HASH_LENGTH));
  REQUIRE_EQ(std::string("f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0"),
                       util::toHex(message->getPeerId(), PEER_ID_LENGTH));
}

void BtHandshakeMessageTest::testCreateMessage()
{
  constexpr unsigned char infoHash[] = {
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  constexpr unsigned char peerId[] = {0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
                                      0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
                                      0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0};

  auto msg = std::make_shared<BtHandshakeMessage>();
  msg->setInfoHash(infoHash);
  msg->setPeerId(peerId);

  unsigned char data[68];
  createHandshakeMessageData(data);
  auto rawmsg = msg->createMessage();
  REQUIRE_EQ((size_t)68, rawmsg.size());
  REQUIRE_EQ(util::toHex((const unsigned char*)data, 68),
                       util::toHex(rawmsg.data(), 68));
}

void BtHandshakeMessageTest::testToString()
{
  unsigned char infoHash[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  unsigned char peerId[] = {0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
                            0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
                            0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0};

  BtHandshakeMessage msg;
  msg.setInfoHash(infoHash);
  msg.setPeerId(peerId);

  REQUIRE_EQ(
      std::string("handshake "
                  "peerId=%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%F0%"
                  "F0%F0%F0, reserved=0000000000100004"),
      msg.toString());
}

void BtHandshakeMessageTest::testSetDHTEnabled()
{
  BtHandshakeMessage msg;
  REQUIRE(!msg.isDHTEnabled());
  msg.setDHTEnabled(false);
  REQUIRE(!msg.isDHTEnabled());
  msg.setDHTEnabled(true);
  REQUIRE(msg.isDHTEnabled());
}

} // namespace aria2
