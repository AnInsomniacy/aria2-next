#include "BtKeepAliveMessage.h"
#include <cstring>
#include "a2doctest.h"

namespace aria2 {

class BtKeepAliveMessageTest {


private:
public:
  void setUp() {}

  void testCreateMessage();
  void testToString();
};

A2_TEST(BtKeepAliveMessageTest, testCreateMessage)
A2_TEST(BtKeepAliveMessageTest, testToString)

void BtKeepAliveMessageTest::testCreateMessage()
{
  char data[4];
  memset(data, 0, sizeof(data));
  BtKeepAliveMessage message;
  REQUIRE_EQ((uint8_t)99, message.getId());
  auto rawmsg = message.createMessage();
  REQUIRE_EQ((size_t)4, rawmsg.size());
  REQUIRE(std::equal(std::begin(rawmsg), std::end(rawmsg), data));
}

void BtKeepAliveMessageTest::testToString()
{
  BtKeepAliveMessage msg;
  REQUIRE_EQ(std::string("keep alive"), msg.toString());
}

} // namespace aria2
