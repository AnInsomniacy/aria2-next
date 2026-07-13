#include "DHTUnknownMessage.h"
#include "DHTNode.h"
#include "Exception.h"
#include "a2doctest.h"

namespace aria2 {

class DHTUnknownMessageTest {


public:
  void setUp() {}

  void tearDown() {}

  void testToString();
};

A2_TEST(DHTUnknownMessageTest, testToString)

void DHTUnknownMessageTest::testToString()
{
  std::shared_ptr<DHTNode> localNode(new DHTNode());
  std::string ipaddr = "192.168.0.1";
  uint16_t port = 6881;

  {
    // data.size() > 8
    std::string data = "chocolate";
    DHTUnknownMessage msg(localNode,
                          reinterpret_cast<const unsigned char*>(data.c_str()),
                          data.size(), ipaddr, port);

    REQUIRE_EQ(
        std::string("dht unknown Remote:192.168.0.1(6881) length=9, first 8 "
                    "bytes(hex)=63686f636f6c6174"),
        msg.toString());
  }
  {
    // data.size() == 3
    std::string data = "foo";
    DHTUnknownMessage msg(localNode,
                          reinterpret_cast<const unsigned char*>(data.c_str()),
                          data.size(), ipaddr, port);

    REQUIRE_EQ(std::string("dht unknown Remote:192.168.0.1(6881) "
                                     "length=3, first 8 bytes(hex)=666f6f"),
                         msg.toString());
  }
}

} // namespace aria2
