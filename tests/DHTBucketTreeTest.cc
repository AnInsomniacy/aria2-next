#include "DHTBucketTree.h"

#include <cstring>
#include "a2doctest.h"

#include "DHTNode.h"
#include "DHTBucket.h"
#include "Exception.h"
#include "util.h"

namespace aria2 {

class DHTBucketTreeTest {


public:
  void testDig();
  void testFindBucketFor();
  void testFindClosestKNodes();
  void testEnumerateBucket();
};

A2_TEST(DHTBucketTreeTest, testDig)
A2_TEST(DHTBucketTreeTest, testFindBucketFor)
A2_TEST(DHTBucketTreeTest, testFindClosestKNodes)
A2_TEST(DHTBucketTreeTest, testEnumerateBucket)

void DHTBucketTreeTest::testDig()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0xff, DHT_ID_LENGTH);

  auto localNode = std::make_shared<DHTNode>(localNodeID);

  auto bucket1 = std::make_shared<DHTBucket>(localNode);
  auto bucket2 = std::shared_ptr<DHTBucket>(bucket1->split());
  auto bucket3 = std::shared_ptr<DHTBucket>(bucket1->split());
  // Tree: number is prefix
  //
  //           +
  //    +------+------+
  //   b2             |
  //   0       +------+------+
  //          b3             b1
  //          10             11
  //                         |
  //                     localNode is here
  {
    DHTBucketTreeNode b(bucket1);
    REQUIRE(!b.dig(localNode->getID()));
  }
  {
    DHTBucketTreeNode b(make_unique<DHTBucketTreeNode>(bucket3),
                        make_unique<DHTBucketTreeNode>(bucket1));
    REQUIRE(b.dig(localNode->getID()) == b.getRight());
  }
}

void DHTBucketTreeTest::testFindBucketFor()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0xaa, DHT_ID_LENGTH);

  auto localNode = std::make_shared<DHTNode>(localNodeID);

  auto bucket1 = std::make_shared<DHTBucket>(localNode);
  auto bucket2 = std::shared_ptr<DHTBucket>(bucket1->split());
  auto bucket3 = std::shared_ptr<DHTBucket>(bucket1->split());
  auto bucket4 = std::shared_ptr<DHTBucket>(bucket3->split());
  auto bucket5 = std::shared_ptr<DHTBucket>(bucket3->split());

  {
    DHTBucketTreeNode b(bucket5);
    REQUIRE(*bucket5 == *dht::findBucketFor(&b, localNodeID));
  }
  {
    // Tree: number is prefix
    //
    //           +
    //    +------+------+
    //   b2             |
    //   0       +------+------+
    //           |             b1
    //     +-----+-----+      11
    //    b4           |
    //   100     +-----+-----+
    //          b5           b3
    //          1010         1011
    //           |
    //    localNode is here
    auto b1 = make_unique<DHTBucketTreeNode>(bucket1);
    auto b2 = make_unique<DHTBucketTreeNode>(bucket2);
    auto b3 = make_unique<DHTBucketTreeNode>(bucket3);
    auto b4 = make_unique<DHTBucketTreeNode>(bucket4);
    auto b5 = make_unique<DHTBucketTreeNode>(bucket5);

    auto bp1 = make_unique<DHTBucketTreeNode>(std::move(b5), std::move(b3));
    auto bp2 = make_unique<DHTBucketTreeNode>(std::move(b4), std::move(bp1));
    auto bp3 = make_unique<DHTBucketTreeNode>(std::move(bp2), std::move(b1));
    DHTBucketTreeNode bp4(std::move(b2), std::move(bp3));

    REQUIRE(*bucket5 == *dht::findBucketFor(&bp4, localNode->getID()));
  }
}

