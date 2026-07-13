#include "DHTRoutingTableSerializer.h"

#include <cstring>
#include <fstream>
#include <iostream>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "DHTNode.h"
#include "array_fun.h"
#include "DHTConstants.h"
#include "bittorrent_helper.h"
#include "a2netcompat.h"

namespace aria2 {

class DHTRoutingTableSerializerTest {


private:
  char zero[256];
  char buf[20];

  void checkToLocalnode(std::istream& ss,
                        const std::shared_ptr<DHTNode>& localNode);

  void checkNumNodes(std::istream& ss, size_t expected);

public:
  void setUp() { memset(zero, 0, sizeof(zero)); }

  void tearDown() {}

  void testSerialize();

  void testSerialize6();
};

A2_TEST(DHTRoutingTableSerializerTest, testSerialize)
A2_TEST(DHTRoutingTableSerializerTest, testSerialize6)

void DHTRoutingTableSerializerTest::checkToLocalnode(
    std::istream& ss, const std::shared_ptr<DHTNode>& localNode)
{
  // header
  ss.read(buf, 8);
  // magic
  REQUIRE((char)0xa1 == buf[0]);
  REQUIRE((char)0xa2 == buf[1]);
  // format ID
  REQUIRE((char)0x02 == buf[2]);
  // reserved
  REQUIRE((char)0x00 == buf[3]);
  REQUIRE((char)0x00 == buf[4]);
  REQUIRE((char)0x00 == buf[5]);
  // version
  REQUIRE((char)0x00 == buf[6]);
  REQUIRE((char)0x03 == buf[7]);

  // time
  ss.read(buf, 8);
  time_t time;
  uint64_t timebuf;
  memcpy(&timebuf, buf, sizeof(timebuf));
  time = ntoh64(timebuf);
  std::cerr << time << std::endl;

  // localnode
  // 8bytes reserved
  ss.read(buf, 8);
  REQUIRE(memcmp(zero, buf, 8) == 0);
  // localnode ID
  ss.read(buf, DHT_ID_LENGTH);
  REQUIRE(memcmp(localNode->getID(), buf, DHT_ID_LENGTH) == 0);
  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);
}

void DHTRoutingTableSerializerTest::checkNumNodes(std::istream& ss,
                                                  size_t expected)
{
  // number of nodes saved
  ss.read(buf, 4);
  uint32_t numNodes;
  memcpy(&numNodes, buf, sizeof(numNodes));
  numNodes = ntohl(numNodes);

  REQUIRE_EQ((uint32_t)expected, numNodes);
}

void DHTRoutingTableSerializerTest::testSerialize()
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
      A2_TEST_OUT_DIR "/aria2_DHTRoutingTableSerializerTest_testSerialize";
  s.serialize(filename);

  std::ifstream ss(filename.c_str(), std::ios::binary);

  checkToLocalnode(ss, localNode);
  size_t numNodes = 3;
  checkNumNodes(ss, numNodes);

  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);

  // node[0]
  // 1byte compatc peer format length
  {
    uint8_t len;
    ss >> len;
    REQUIRE_EQ((uint8_t)6, len);
  }
  // 7bytes reserved
  ss.read(buf, 7);
  REQUIRE(memcmp(zero, buf, 7) == 0);
  // 6bytes compact peer info
  ss.read(buf, 6);
  {
    std::pair<std::string, uint16_t> peer = bittorrent::unpackcompact(
        reinterpret_cast<const unsigned char*>(buf), AF_INET);
    REQUIRE_EQ(std::string("192.168.0.1"), peer.first);
    REQUIRE_EQ((uint16_t)6881, peer.second);
  }
  // 2bytes reserved
  ss.read(buf, 2);
  REQUIRE(memcmp(zero, buf, 2) == 0);
  // 16bytes reserved
  ss.read(buf, 16);
  REQUIRE(memcmp(zero, buf, 16) == 0);
  // localnode ID
  ss.read(buf, DHT_ID_LENGTH);
  REQUIRE(memcmp(nodes[0]->getID(), buf, DHT_ID_LENGTH) == 0);
  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);

  // node[1]
  // 1byte compatc peer format length
  {
    uint8_t len;
    ss >> len;
    REQUIRE_EQ((uint8_t)6, len);
  }
  // 7bytes reserved
  ss.read(buf, 7);
  REQUIRE(memcmp(zero, buf, 7) == 0);
  // 6bytes compact peer info
  ss.read(buf, 6);
  // zero filled because node[1]'s hostname is not numerical form
  // deserializer should skip this entry
  REQUIRE(memcmp(zero, buf, 6) == 0);
  // 2bytes reserved
  ss.read(buf, 2);
  REQUIRE(memcmp(zero, buf, 2) == 0);
  // 16bytes reserved
  ss.read(buf, 16);
  REQUIRE(memcmp(zero, buf, 16) == 0);
  // localnode ID
  ss.read(buf, DHT_ID_LENGTH);
  REQUIRE(memcmp(nodes[1]->getID(), buf, DHT_ID_LENGTH) == 0);
  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);

  // node[2]
  // 1byte compatc peer format length
  {
    uint8_t len;
    ss >> len;
    REQUIRE_EQ((uint8_t)6, len);
  }
  // 7bytes reserved
  ss.read(buf, 7);
  REQUIRE(memcmp(zero, buf, 7) == 0);
  // 6bytes compact peer info
  ss.read(buf, 6);
  {
    std::pair<std::string, uint16_t> peer = bittorrent::unpackcompact(
        reinterpret_cast<const unsigned char*>(buf), AF_INET);
    REQUIRE_EQ(std::string("192.168.0.3"), peer.first);
    REQUIRE_EQ((uint16_t)6883, peer.second);
  }
  // 2bytes reserved
  ss.read(buf, 2);
  REQUIRE(memcmp(zero, buf, 2) == 0);
  // 16bytes reserved
  ss.read(buf, 16);
  REQUIRE(memcmp(zero, buf, 16) == 0);
  // localnode ID
  ss.read(buf, DHT_ID_LENGTH);
  REQUIRE(memcmp(nodes[2]->getID(), buf, DHT_ID_LENGTH) == 0);
  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);

  // check to see stream ends
  ss.read(buf, 1);
  REQUIRE_EQ((std::streamsize)0, ss.gcount());
  REQUIRE(ss.eof());
}

