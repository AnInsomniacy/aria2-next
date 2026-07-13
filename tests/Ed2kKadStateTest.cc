#include "Ed2kKadState.h"

#include "a2doctest.h"

#include "Ed2kAttribute.h"
#include "ed2k_constants.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

class Ed2kKadStateTest {

public:
  void testRoutingPromotesReplacementOnFailure();
  void testRoutingFindClosestAndSnapshot();
  void testRoutingRouterContactKeepsKadMetadata();
  void testRoutingFindsKnownContactByEndpoint();
  void testRoutingFindClosestExcludesRequester();
  void testRoutingBootstrapAndRefresh();
  void testKadSourceSearchCadence();
  void testTraversalContinuesBeforeSearch();
  void testExpiredTransactionCarriesContactForFailure();
  void testTransactionCompletionMatchesTarget();
  void testTransactionCompletionAndExpiry();
};

A2_TEST(Ed2kKadStateTest, testRoutingPromotesReplacementOnFailure)
A2_TEST(Ed2kKadStateTest, testRoutingFindClosestAndSnapshot)
A2_TEST(Ed2kKadStateTest, testRoutingRouterContactKeepsKadMetadata)
A2_TEST(Ed2kKadStateTest, testRoutingFindsKnownContactByEndpoint)
A2_TEST(Ed2kKadStateTest, testRoutingFindClosestExcludesRequester)
A2_TEST(Ed2kKadStateTest, testRoutingBootstrapAndRefresh)
A2_TEST(Ed2kKadStateTest, testKadSourceSearchCadence)
A2_TEST(Ed2kKadStateTest, testTraversalContinuesBeforeSearch)
A2_TEST(Ed2kKadStateTest, testExpiredTransactionCarriesContactForFailure)
A2_TEST(Ed2kKadStateTest, testTransactionCompletionMatchesTarget)
A2_TEST(Ed2kKadStateTest, testTransactionCompletionAndExpiry)

namespace {

std::string hashFromHex(const std::string& hex)
{
  return util::fromHex(hex.begin(), hex.end());
}

KadContact contactFromHex(const std::string& id, const std::string& host,
                          uint16_t udpPort)
{
  KadContact contact;
  contact.id = hashFromHex(id);
  contact.host = host;
  contact.udpPort = udpPort;
  contact.tcpPort = udpPort - 10;
  contact.version = 8;
  return contact;
}

Endpoint endpoint(const std::string& host, uint16_t port)
{
  Endpoint ep;
  ep.host = host;
  ep.port = port;
  return ep;
}

} // namespace

void Ed2kKadStateTest::testRoutingPromotesReplacementOnFailure()
{
  auto self = hashFromHex("23a8ceff57a7a32d562d649ed7893796");
  KadRoutingTable table(self, 1);
  auto live = contactFromHex("31d6cfe0d16ae931b73c59d7e0c089c0",
                             "1.2.3.4", 4672);
  auto replacement = contactFromHex("31d6cfe0d14ce931b73c59d7e0c04bc0",
                                    "5.6.7.8", 4672);

  table.nodeSeen(live, 10);
  table.heardAbout(replacement, 11);
  table.nodeFailed(live);

  auto closest = table.findClosest(self, 10, true);
  REQUIRE_EQ((size_t)1, closest.size());
  REQUIRE_EQ(replacement.id, closest[0].id);
  REQUIRE_EQ((size_t)1, table.liveSize());
  REQUIRE_EQ((size_t)0, table.replacementSize());
}

