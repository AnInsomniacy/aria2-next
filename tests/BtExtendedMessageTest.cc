#include "BtExtendedMessage.h"

#include <cstring>
#include <iostream>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "MockExtensionMessageFactory.h"
#include "Peer.h"
#include "Exception.h"

namespace aria2 {

class BtExtendedMessageTest {


private:
public:
  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testToString();
};

A2_TEST(BtExtendedMessageTest, testCreate)
A2_TEST(BtExtendedMessageTest, testCreateMessage)
A2_TEST(BtExtendedMessageTest, testDoReceivedAction)
A2_TEST(BtExtendedMessageTest, testToString)

void BtExtendedMessageTest::testCreate()
{
  auto peer = std::make_shared<Peer>("192.168.0.1", 6969);
  peer->allocateSessionResource(1_k, 1_m);
  auto exmsgFactory = MockExtensionMessageFactory{};

  // payload:{4:name3:foo}->11bytes
  std::string payload = "4:name3:foo";
  unsigned char msg[17]; // 6+11bytes
  bittorrent::createPeerMessageString((unsigned char*)msg, sizeof(msg), 13, 20);
  msg[5] = 1; // Set dummy extended message ID 1
  memcpy(msg + 6, payload.c_str(), payload.size());
  auto pm = BtExtendedMessage::create(&exmsgFactory, peer, &msg[4], 13);
  REQUIRE_EQ((uint8_t)20, pm->getId());

  // case: payload size is wrong
  try {
    unsigned char msg[5];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 1, 20);
    BtExtendedMessage::create(&exmsgFactory, peer, &msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
  // case: id is wrong
  try {
    unsigned char msg[6];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 2, 21);
    BtExtendedMessage::create(&exmsgFactory, peer, &msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

void BtExtendedMessageTest::testCreateMessage()
{
  std::string payload = "4:name3:foo";
  uint8_t extendedMessageID = 1;
  BtExtendedMessage msg{make_unique<MockExtensionMessage>(
      "charlie", extendedMessageID, payload, nullptr)};
  unsigned char data[17];
  bittorrent::createPeerMessageString(data, sizeof(data), 13, 20);
  *(data + 5) = extendedMessageID;
  memcpy(data + 6, payload.c_str(), payload.size());
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)17, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtExtendedMessageTest::testDoReceivedAction()
{
  auto evcheck = MockExtensionMessageEventCheck{};
  BtExtendedMessage msg{
      make_unique<MockExtensionMessage>("charlie", 1, "", &evcheck)};
  msg.doReceivedAction();
  REQUIRE(evcheck.doReceivedActionCalled);
}

void BtExtendedMessageTest::testToString()
{
  std::string payload = "4:name3:foo";
  uint8_t extendedMessageID = 1;
  BtExtendedMessage msg{make_unique<MockExtensionMessage>(
      "charlie", extendedMessageID, payload, nullptr)};
  REQUIRE_EQ(std::string("extended charlie"), msg.toString());
}

} // namespace aria2