void DHTRoutingTableSerializerTest::testSerialize6()
{
  std::shared_ptr<DHTNode> localNode(new DHTNode());

  std::vector<std::shared_ptr<DHTNode>> nodes(2);
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
      A2_TEST_OUT_DIR "/aria2_DHTRoutingTableSerializerTest_testSerialize6";
  s.serialize(filename);

  std::ifstream ss(filename.c_str(), std::ios::binary);

  checkToLocalnode(ss, localNode);
  size_t numNodes = 2;
  checkNumNodes(ss, numNodes);

  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);

  // node[0]
  // 1byte compatc peer format length
  {
    uint8_t len;
    ss >> len;
    REQUIRE_EQ((uint8_t)18, len);
  }
  // 7bytes reserved
  ss.read(buf, 7);
  REQUIRE(memcmp(zero, buf, 7) == 0);
  // 18 bytes compact peer info
  ss.read(buf, 18);
  {
    std::pair<std::string, uint16_t> peer = bittorrent::unpackcompact(
        reinterpret_cast<const unsigned char*>(buf), AF_INET6);
    REQUIRE_EQ(std::string("2001::1001"), peer.first);
    REQUIRE_EQ((uint16_t)6881, peer.second);
  }
  // 6bytes reserved
  ss.read(buf, 6);
  REQUIRE(memcmp(zero, buf, 6) == 0);
  // localnode ID
  ss.read(buf, DHT_ID_LENGTH);
  REQUIRE(memcmp(nodes[0]->getID(), buf, DHT_ID_LENGTH) == 0);
  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);

  // node[1]
  // 1byte compatc peer format length
  {
    uint8_t len;
    ss >> len;
    REQUIRE_EQ((uint8_t)18, len);
  }
  // 7bytes reserved
  ss.read(buf, 7);
  REQUIRE(memcmp(zero, buf, 7) == 0);
  // 18bytes compact peer info
  ss.read(buf, 18);
  // zero filled because node[1]'s hostname is not numerical form
  // deserializer should skip this entry
  REQUIRE(memcmp(zero, buf, 18) == 0);
  // 6bytes reserved
  ss.read(buf, 6);
  REQUIRE(memcmp(zero, buf, 6) == 0);
  // localnode ID
  ss.read(buf, DHT_ID_LENGTH);
  REQUIRE(memcmp(nodes[1]->getID(), buf, DHT_ID_LENGTH) == 0);
  // 4bytes reserved
  ss.read(buf, 4);
  REQUIRE(memcmp(zero, buf, 4) == 0);

  // check to see stream ends
  ss.read(buf, 1);
  REQUIRE_EQ((std::streamsize)0, ss.gcount());
  REQUIRE(ss.eof());
}

} // namespace aria2