void Ed2kKadStateTest::testRoutingFindClosestAndSnapshot()
{
  auto self = hashFromHex("00000000000000000000000000000000");
  KadRoutingTable table(self, 10);
  auto near = contactFromHex("00000000000000000000000000000001",
                             "1.2.3.4", 4672);
  auto far = contactFromHex("80000000000000000000000000000000",
                            "5.6.7.8", 4672);
  auto unconfirmed = contactFromHex("00000000000000000000000000000002",
                                    "9.9.9.9", 4672);
  table.nodeSeen(far, 10);
  table.nodeSeen(near, 11);
  table.heardAbout(unconfirmed, 12);
  table.addRouterNode(endpoint("203.0.113.1", 4672));
  table.addRouterNode(endpoint("203.0.113.1", 4672));

  auto confirmed = table.findClosest(self, 10, false);
  REQUIRE_EQ((size_t)2, confirmed.size());
  REQUIRE_EQ(near.id, confirmed[0].id);
  REQUIRE_EQ(far.id, confirmed[1].id);

  auto all = table.findClosest(self, 10, true);
  REQUIRE_EQ((size_t)3, all.size());
  REQUIRE_EQ((size_t)1, table.getRouterNodes().size());

  KadRoutingTable restored(self, 10);
  restored.restore(table.snapshot());
  REQUIRE_EQ((size_t)3, restored.liveSize());
  REQUIRE_EQ((size_t)1, restored.getRouterNodes().size());
}

void Ed2kKadStateTest::testRoutingRouterContactKeepsKadMetadata()
{
  auto self = hashFromHex("00000000000000000000000000000000");
  KadRoutingTable table(self, 10);
  auto contact = contactFromHex("00000000000000000000000000000009",
                                "203.0.113.9", 4672);
  contact.udpKey = 0x55667788;

  table.addRouterNode(contact);
  table.addRouterNode(contact);

  auto routers = table.getRouterContacts();
  REQUIRE_EQ((size_t)1, routers.size());
  REQUIRE_EQ(contact.id, routers[0].id);
  REQUIRE_EQ(std::string("203.0.113.9"), routers[0].host);
  REQUIRE_EQ((uint16_t)4672, routers[0].udpPort);
  REQUIRE_EQ((uint16_t)4662, routers[0].tcpPort);
  REQUIRE_EQ((uint8_t)8, routers[0].version);
  REQUIRE_EQ((uint32_t)0x55667788, routers[0].udpKey);

  KadRoutingTable restored(self, 10);
  restored.restore(table.snapshot());
  routers = restored.getRouterContacts();
  REQUIRE_EQ((size_t)1, routers.size());
  REQUIRE_EQ(contact.id, routers[0].id);
  REQUIRE_EQ((uint8_t)8, routers[0].version);
  REQUIRE_EQ((uint32_t)0x55667788, routers[0].udpKey);
}

void Ed2kKadStateTest::testRoutingFindsKnownContactByEndpoint()
{
  auto self = hashFromHex("00000000000000000000000000000000");
  KadRoutingTable table(self, 10);
  auto live = contactFromHex("00000000000000000000000000000009",
                             "203.0.113.9", 4672);
  auto replacement = contactFromHex("0000000000000000000000000000000a",
                                    "203.0.113.10", 4673);
  auto router = contactFromHex("0000000000000000000000000000000b",
                               "203.0.113.11", 4674);
  table.nodeSeen(live, 10);
  table.heardAbout(replacement, 11);
  table.addRouterNode(router);

  KadContact found;
  REQUIRE(table.findByEndpoint(found, endpoint("203.0.113.9", 4672)));
  REQUIRE_EQ(live.id, found.id);
  REQUIRE(table.findByEndpoint(found, endpoint("203.0.113.10", 4673)));
  REQUIRE_EQ(replacement.id, found.id);
  REQUIRE(table.findByEndpoint(found, endpoint("203.0.113.11", 4674)));
  REQUIRE_EQ(router.id, found.id);
  REQUIRE(!table.findByEndpoint(found, endpoint("203.0.113.12", 4675)));
}

