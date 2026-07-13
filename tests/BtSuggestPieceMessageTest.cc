#include "BtSuggestPieceMessage.h"

#include <cstring>
#include "a2doctest.h"

#include "bittorrent_helper.h"

namespace aria2 {

class BtSuggestPieceMessageTest {


private:
public:
  void setUp() {}

  void testCreate();
  void testCreateMessage();
  void testToString();
};

A2_TEST(BtSuggestPieceMessageTest, testCreate)
A2_TEST(BtSuggestPieceMessageTest, testCreateMessage)
A2_TEST(BtSuggestPieceMessageTest, testToString)

void BtSuggestPieceMessageTest::testCreate()
{
  unsigned char msg[9];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 5, 13);
  bittorrent::setIntParam(&msg[5], 12345);
  auto pm = BtSuggestPieceMessage::create(&msg[4], 5);
  REQUIRE(BtSuggestPieceMessage::ID == pm->getId());
  REQUIRE_EQ((size_t)12345, pm->getIndex());

  // case: payload size is wrong
  try {
    unsigned char msg[10];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 6, 13);
    BtSuggestPieceMessage::create(&msg[4], 2);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[9];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 5, 14);
    BtSuggestPieceMessage::create(&msg[4], 1);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtSuggestPieceMessageTest::testCreateMessage()
{
  BtSuggestPieceMessage msg;
  msg.setIndex(12345);
  unsigned char data[9];
  bittorrent::createPeerMessageString(data, sizeof(data), 5, 13);
  bittorrent::setIntParam(&data[5], 12345);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)9, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtSuggestPieceMessageTest::testToString()
{
  BtSuggestPieceMessage msg;
  msg.setIndex(12345);

  REQUIRE_EQ(std::string("suggest piece index=12345"),
                       msg.toString());
}

} // namespace aria2
