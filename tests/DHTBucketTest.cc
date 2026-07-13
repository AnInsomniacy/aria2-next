#include "DHTBucket.h"

#include <cstring>
#include <algorithm>

#include "a2doctest.h"

#include "DHTNode.h"
#include "Exception.h"
#include "util.h"

namespace aria2 {

class DHTBucketTest {


public:
  void setUp() {}

  void tearDown() {}

  void testGetRandomNodeID();
  void testIsInRange();
  void testSplitAllowed();
  void testSplit();
  void testAddNode();
  void testMoveToHead();
  void testMoveToTail();
  void testGetGoodNodes();
  void testCacheNode();
  void testDropNode();
  void testGetNode();
};

A2_TEST(DHTBucketTest, testGetRandomNodeID)
A2_TEST(DHTBucketTest, testIsInRange)
A2_TEST(DHTBucketTest, testSplitAllowed)
A2_TEST(DHTBucketTest, testSplit)
A2_TEST(DHTBucketTest, testAddNode)
A2_TEST(DHTBucketTest, testMoveToHead)
A2_TEST(DHTBucketTest, testMoveToTail)
A2_TEST(DHTBucketTest, testGetGoodNodes)
A2_TEST(DHTBucketTest, testCacheNode)
A2_TEST(DHTBucketTest, testDropNode)
A2_TEST(DHTBucketTest, testGetNode)

void DHTBucketTest::testGetRandomNodeID()
{
  unsigned char localNodeID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  {
    DHTBucket bucket(localNode);
    unsigned char nodeID[DHT_ID_LENGTH];
    bucket.getRandomNodeID(nodeID);
  }
  {
    unsigned char max[] = {0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    unsigned char min[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    DHTBucket bucket(16, max, min, localNode);
    unsigned char nodeID[DHT_ID_LENGTH];
    bucket.getRandomNodeID(nodeID);
    REQUIRE_EQ(std::string("0101"),
                         util::toHex(nodeID, sizeof(nodeID)).substr(0, 4));
  }
}

void DHTBucketTest::testIsInRange()
{
  unsigned char localNodeID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  {
    unsigned char nodeID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::shared_ptr<DHTNode> node(new DHTNode(nodeID));
    DHTBucket bucket(localNode);
    REQUIRE(bucket.isInRange(node));
    memset(nodeID, 0xff, sizeof(nodeID));
    REQUIRE(bucket.isInRange(node));
  }
  {
    unsigned char max[] = {0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    unsigned char min[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    {
      // min
      unsigned char nodeID[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      std::shared_ptr<DHTNode> node(new DHTNode(nodeID));
      DHTBucket bucket(16, max, min, localNode);
      REQUIRE(bucket.isInRange(node));
    }
    {
      // max
      unsigned char nodeID[] = {0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
      std::shared_ptr<DHTNode> node(new DHTNode(nodeID));
      DHTBucket bucket(16, max, min, localNode);
      REQUIRE(bucket.isInRange(node));
    }
    {
      unsigned char nodeID[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
      std::shared_ptr<DHTNode> node(new DHTNode(nodeID));
      DHTBucket bucket(16, max, min, localNode);
      REQUIRE(bucket.isInRange(node));
    }
    {
      // nodeID is out of range: smaller than this range
      unsigned char nodeID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      std::shared_ptr<DHTNode> node(new DHTNode(nodeID));
      DHTBucket bucket(16, max, min, localNode);
      REQUIRE(!bucket.isInRange(node));
    }
    {
      // nodeID is out of range: larger than this range
      unsigned char nodeID[] = {0x01, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
      std::shared_ptr<DHTNode> node(new DHTNode(nodeID));
      DHTBucket bucket(16, max, min, localNode);
      REQUIRE(!bucket.isInRange(node));
    }
  }
}

void DHTBucketTest::testSplitAllowed()
{
  {
    unsigned char localNodeID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
    DHTBucket bucket(localNode);
    REQUIRE(bucket.splitAllowed());
  }
  {
    unsigned char max[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    unsigned char min[] = {0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    {
      unsigned char localNodeID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
      DHTBucket bucket(3, max, min, localNode);
      REQUIRE(!bucket.splitAllowed());
    }
    {
      unsigned char localNodeID[] = {0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
      std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
      DHTBucket bucket(3, max, min, localNode);
      REQUIRE(bucket.splitAllowed());
    }
  }
}

void DHTBucketTest::testSplit()
{
  {
    unsigned char localNodeID[DHT_ID_LENGTH];
    memset(localNodeID, 0, DHT_ID_LENGTH);
    std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
    {
      DHTBucket bucket(localNode);
      std::shared_ptr<DHTBucket> r = bucket.split();
      {
        unsigned char expectedRMax[] = {
            0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        unsigned char expectedRMin[DHT_ID_LENGTH];
        memset(expectedRMin, 0, DHT_ID_LENGTH);
        REQUIRE_EQ(util::toHex(expectedRMax, DHT_ID_LENGTH),
                             util::toHex(r->getMaxID(), DHT_ID_LENGTH));
        REQUIRE_EQ(util::toHex(expectedRMin, DHT_ID_LENGTH),
                             util::toHex(r->getMinID(), DHT_ID_LENGTH));
        REQUIRE_EQ((size_t)1, r->getPrefixLength());
      }
      {
        unsigned char expectedLMax[DHT_ID_LENGTH];
        memset(expectedLMax, 0xff, DHT_ID_LENGTH);
        unsigned char expectedLMin[] = {
            0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        REQUIRE_EQ(util::toHex(expectedLMax, DHT_ID_LENGTH),
                             util::toHex(bucket.getMaxID(), DHT_ID_LENGTH));
        REQUIRE_EQ(util::toHex(expectedLMin, DHT_ID_LENGTH),
                             util::toHex(bucket.getMinID(), DHT_ID_LENGTH));
        REQUIRE_EQ((size_t)1, bucket.getPrefixLength());
      }
    }
    {
      std::shared_ptr<DHTBucket> bucket(new DHTBucket(localNode));
      for (int i = 0; i < 159; ++i) {
        REQUIRE(bucket->splitAllowed());
        std::shared_ptr<DHTBucket> t = bucket;
        bucket = bucket->split();
        REQUIRE(!t->splitAllowed());
      }
      REQUIRE(!bucket->splitAllowed());
      unsigned char expectedMax[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
      unsigned char expectedMin[DHT_ID_LENGTH];
      memset(expectedMin, 0, DHT_ID_LENGTH);
      REQUIRE_EQ(util::toHex(expectedMax, DHT_ID_LENGTH),
                           util::toHex(bucket->getMaxID(), DHT_ID_LENGTH));
      REQUIRE_EQ(util::toHex(expectedMin, DHT_ID_LENGTH),
                           util::toHex(bucket->getMinID(), DHT_ID_LENGTH));
      REQUIRE_EQ((size_t)159, bucket->getPrefixLength());
    }
  }
  {
    unsigned char localNodeID[DHT_ID_LENGTH];
    memset(localNodeID, 0, DHT_ID_LENGTH);
    localNodeID[0] = 0x80;
    std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
    DHTBucket bucket(localNode);
    std::shared_ptr<DHTBucket> r = bucket.split();
    {
      unsigned char expectedRMax[] = {0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                      0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
      unsigned char expectedRMin[DHT_ID_LENGTH];
      memset(expectedRMin, 0, DHT_ID_LENGTH);
      REQUIRE_EQ(util::toHex(expectedRMax, DHT_ID_LENGTH),
                           util::toHex(r->getMaxID(), DHT_ID_LENGTH));
      REQUIRE_EQ(util::toHex(expectedRMin, DHT_ID_LENGTH),
                           util::toHex(r->getMinID(), DHT_ID_LENGTH));
      REQUIRE_EQ((size_t)1, r->getPrefixLength());
    }
    {
      unsigned char expectedLMax[DHT_ID_LENGTH];
      memset(expectedLMax, 0xff, DHT_ID_LENGTH);
      unsigned char expectedLMin[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      REQUIRE_EQ(util::toHex(expectedLMax, DHT_ID_LENGTH),
                           util::toHex(bucket.getMaxID(), DHT_ID_LENGTH));
      REQUIRE_EQ(util::toHex(expectedLMin, DHT_ID_LENGTH),
                           util::toHex(bucket.getMinID(), DHT_ID_LENGTH));
      REQUIRE_EQ((size_t)1, bucket.getPrefixLength());
    }
  }
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

void DHTBucketTest::testAddNode()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0, DHT_ID_LENGTH);
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  DHTBucket bucket(localNode);

  unsigned char id[DHT_ID_LENGTH];
  std::shared_ptr<DHTNode> nodes[8];
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0xf0, i);
    nodes[i].reset(new DHTNode(id));
    REQUIRE(bucket.addNode(nodes[i]));
  }
  createID(id, 0xf0, 0xff);
  std::shared_ptr<DHTNode> newNode(new DHTNode(id));
  REQUIRE(!bucket.addNode(newNode));

  // nodes[0] is located at the tail of the bucket(least recent seen)
  nodes[0]->markBad();
  REQUIRE(bucket.addNode(newNode));
  REQUIRE(*bucket.getNodes().back() == *newNode);
}

void DHTBucketTest::testMoveToHead()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0, DHT_ID_LENGTH);
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  DHTBucket bucket(localNode);

  unsigned char id[DHT_ID_LENGTH];
  std::shared_ptr<DHTNode> nodes[8];
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0xf0, i);
    nodes[i].reset(new DHTNode(id));
    REQUIRE(bucket.addNode(nodes[i]));
  }
  bucket.moveToHead(nodes[DHTBucket::K - 1]);
  REQUIRE(*bucket.getNodes().front() == *nodes[DHTBucket::K - 1]);
}

void DHTBucketTest::testMoveToTail()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0, DHT_ID_LENGTH);
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  DHTBucket bucket(localNode);

  unsigned char id[DHT_ID_LENGTH];
  std::shared_ptr<DHTNode> nodes[8];
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0xf0, i);
    nodes[i].reset(new DHTNode(id));
    REQUIRE(bucket.addNode(nodes[i]));
  }
  bucket.moveToTail(nodes[0]);
  REQUIRE(*bucket.getNodes().back() == *nodes[0]);
}

void DHTBucketTest::testGetGoodNodes()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0, DHT_ID_LENGTH);
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  DHTBucket bucket(localNode);

  unsigned char id[DHT_ID_LENGTH];
  std::shared_ptr<DHTNode> nodes[8];
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0xf0, i);
    nodes[i].reset(new DHTNode(id));
    nodes[i]->setPort(6881 + i);
    REQUIRE(bucket.addNode(nodes[i]));
  }
  nodes[3]->markBad();
  nodes[5]->markBad();
  std::vector<std::shared_ptr<DHTNode>> goodNodes;
  bucket.getGoodNodes(goodNodes);
  REQUIRE_EQ((size_t)6, goodNodes.size());
  REQUIRE_EQ((uint16_t)6881, goodNodes[0]->getPort());
  REQUIRE_EQ((uint16_t)6882, goodNodes[1]->getPort());
  REQUIRE_EQ((uint16_t)6883, goodNodes[2]->getPort());
  REQUIRE_EQ((uint16_t)6885, goodNodes[3]->getPort());
  REQUIRE_EQ((uint16_t)6887, goodNodes[4]->getPort());
  REQUIRE_EQ((uint16_t)6888, goodNodes[5]->getPort());
}

void DHTBucketTest::testCacheNode()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0, DHT_ID_LENGTH);
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  DHTBucket bucket(localNode);

