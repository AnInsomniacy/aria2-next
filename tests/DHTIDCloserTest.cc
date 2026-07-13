#include "DHTIDCloser.h"

#include <cstring>
#include <algorithm>

#include "a2doctest.h"

#include "DHTNode.h"
#include "DHTNodeLookupEntry.h"
#include "Exception.h"
#include "util.h"

namespace aria2 {

class DHTIDCloserTest {


public:
  void setUp() {}

  void tearDown() {}

  void testOperator();
};

A2_TEST(DHTIDCloserTest, testOperator)

void DHTIDCloserTest::testOperator()
{
  unsigned char id[DHT_ID_LENGTH];
  memset(id, 0xf0, DHT_ID_LENGTH);

  auto e1 = make_unique<DHTNodeLookupEntry>(std::make_shared<DHTNode>(id));
  auto ep1 = e1.get();

  id[0] = 0xb0;
  auto e2 = make_unique<DHTNodeLookupEntry>(std::make_shared<DHTNode>(id));
  auto ep2 = e2.get();

  id[0] = 0xa0;
  auto e3 = make_unique<DHTNodeLookupEntry>(std::make_shared<DHTNode>(id));
  auto ep3 = e3.get();

  id[0] = 0x80;
  auto e4 = make_unique<DHTNodeLookupEntry>(std::make_shared<DHTNode>(id));
  auto ep4 = e4.get();

  id[0] = 0x00;
  auto e5 = make_unique<DHTNodeLookupEntry>(std::make_shared<DHTNode>(id));
  auto ep5 = e5.get();

  auto entries = std::vector<std::unique_ptr<DHTNodeLookupEntry>>{};
  entries.push_back(std::move(e1));
  entries.push_back(std::move(e2));
  entries.push_back(std::move(e3));
  entries.push_back(std::move(e4));
  entries.push_back(std::move(e5));

  std::sort(std::begin(entries), std::end(entries),
            DHTIDCloser(ep3->node->getID()));

  REQUIRE(*ep3 == *entries[0]);
  REQUIRE(*ep2 == *entries[1]);
  REQUIRE(*ep4 == *entries[2]);
  REQUIRE(*ep1 == *entries[3]);
  REQUIRE(*ep5 == *entries[4]);
}

} // namespace aria2
