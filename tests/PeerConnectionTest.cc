#include "PeerConnection.h"

#include <cstring>

#include "a2doctest.h"

#include "Peer.h"
#include "SocketCore.h"

namespace aria2 {

class PeerConnectionTest {


public:
  void testReserveBuffer();
};

A2_TEST(PeerConnectionTest, testReserveBuffer)

void PeerConnectionTest::testReserveBuffer()
{
  PeerConnection con(1, std::shared_ptr<Peer>(), std::shared_ptr<SocketCore>());
  con.presetBuffer((unsigned char*)"foo", 3);
  REQUIRE_EQ((size_t)MAX_BUFFER_CAPACITY, con.getBufferCapacity());
  REQUIRE_EQ((size_t)3, con.getBufferLength());

  constexpr size_t newLength = 128_k;
  con.reserveBuffer(newLength);

  REQUIRE_EQ(newLength, con.getBufferCapacity());
  REQUIRE_EQ((size_t)3, con.getBufferLength());
  REQUIRE(memcmp("foo", con.getBuffer(), 3) == 0);
}

} // namespace aria2