  std::shared_ptr<DHTNode> n1(new DHTNode());
  std::shared_ptr<DHTNode> n2(new DHTNode());
  std::shared_ptr<DHTNode> n3(new DHTNode());

  bucket.cacheNode(n1);
  bucket.cacheNode(n2);
  REQUIRE_EQ((size_t)2, bucket.getCachedNodes().size());
  REQUIRE(*n2 == *bucket.getCachedNodes()[0]);

  bucket.cacheNode(n3);
  REQUIRE_EQ((size_t)2, bucket.getCachedNodes().size());
  REQUIRE(*n3 == *bucket.getCachedNodes()[0]);
  REQUIRE(*n2 == *bucket.getCachedNodes()[1]);
}

void DHTBucketTest::testDropNode()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0, DHT_ID_LENGTH);
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  DHTBucket bucket(localNode);

  unsigned char id[DHT_ID_LENGTH];
  std::shared_ptr<DHTNode> nodes[8];
  for (size_t i = 0; i < DHTBucket::K; ++i) {
    createID(id, 0xf0, i);
    nodes[i].reset(new DHTNode(id));
    nodes[i]->setPort(6881 + i);
    REQUIRE(bucket.addNode(nodes[i]));
  }

  std::shared_ptr<DHTNode> cachedNode1(new DHTNode());
  std::shared_ptr<DHTNode> cachedNode2(new DHTNode());

  bucket.dropNode(nodes[3]);
  // nothing happens because the replacement cache is empty.
  {
    std::deque<std::shared_ptr<DHTNode>> tnodes = bucket.getNodes();
    REQUIRE_EQ((size_t)8, tnodes.size());
    REQUIRE(*nodes[3] == *tnodes[3]);
  }

  bucket.cacheNode(cachedNode1);
  bucket.cacheNode(cachedNode2);

  bucket.dropNode(nodes[3]);
  {
    std::deque<std::shared_ptr<DHTNode>> tnodes = bucket.getNodes();
    REQUIRE_EQ((size_t)8, tnodes.size());
    REQUIRE(tnodes.end() == std::find_if(tnodes.begin(), tnodes.end(),
                                                derefEqual(nodes[3])));
    REQUIRE(*cachedNode2 == *tnodes[7]);
  }
  REQUIRE_EQ((size_t)1, bucket.getCachedNodes().size());
  REQUIRE(*cachedNode1 == *bucket.getCachedNodes()[0]);
}

void DHTBucketTest::testGetNode()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0, DHT_ID_LENGTH);
  std::shared_ptr<DHTNode> localNode(new DHTNode(localNodeID));
  DHTBucket bucket(localNode);

  unsigned char id[DHT_ID_LENGTH];
  createID(id, 0xf0, 0);
  std::shared_ptr<DHTNode> node(new DHTNode(id));
  node->setIPAddress("192.168.0.1");
  node->setPort(6881);
  REQUIRE(bucket.addNode(node));
  REQUIRE(bucket.getNode(id, "192.168.0.1", 6881));
  REQUIRE(!bucket.getNode(id, "192.168.0.2", 6881));
  REQUIRE(!bucket.getNode(id, "192.168.0.1", 6882));
  REQUIRE(!bucket.getNode(localNodeID, "192.168.0.1", 6881));
}

} // namespace aria2