void DHTBucketTreeTest::testFindClosestKNodes()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0xaa, DHT_ID_LENGTH);

  auto localNode = std::make_shared<DHTNode>(localNodeID);

  auto bucket1 = std::make_shared<DHTBucket>(localNode);
  auto bucket2 = std::shared_ptr<DHTBucket>(bucket1->split());
  auto bucket3 = std::shared_ptr<DHTBucket>(bucket1->split());
  auto bucket4 = std::shared_ptr<DHTBucket>(bucket3->split());
  auto bucket5 = std::shared_ptr<DHTBucket>(bucket3->split());

  unsigned char id[DHT_ID_LENGTH];
  {
    auto b1 = make_unique<DHTBucketTreeNode>(bucket1);
    auto b2 = make_unique<DHTBucketTreeNode>(bucket2);
    auto b3 = make_unique<DHTBucketTreeNode>(bucket3);
    auto b4 = make_unique<DHTBucketTreeNode>(bucket4);
    auto b5 = make_unique<DHTBucketTreeNode>(bucket5);

    auto bp1 = make_unique<DHTBucketTreeNode>(std::move(b5), std::move(b3));
    auto bp2 = make_unique<DHTBucketTreeNode>(std::move(b4), std::move(bp1));
    auto bp3 = make_unique<DHTBucketTreeNode>(std::move(bp2), std::move(b1));
    DHTBucketTreeNode bp4(std::move(b2), std::move(bp3));

    for (size_t i = 0; i < 2; ++i) {
      bucket1->getRandomNodeID(id);
      bucket1->addNode(std::make_shared<DHTNode>(id));
      bucket2->getRandomNodeID(id);
      bucket2->addNode(std::make_shared<DHTNode>(id));
      bucket3->getRandomNodeID(id);
      bucket3->addNode(std::make_shared<DHTNode>(id));
      bucket4->getRandomNodeID(id);
      bucket4->addNode(std::make_shared<DHTNode>(id));
      bucket5->getRandomNodeID(id);
      bucket5->addNode(std::make_shared<DHTNode>(id));
    }
    {
      unsigned char targetID[DHT_ID_LENGTH];
      memset(targetID, 0x80, DHT_ID_LENGTH);
      std::vector<std::shared_ptr<DHTNode>> nodes;
      dht::findClosestKNodes(nodes, &bp4, targetID);
      REQUIRE_EQ((size_t)8, nodes.size());
      REQUIRE(bucket4->isInRange(nodes[0]));
      REQUIRE(bucket4->isInRange(nodes[1]));
      REQUIRE(bucket5->isInRange(nodes[2]));
      REQUIRE(bucket5->isInRange(nodes[3]));
      REQUIRE(bucket3->isInRange(nodes[4]));
      REQUIRE(bucket3->isInRange(nodes[5]));
      REQUIRE(bucket1->isInRange(nodes[6]));
      REQUIRE(bucket1->isInRange(nodes[7]));
    }
    {
      unsigned char targetID[DHT_ID_LENGTH];
      memset(targetID, 0xf0, DHT_ID_LENGTH);
      std::vector<std::shared_ptr<DHTNode>> nodes;
      dht::findClosestKNodes(nodes, &bp4, targetID);
      REQUIRE_EQ((size_t)8, nodes.size());
      REQUIRE(bucket1->isInRange(nodes[0]));
      REQUIRE(bucket1->isInRange(nodes[1]));
      REQUIRE(bucket3->isInRange(nodes[2]));
      REQUIRE(bucket3->isInRange(nodes[3]));
      REQUIRE(bucket5->isInRange(nodes[4]));
      REQUIRE(bucket5->isInRange(nodes[5]));
      REQUIRE(bucket4->isInRange(nodes[6]));
      REQUIRE(bucket4->isInRange(nodes[7]));
    }
    {
      for (size_t i = 0; i < 6; ++i) {
        bucket4->getRandomNodeID(id);
        bucket4->addNode(std::make_shared<DHTNode>(id));
      }
      unsigned char targetID[DHT_ID_LENGTH];
      memset(targetID, 0x80, DHT_ID_LENGTH);
      std::vector<std::shared_ptr<DHTNode>> nodes;
      dht::findClosestKNodes(nodes, &bp4, targetID);
      REQUIRE_EQ((size_t)8, nodes.size());
      for (size_t i = 0; i < DHTBucket::K; ++i) {
        REQUIRE(bucket4->isInRange(nodes[i]));
      }
    }
  }
}

void DHTBucketTreeTest::testEnumerateBucket()
{
  unsigned char localNodeID[DHT_ID_LENGTH];
  memset(localNodeID, 0xaa, DHT_ID_LENGTH);

  auto localNode = std::make_shared<DHTNode>(localNodeID);

  auto bucket1 = std::make_shared<DHTBucket>(localNode);
  auto bucket2 = std::shared_ptr<DHTBucket>(bucket1->split());
  auto bucket3 = std::shared_ptr<DHTBucket>(bucket1->split());
  auto bucket4 = std::shared_ptr<DHTBucket>(bucket3->split());
  auto bucket5 = std::shared_ptr<DHTBucket>(bucket3->split());

  {
    DHTBucketTreeNode b(bucket1);
    std::vector<std::shared_ptr<DHTBucket>> buckets;
    dht::enumerateBucket(buckets, &b);
    REQUIRE_EQ((size_t)1, buckets.size());
    REQUIRE(*bucket1 == *buckets[0]);
  }
  {
    auto b1 = make_unique<DHTBucketTreeNode>(bucket1);
    auto b2 = make_unique<DHTBucketTreeNode>(bucket2);
    auto b3 = make_unique<DHTBucketTreeNode>(bucket3);
    auto b4 = make_unique<DHTBucketTreeNode>(bucket4);
    auto b5 = make_unique<DHTBucketTreeNode>(bucket5);

    auto bp1 = make_unique<DHTBucketTreeNode>(std::move(b5), std::move(b3));
    auto bp2 = make_unique<DHTBucketTreeNode>(std::move(b4), std::move(bp1));
    auto bp3 = make_unique<DHTBucketTreeNode>(std::move(bp2), std::move(b1));
    DHTBucketTreeNode bp4(std::move(b2), std::move(bp3));

    std::vector<std::shared_ptr<DHTBucket>> buckets;
    dht::enumerateBucket(buckets, &bp4);
    REQUIRE_EQ((size_t)5, buckets.size());
    REQUIRE(*bucket2 == *buckets[0]);
    REQUIRE(*bucket4 == *buckets[1]);
    REQUIRE(*bucket5 == *buckets[2]);
    REQUIRE(*bucket3 == *buckets[3]);
    REQUIRE(*bucket1 == *buckets[4]);
  }
}

} // namespace aria2
