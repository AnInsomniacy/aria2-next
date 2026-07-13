#include "BtRegistry.h"

#include "a2doctest.h"

#include "Exception.h"
#include "DownloadContext.h"
#include "MockPeerStorage.h"
#include "MockPieceStorage.h"
#include "MockBtAnnounce.h"
#include "MockBtProgressInfoFile.h"
#include "BtRuntime.h"
#include "FileEntry.h"
#include "bittorrent_helper.h"
#include "UDPTrackerRequest.h"

namespace aria2 {

class BtRegistryTest {


private:
public:
  void testGetDownloadContext();
  void testGetDownloadContext_infoHash();
  void testGetAllDownloadContext();
  void testRemove();
  void testRemoveAll();
};

A2_TEST(BtRegistryTest, testGetDownloadContext)
A2_TEST(BtRegistryTest, testGetDownloadContext_infoHash)
A2_TEST(BtRegistryTest, testGetAllDownloadContext)
A2_TEST(BtRegistryTest, testRemove)
A2_TEST(BtRegistryTest, testRemoveAll)

void BtRegistryTest::testGetDownloadContext()
{
  BtRegistry btRegistry;
  REQUIRE(!btRegistry.getDownloadContext(1));
  auto dctx = std::make_shared<DownloadContext>();
  auto btObject = make_unique<BtObject>();
  btObject->downloadContext = dctx;
  btRegistry.put(1, std::move(btObject));
  REQUIRE_EQ(dctx.get(), btRegistry.getDownloadContext(1).get());
}

namespace {
void addTwoDownloadContext(BtRegistry& btRegistry)
{
  auto dctx1 = std::make_shared<DownloadContext>();
  auto dctx2 = std::make_shared<DownloadContext>();
  auto btObject1 = make_unique<BtObject>();
  btObject1->downloadContext = dctx1;
  auto btObject2 = make_unique<BtObject>();
  btObject2->downloadContext = dctx2;
  btRegistry.put(1, std::move(btObject1));
  btRegistry.put(2, std::move(btObject2));
}
} // namespace

void BtRegistryTest::testGetDownloadContext_infoHash()
{
  BtRegistry btRegistry;
  addTwoDownloadContext(btRegistry);
  {
    auto attrs1 = make_unique<TorrentAttribute>();
    attrs1->infoHash = "hash1";
    auto attrs2 = make_unique<TorrentAttribute>();
    attrs2->infoHash = "hash2";
    btRegistry.getDownloadContext(1)->setAttribute(CTX_ATTR_BT,
                                                   std::move(attrs1));
    btRegistry.getDownloadContext(2)->setAttribute(CTX_ATTR_BT,
                                                   std::move(attrs2));
  }
  REQUIRE(btRegistry.getDownloadContext("hash1"));
  REQUIRE(btRegistry.getDownloadContext("hash1").get() ==
                 btRegistry.getDownloadContext(1).get());
  REQUIRE(!btRegistry.getDownloadContext("not exists"));
}

void BtRegistryTest::testGetAllDownloadContext()
{
  BtRegistry btRegistry;
  addTwoDownloadContext(btRegistry);

  std::vector<std::shared_ptr<DownloadContext>> result;
  btRegistry.getAllDownloadContext(std::back_inserter(result));
  REQUIRE_EQ((size_t)2, result.size());
}

void BtRegistryTest::testRemove()
{
  BtRegistry btRegistry;
  addTwoDownloadContext(btRegistry);
  REQUIRE(btRegistry.remove(1));
  REQUIRE(!btRegistry.get(1));
  REQUIRE(btRegistry.get(2));
}

void BtRegistryTest::testRemoveAll()
{
  BtRegistry btRegistry;
  addTwoDownloadContext(btRegistry);
  btRegistry.removeAll();
  REQUIRE(!btRegistry.get(1));
  REQUIRE(!btRegistry.get(2));
}

} // namespace aria2