void Ed2kKadStateTest::testRoutingFindClosestExcludesRequester()
{
  auto self = hashFromHex("00000000000000000000000000000000");
  KadRoutingTable table(self, 10);
  auto requester = contactFromHex("00000000000000000000000000000001",
                                  "203.0.113.1", 4672);
  auto other = contactFromHex("00000000000000000000000000000002",
                              "203.0.113.2", 4672);
  table.nodeSeen(requester, 10);
  table.nodeSeen(other, 11);

  auto closest =
      table.findClosestExcluding(self, requester.id, 8, false);

  REQUIRE_EQ((size_t)1, closest.size());
  REQUIRE_EQ(other.id, closest[0].id);

  auto kad1 = contactFromHex("00000000000000000000000000000003",
                             "203.0.113.3", 4672);
  kad1.version = 1;
  table.nodeSeen(kad1, 12);
  auto dnsPort = contactFromHex("00000000000000000000000000000004",
                                "203.0.113.4", 53);
  dnsPort.version = 5;
  table.nodeSeen(dnsPort, 13);
  auto acceptedDnsPort = dnsPort;
  acceptedDnsPort.id = hashFromHex("00000000000000000000000000000005");
  acceptedDnsPort.version = 6;
  table.nodeSeen(acceptedDnsPort, 14);

  auto all = table.findClosest(self, 8, false);
  REQUIRE_EQ((size_t)3, all.size());
}

void Ed2kKadStateTest::testRoutingBootstrapAndRefresh()
{
  auto self = hashFromHex("00000000000000000000000000000000");
  KadRoutingTable table(self, 10);
  REQUIRE(table.needBootstrap(100));
  REQUIRE(!table.needBootstrap(120));
  REQUIRE(table.needBootstrap(131));

  std::string target;
  REQUIRE(table.needRefresh(target, 200));
  REQUIRE_EQ(self, target);
  REQUIRE(!table.needRefresh(target, 210));
}

void Ed2kKadStateTest::testKadSourceSearchCadence()
{
  Ed2kAttribute attrs;
  attrs.link.hash = hashFromHex("0123456789abcdef0123456789abcdef");
  attrs.kadRoutingTable =
      std::make_shared<KadRoutingTable>(
          hashFromHex("00000000000000000000000000000000"));

  REQUIRE(!shouldStartEd2kKadSourceSearch(&attrs, 100));

  auto seed = contactFromHex("11111111111111111111111111111111",
                             "203.0.113.1", 4672);
  attrs.kadRoutingTable->heardAbout(seed, 100);
  REQUIRE(shouldStartEd2kKadSourceSearch(&attrs, 100));

  markEd2kKadSourceSearchStarted(&attrs, 100);
  REQUIRE_EQ((int64_t)100, attrs.lastKadSourceSearch);
  REQUIRE_EQ((uint32_t)1, attrs.kadSourceSearchCount);
  REQUIRE(!shouldStartEd2kKadSourceSearch(&attrs, 3699));
  REQUIRE(shouldStartEd2kKadSourceSearch(&attrs, 3700));

  attrs.kadSourceTraversal = make_unique<KadTraversal>(
      KadTraversalKind::SOURCE_LOOKUP, attrs.link.hash, 12345);
  REQUIRE(!shouldStartEd2kKadSourceSearch(&attrs, 3700));
  attrs.kadSourceTraversal.reset();

  markEd2kKadSourceSearchStarted(&attrs, 3700);
  REQUIRE_EQ((uint32_t)2, attrs.kadSourceSearchCount);
  REQUIRE(!shouldStartEd2kKadSourceSearch(&attrs, 10899));
  REQUIRE(shouldStartEd2kKadSourceSearch(&attrs, 10900));

  attrs.peerStates.resize(50);
  REQUIRE(!shouldStartEd2kKadSourceSearch(&attrs, 20000));
}

