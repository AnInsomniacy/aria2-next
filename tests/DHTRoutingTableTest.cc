#include "DHTRoutingTable.h"

#include <cstring>
#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "DHTNode.h"
#include "DHTBucket.h"
#include "MockDHTTaskQueue.h"
#include "MockDHTTaskFactory.h"
#include "DHTTask.h"

namespace aria2 {

class DHTRoutingTableTest {


public:
  void setUp() {}

  void tearDown() {}

  void testAddNode();
  void testAddNode_localNode();
  void testGetClosestKNodes();
};

A2_TEST(DHTRoutingTableTest, testAddNode)
A2_TEST(DHTRoutingTableTest, testAddNode_localNode)
A2_TEST(DHTRoutingTableTest, testGetClosestKNodes)

void DHTRoutingTableTest::testAddNode()
{
  auto localNode = std::make_shared<DHTNode>();
  DHTRoutingTable table(localNode);
  auto taskFactory = make_unique<MockDHTTaskFactory>();
  table.setTaskFactory(taskFactory.get());
  auto taskQueue = make_unique<MockDHTTaskQueue>();
  table.setTaskQueue(taskQueue.get());
  uint32_t count = 0;
  for (int i = 0; i < 100; ++i) {
    if (table.addNode(std::make_shared<DHTNode>())) {
      ++count;
    }
  }
  table.showBuckets();
}

void DHTRoutingTableTest::testAddNode_localNode()
{
  auto localNode = std::make_shared<DHTNode>();
  DHTRoutingTable table(localNode);
  auto taskFactory = make_unique<MockDHTTaskFactory>();
  table.setTaskFactory(taskFactory.get());
  auto taskQueue = make_unique<MockDHTTaskQueue>();
  table.setTaskQueue(taskQueue.get());

  auto newNode = std::make_shared<DHTNode>(localNode->getID());
  REQUIRE(!table.addNode(newNode));
}

namespace {
void createID(unsigned char* id, unsigned char firstChar,
              unsigned char lastChar)
{
  memset(id, 0, DHT_ID_LENGTH);
  id[0] = firstChar;
  id[DHT_ID_LENGTH - 1] = lastChar;
}
} // namespace

void DHTRoutingTableTest::testGetClosestKNodes()
{
  unsigned char id[DHT_ID_LENGTH];
  createID(id, 0x81, 0);
  auto localNode = std::make_shared<DHTNode>(id);

  DHTRoutingTable table(localNode);

  std::shared_ptr<DHTNode> nodes1[8];
  std::shared_ptr<DHTNode> nodes2[8];
  std::shared_ptr<DHTNode> nodes3[8];
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0xf0, i);
    nodes1[i] = std::make_shared<DHTNode>(id);
    REQUIRE(table.addNode(nodes1[i]));
  }
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0x80, i);
    nodes2[i] = std::make_shared<DHTNode>(id);
    REQUIRE(table.addNode(nodes2[i]));
  }
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0x70, i);
    nodes3[i] = std::make_shared<DHTNode>(id);
    REQUIRE(table.addNode(nodes3[i]));
  }
  {
    createID(id, 0x80, 0x10);
    std::vector<std::shared_ptr<DHTNode>> nodes;
    table.getClosestKNodes(nodes, id);
    REQUIRE_EQ((size_t)8, nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
      REQUIRE(
          memcmp(nodes2[0]->getID(), nodes[0]->getID(), DHT_ID_LENGTH) == 0);
    }
  }
}

} // namespace aria2
