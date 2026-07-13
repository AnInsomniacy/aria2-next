#include "DHTTokenTracker.h"

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "DHTConstants.h"

namespace aria2 {

class DHTTokenTrackerTest {


public:
  void setUp() {}

  void tearDown() {}

  void testGenerateToken();
};

A2_TEST(DHTTokenTrackerTest, testGenerateToken)

void DHTTokenTrackerTest::testGenerateToken()
{
  unsigned char infohash[DHT_ID_LENGTH];
  util::generateRandomData(infohash, DHT_ID_LENGTH);
  std::string ipaddr = "192.168.0.1";
  uint16_t port = 6881;

  DHTTokenTracker tracker;
  std::string token = tracker.generateToken(infohash, ipaddr, port);
  REQUIRE(tracker.validateToken(token, infohash, ipaddr, port));

  tracker.updateTokenSecret();
  REQUIRE(tracker.validateToken(token, infohash, ipaddr, port));
  std::string newtoken = tracker.generateToken(infohash, ipaddr, port);
  tracker.updateTokenSecret();
  REQUIRE(!tracker.validateToken(token, infohash, ipaddr, port));
  REQUIRE(tracker.validateToken(newtoken, infohash, ipaddr, port));
}

} // namespace aria2