void Ed2kKadStateTest::testTraversalContinuesBeforeSearch()
{
  auto target = hashFromHex("0123456789abcdef0123456789abcdef");
  auto seed = contactFromHex("11111111111111111111111111111111",
                             "203.0.113.1", 4672);
  auto closer = contactFromHex("0123456789abcdef0123456789abcdee",
                               "203.0.113.2", 4672);
  KadTraversal traversal(KadTraversalKind::SOURCE_LOOKUP, target, 12345, 1, 2);

  auto actions = traversal.start(std::vector<KadContact>{seed});
  REQUIRE_EQ((size_t)1, actions.size());
  REQUIRE_EQ(KadTraversalActionType::FIND_NODE, actions[0].type);
  REQUIRE_EQ(seed.id, actions[0].contact.id);

  actions = traversal.onResponse(seed, std::vector<KadContact>{closer});
  REQUIRE_EQ((size_t)1, actions.size());
  REQUIRE_EQ(KadTraversalActionType::FIND_NODE, actions[0].type);
  REQUIRE_EQ(closer.id, actions[0].contact.id);

  actions = traversal.onResponse(closer, std::vector<KadContact>());
  REQUIRE_EQ((size_t)2, actions.size());
  REQUIRE_EQ(KadTraversalActionType::SEARCH, actions[0].type);
  REQUIRE_EQ(KadTraversalActionType::SEARCH, actions[1].type);
  const bool pairedEitherOrder = (actions[0].contact.id == seed.id &&
                                  actions[1].contact.id == closer.id) ||
                                 (actions[0].contact.id == closer.id &&
                                  actions[1].contact.id == seed.id);
  REQUIRE(pairedEitherOrder);
}

void Ed2kKadStateTest::testExpiredTransactionCarriesContactForFailure()
{
  KadTransactionTable table;
  KadTransaction tx;
  tx.endpoint = endpoint("203.0.113.9", 4672);
  tx.contact = contactFromHex("31d6cfe0d16ae931b73c59d7e0c089c0",
                              "203.0.113.9", 4672);
  tx.expectedOpcode = KAD_RES;
  tx.targetId = hashFromHex("00000000000000000000000000000000");
  tx.sentTime = 100;
  table.add(tx);

  auto expired = table.expire(113, 12);
  REQUIRE_EQ((size_t)1, expired.size());
  REQUIRE_EQ(tx.contact.id, expired[0].contact.id);
  REQUIRE_EQ(tx.contact.host, expired[0].contact.host);
  REQUIRE_EQ(tx.contact.udpPort, expired[0].contact.udpPort);
}

void Ed2kKadStateTest::testTransactionCompletionMatchesTarget()
{
  KadTransactionTable table;
  KadTransaction refresh;
  refresh.endpoint = endpoint("203.0.113.9", 4672);
  refresh.expectedOpcode = KAD_RES;
  refresh.purpose = KadTransactionPurpose::REFRESH;
  refresh.targetId = hashFromHex("ffffffffffffffffffffffffffffffff");
  refresh.sentTime = 100;
  table.add(refresh);

  KadTransaction lookup;
  lookup.endpoint = refresh.endpoint;
  lookup.expectedOpcode = KAD_RES;
  lookup.purpose = KadTransactionPurpose::SOURCE_LOOKUP;
  lookup.targetId = hashFromHex("0123456789abcdef0123456789abcdef");
  lookup.sentTime = 100;
  table.add(lookup);

  KadTransaction completed;
  REQUIRE(table.complete(endpoint("203.0.113.9", 4672), KAD_RES,
                                lookup.targetId, completed));
  REQUIRE_EQ(KadTransactionPurpose::SOURCE_LOOKUP,
                       completed.purpose);
  REQUIRE_EQ(lookup.targetId, completed.targetId);
  REQUIRE_EQ((size_t)1, table.size());
}

void Ed2kKadStateTest::testTransactionCompletionAndExpiry()
{
  KadTransactionTable table;
  KadTransaction tx;
  tx.endpoint = endpoint("203.0.113.9", 4672);
  tx.expectedOpcode = KAD_BOOTSTRAP_RES;
  tx.targetId = hashFromHex("00000000000000000000000000000000");
  tx.sentTime = 100;
  table.add(tx);

  KadTransaction completed;
  REQUIRE(table.complete(endpoint("203.0.113.9", 4672),
                                KAD_BOOTSTRAP_RES, completed));
  REQUIRE_EQ(tx.targetId, completed.targetId);
  REQUIRE_EQ((size_t)0, table.size());

  table.add(tx);
  auto expired = table.expire(113, 12);
  REQUIRE_EQ((size_t)1, expired.size());
  REQUIRE_EQ((size_t)0, table.size());
}

} // namespace ed2k

} // namespace aria2
