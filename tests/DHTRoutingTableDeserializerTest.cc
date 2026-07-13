#include "DHTRoutingTableDeserializer.h"

#include <cstring>
#include <sstream>
#include <iostream>

#include "a2doctest.h"

#include "DHTRoutingTableSerializer.h"
#include "Exception.h"
#include "util.h"
#include "DHTNode.h"
#include "array_fun.h"
#include "DHTConstants.h"
#include "a2netcompat.h"

namespace aria2 {

class DHTRoutingTableDeserializerTest {


public:
  void setUp() {}

  void tearDown() {}

  void testDeserialize();

  void testDeserialize6();
};

A2_TEST(DHTRoutingTableDeserializerTest, testDeserialize)
A2_TEST(DHTRoutingTableDeserializerTest, testDeserialize6)

void DHTRoutingTableDeserializerTest::testDeserialize()
{
  std::shared_ptr<DHTNode> localNode(new DHTNode());

  std::vector<std::shared_ptr<DHTNode>> nodes(3);
  for (size_t i = 0; i < nodes.size(); ++i) {
    nodes[i].reset(new DHTNode());
    nodes[i]->setIPAddress("192.168.0." + util::uitos(i + 1));
    nodes[i]->setPort(6881 + i);
  }
  nodes[1]->setIPAddress("non-numerical-name");

  DHTRoutingTableSerializer s(AF_INET);
  s.setLocalNode(localNode);
  s.setNodes(nodes);

  std::string filename =
      A2_TEST_OUT_DIR "/aria2_DHTRoutingTableDeserializerTest_testDeserialize";
  s.serialize(filename);

  DHTRoutingTableDeserializer d(AF_INET);
  d.deserialize(filename);

  REQUIRE(memcmp(localNode->getID(), d.getLocalNode()->getID(),
                        DHT_ID_LENGTH) == 0);

  REQUIRE_EQ((size_t)2, d.getNodes().size());
  const std::vector<std::shared_ptr<DHTNode>>& dsnodes = d.getNodes();
  REQUIRE_EQ(std::string("192.168.0.1"), dsnodes[0]->getIPAddress());
  REQUIRE_EQ((uint16_t)6881, dsnodes[0]->getPort());
  REQUIRE(
      memcmp(nodes[0]->getID(), dsnodes[0]->getID(), DHT_ID_LENGTH) == 0);
  REQUIRE_EQ(std::string("192.168.0.3"), dsnodes[1]->getIPAddress());
  REQUIRE_EQ((uint16_t)6883, dsnodes[1]->getPort());
  REQUIRE(
      memcmp(nodes[2]->getID(), dsnodes[1]->getID(), DHT_ID_LENGTH) == 0);
}

void DHTRoutingTableDeserializerTest::testDeserialize6()
{
  std::shared_ptr<DHTNode> localNode(new DHTNode());

  std::vector<std::shared_ptr<DHTNode>> nodes(3);
  for (size_t i = 0; i < nodes.size(); ++i) {
    nodes[i].reset(new DHTNode());
    nodes[i]->setIPAddress("2001::100" + util::uitos(i + 1));
    nodes[i]->setPort(6881 + i);
  }
  nodes[1]->setIPAddress("non-numerical-name");

  DHTRoutingTableSerializer s(AF_INET6);
  s.setLocalNode(localNode);
  s.setNodes(nodes);

  std::string filename =
      A2_TEST_OUT_DIR "/aria2_DHTRoutingTableDeserializerTest_testDeserialize6";
  s.serialize(filename);

  DHTRoutingTableDeserializer d(AF_INET6);
  d.deserialize(filename);

  REQUIRE(memcmp(localNode->getID(), d.getLocalNode()->getID(),
                        DHT_ID_LENGTH) == 0);

  REQUIRE_EQ((size_t)2, d.getNodes().size());
  const std::vector<std::shared_ptr<DHTNode>>& dsnodes = d.getNodes();
  REQUIRE_EQ(std::string("2001::1001"), dsnodes[0]->getIPAddress());
  REQUIRE_EQ((uint16_t)6881, dsnodes[0]->getPort());
  REQUIRE(
      memcmp(nodes[0]->getID(), dsnodes[0]->getID(), DHT_ID_LENGTH) == 0);
  REQUIRE_EQ(std::string("2001::1003"), dsnodes[1]->getIPAddress());
  REQUIRE_EQ((uint16_t)6883, dsnodes[1]->getPort());
  REQUIRE(
      memcmp(nodes[2]->getID(), dsnodes[1]->getID(), DHT_ID_LENGTH) == 0);
}

} // namespace aria2
