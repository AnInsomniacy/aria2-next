#include "DHTMessageTrackerEntry.h"

#include <iostream>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "MockDHTMessage.h"
#include "DHTNode.h"
#include "DHTMessageCallback.h"

namespace aria2 {

class DHTMessageTrackerEntryTest {


public:
  void setUp() {}

  void tearDown() {}

  void testMatch();

  void testHandleTimeout();
};

A2_TEST(DHTMessageTrackerEntryTest, testMatch)
A2_TEST(DHTMessageTrackerEntryTest, testHandleTimeout)

void DHTMessageTrackerEntryTest::testMatch()
{
  auto localNode = std::make_shared<DHTNode>();
  try {
    auto node1 = std::make_shared<DHTNode>();
    auto msg1 = make_unique<MockDHTMessage>(localNode, node1);
    auto node2 = std::make_shared<DHTNode>();
    auto msg2 = make_unique<MockDHTMessage>(localNode, node2);

    DHTMessageTrackerEntry entry(msg1->getRemoteNode(),
                                 msg1->getTransactionID(),
                                 msg1->getMessageType(), 30_s);

    REQUIRE(entry.match(msg1->getTransactionID(),
                               msg1->getRemoteNode()->getIPAddress(),
                               msg1->getRemoteNode()->getPort()));
    REQUIRE(!entry.match(msg2->getTransactionID(),
                                msg2->getRemoteNode()->getIPAddress(),
                                msg2->getRemoteNode()->getPort()));
  }
  catch (Exception& e) {
    FAIL(e.stackTrace());
  }
}

void DHTMessageTrackerEntryTest::testHandleTimeout() {}

} // namespace aria2
