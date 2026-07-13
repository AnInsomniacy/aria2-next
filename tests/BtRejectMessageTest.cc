#include "BtRejectMessage.h"

#include <cstring>

#include "a2doctest.h"

#include "bittorrent_helper.h"
#include "Peer.h"
#include "FileEntry.h"
#include "MockBtMessageDispatcher.h"

namespace aria2 {

class BtRejectMessageTest {


private:
public:
  void testCreate();
  void testCreateMessage();
  void testDoReceivedAction();
  void testDoReceivedActionNoMatch();
  void testDoReceivedActionFastExtensionDisabled();
  void testToString();

  class MockBtMessageDispatcher2 : public MockBtMessageDispatcher {
  public:
    std::unique_ptr<RequestSlot> slot;

    void setRequestSlot(std::unique_ptr<RequestSlot> s) { slot = std::move(s); }

    virtual const RequestSlot*
    getOutstandingRequest(size_t index, int32_t begin,
                          int32_t length) override
    {
      if (slot && slot->getIndex() == index && slot->getBegin() == begin &&
          slot->getLength() == length) {
        return slot.get();
      }
      else {
        return nullptr;
      }
    }

    virtual void removeOutstandingRequest(const RequestSlot* s) override
    {
      if (slot->getIndex() == s->getIndex() &&
          slot->getBegin() == s->getBegin() &&
          slot->getLength() == s->getLength()) {
        slot.reset();
      }
    }
  };

  typedef std::shared_ptr<MockBtMessageDispatcher2>
      MockBtMessageDispatcher2Handle;

  std::shared_ptr<Peer> peer;
  std::shared_ptr<MockBtMessageDispatcher2> dispatcher;
  std::shared_ptr<BtRejectMessage> msg;

  void setUp()
  {
    peer.reset(new Peer("host", 6969));
    peer->allocateSessionResource(1_k, 1_m);

    dispatcher.reset(new MockBtMessageDispatcher2());

    msg.reset(new BtRejectMessage());
    msg->setPeer(peer);
    msg->setIndex(1);
    msg->setBegin(16);
    msg->setLength(32);
    msg->setBtMessageDispatcher(dispatcher.get());
  }
};

A2_TEST(BtRejectMessageTest, testCreate)
A2_TEST(BtRejectMessageTest, testCreateMessage)
A2_TEST(BtRejectMessageTest, testDoReceivedAction)
A2_TEST(BtRejectMessageTest, testDoReceivedActionNoMatch)
A2_TEST(BtRejectMessageTest, testDoReceivedActionFastExtensionDisabled)
A2_TEST(BtRejectMessageTest, testToString)

void BtRejectMessageTest::testCreate()
{
  unsigned char msg[17];
  bittorrent::createPeerMessageString(msg, sizeof(msg), 13, 16);
  bittorrent::setIntParam(&msg[5], 12345);
  bittorrent::setIntParam(&msg[9], 256);
  bittorrent::setIntParam(&msg[13], 1_k);
  std::shared_ptr<BtRejectMessage> pm(BtRejectMessage::create(&msg[4], 13));
  REQUIRE_EQ((uint8_t)16, pm->getId());
  REQUIRE_EQ((size_t)12345, pm->getIndex());
  REQUIRE_EQ(256, pm->getBegin());
  REQUIRE_EQ((int32_t)1_k, pm->getLength());

  // case: payload size is wrong
  try {
    unsigned char msg[18];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 14, 16);
    BtRejectMessage::create(&msg[4], 14);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
  // case: id is wrong
  try {
    unsigned char msg[17];
    bittorrent::createPeerMessageString(msg, sizeof(msg), 13, 17);
    BtRejectMessage::create(&msg[4], 13);
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtRejectMessageTest::testCreateMessage()
{
  BtRejectMessage msg;
  msg.setIndex(12345);
  msg.setBegin(256);
  msg.setLength(1_k);
  unsigned char data[17];
  bittorrent::createPeerMessageString(data, sizeof(data), 13, 16);
  bittorrent::setIntParam(&data[5], 12345);
  bittorrent::setIntParam(&data[9], 256);
  bittorrent::setIntParam(&data[13], 1_k);
  auto rawmsg = msg.createMessage();
  REQUIRE_EQ((size_t)17, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtRejectMessageTest::testDoReceivedAction()
{
  peer->setFastExtensionEnabled(true);
  dispatcher->setRequestSlot(make_unique<RequestSlot>(1, 16, 32, 2));

  REQUIRE(dispatcher->getOutstandingRequest(1, 16, 32));

  msg->doReceivedAction();

  REQUIRE(!dispatcher->getOutstandingRequest(1, 16, 32));
}

void BtRejectMessageTest::testDoReceivedActionNoMatch()
{
  peer->setFastExtensionEnabled(true);
  dispatcher->setRequestSlot(make_unique<RequestSlot>(2, 16, 32, 2));

  REQUIRE(dispatcher->getOutstandingRequest(2, 16, 32));

  msg->doReceivedAction();

  REQUIRE(dispatcher->getOutstandingRequest(2, 16, 32));
}

void BtRejectMessageTest::testDoReceivedActionFastExtensionDisabled()
{
  RequestSlot slot(1, 16, 32, 2);
  dispatcher->setRequestSlot(make_unique<RequestSlot>(1, 16, 32, 2));

  REQUIRE(dispatcher->getOutstandingRequest(1, 16, 32));
  try {
    msg->doReceivedAction();
    FAIL("exception must be thrown.");
  }
  catch (...) {
  }
}

void BtRejectMessageTest::testToString()
{
  REQUIRE_EQ(std::string("reject index=1, begin=16, length=32"),
                       msg->toString());
}

} // namespace aria2
