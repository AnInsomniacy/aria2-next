#include "DHTNode.h"
#include "Exception.h"
#include "util.h"
#include <iostream>
#include "a2doctest.h"

namespace aria2 {

class DHTNodeTest {


public:
  void setUp() {}

  void tearDown() {}

  void testGenerateID();
};

A2_TEST(DHTNodeTest, testGenerateID)

void DHTNodeTest::testGenerateID()
{
  DHTNode node;
  std::cerr << util::toHex(node.getID(), DHT_ID_LENGTH) << std::endl;
}

} // namespace aria2
