#include "DHTConnectionImpl.h"

#include <iostream>
#include "a2doctest.h"

#include "Exception.h"
#include "SocketCore.h"

namespace aria2 {

class DHTConnectionImplTest {


public:
  void setUp() {}

  void tearDown() {}

  void testWriteAndReadData();
};

A2_TEST(DHTConnectionImplTest, testWriteAndReadData)

void DHTConnectionImplTest::testWriteAndReadData()
{
  try {
    DHTConnectionImpl con1(AF_INET);
    uint16_t con1port = 0;
    REQUIRE(con1.bind(con1port, ""));

    DHTConnectionImpl con2(AF_INET);
    uint16_t con2port = 0;
    REQUIRE(con2.bind(con2port, ""));

    std::string message1 = "hello world.";
    // hostname should be "localhost", not 127.0.0.1. Test failed on Mac OSX10.5
    con1.sendMessage(reinterpret_cast<const unsigned char*>(message1.c_str()),
                     message1.size(), "localhost", con2port);

    unsigned char readbuffer[100];
    std::string remoteHost;
    uint16_t remotePort;
    {
      while (!con2.getSocket()->isReadable(0))
        ;
      ssize_t rlength = con2.receiveMessage(readbuffer, sizeof(readbuffer),
                                            remoteHost, remotePort);
      REQUIRE_EQ((ssize_t)message1.size(), rlength);
      REQUIRE_EQ(message1,
                           std::string(&readbuffer[0], &readbuffer[rlength]));
    }
  }
  catch (Exception& e) {
    FAIL(e.stackTrace());
  }
}

} // namespace aria2
