#include "LpdMessageReceiver.h"

#include <cstring>

#include "a2doctest.h"

#include "TestUtil.h"
#include "Exception.h"
#include "util.h"
#include "LpdMessageReceiver.h"
#include "SocketCore.h"
#include "BtConstants.h"
#include "LpdMessage.h"
#include "LpdMessageDispatcher.h"
#include "Peer.h"

namespace aria2 {

class LpdMessageReceiverTest {


public:
  void testReceiveMessage();
};

A2_TEST(LpdMessageReceiverTest, testReceiveMessage)

void LpdMessageReceiverTest::testReceiveMessage()
{
  LpdMessageReceiver rcv(LPD_MULTICAST_ADDR, LPD_MULTICAST_PORT);
  REQUIRE(rcv.init(""));

  std::shared_ptr<SocketCore> sendsock(new SocketCore(SOCK_DGRAM));
  sendsock->create(AF_INET);
  // Mingw32 build needs to set interface explicitly.
  sendsock->setMulticastInterface("");
  sendsock->setMulticastTtl(0);

  std::string infoHashString = "cd41c7fdddfd034a15a04d7ff881216e01c4ceaf";
  std::string infoHash = fromHex(infoHashString);
  std::string request = bittorrent::createLpdRequest(
      LPD_MULTICAST_ADDR, LPD_MULTICAST_PORT, infoHash, 6000);

  sendsock->writeData(request.c_str(), request.size(), LPD_MULTICAST_ADDR,
                      LPD_MULTICAST_PORT);

  rcv.getSocket()->isReadable(5);
  auto msg = rcv.receiveMessage();
  REQUIRE(msg);
  REQUIRE_EQ(std::string("cd41c7fdddfd034a15a04d7ff881216e01c4ceaf"),
                       util::toHex(msg->infoHash));
  REQUIRE_EQ((uint16_t)6000, msg->peer->getPort());

  // Bad infohash
  std::string badInfoHashString = "cd41c7fdddfd034a15a04d7ff881216e01c4ce";
  request = bittorrent::createLpdRequest(LPD_MULTICAST_ADDR, LPD_MULTICAST_PORT,
                                         fromHex(badInfoHashString), 6000);
  sendsock->writeData(request.c_str(), request.size(), LPD_MULTICAST_ADDR,
                      LPD_MULTICAST_PORT);

  rcv.getSocket()->isReadable(5);
  msg = rcv.receiveMessage();
  REQUIRE(!msg);

  // Bad port
  request = bittorrent::createLpdRequest(LPD_MULTICAST_ADDR, LPD_MULTICAST_PORT,
                                         infoHash, 0);
  sendsock->writeData(request.c_str(), request.size(), LPD_MULTICAST_ADDR,
                      LPD_MULTICAST_PORT);

  rcv.getSocket()->isReadable(5);
  msg = rcv.receiveMessage();
  REQUIRE(!msg);

  // No data available
  msg = rcv.receiveMessage();
  REQUIRE(!msg);
}

} // namespace aria2
