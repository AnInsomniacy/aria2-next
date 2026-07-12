#include "Ed2kShareIndex.h"

#include <cppunit/extensions/HelperMacros.h>

#include "DefaultPieceStorage.h"
#include "DiskAdaptor.h"
#include "DownloadContext.h"
#include "Ed2kAttribute.h"
#include "FileEntry.h"
#include "Option.h"
#include "Piece.h"
#include "RequestGroup.h"
#include "RequestGroupMan.h"
#include "TestUtil.h"
#include "ed2k_hash.h"
#include "ed2k_peer.h"
#include "prefs.h"

namespace aria2 {

namespace ed2k {

class Ed2kShareIndexTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(Ed2kShareIndexTest);
  CPPUNIT_TEST(testActiveSourceExposesVerifiedPiecesOnly);
  CPPUNIT_TEST(testActiveSourceRejectsUnverifiedRange);
  CPPUNIT_TEST(testRequestGroupManFindsActiveSource);
  CPPUNIT_TEST(testOfferFilesPayloadSkipsLargeFilesWithoutServerSupport);
  CPPUNIT_TEST_SUITE_END();

public:
  void testActiveSourceExposesVerifiedPiecesOnly();
  void testActiveSourceRejectsUnverifiedRange();
  void testRequestGroupManFindsActiveSource();
  void testOfferFilesPayloadSkipsLargeFilesWithoutServerSupport();
};

CPPUNIT_TEST_SUITE_REGISTRATION(Ed2kShareIndexTest);

namespace {

class TestSharedSource : public SharedSource {
private:
  std::string hash_;
  std::string name_;
  int64_t size_;

public:
  TestSharedSource(std::string hash, std::string name, int64_t size)
      : hash_(std::move(hash)), name_(std::move(name)), size_(size)
  {
  }

  const std::string& hash() const override { return hash_; }
  const std::string& aichRootHash() const override
  {
    static const std::string empty;
    return empty;
  }
  const std::vector<std::string>& pieceHashes() const override
  {
    static const std::vector<std::string> empty;
    return empty;
  }
  const std::string& name() const override { return name_; }
  int64_t size() const override { return size_; }
  bool complete() const override { return true; }
  std::vector<bool> bitfield() const override
  {
    return std::vector<bool>(1, true);
  }
  bool readRange(std::string& data, int64_t begin,
                 int64_t end) const override
  {
    data.assign(static_cast<size_t>(end - begin), '\0');
    return true;
  }
  void recordUpload(size_t bytes) override {}
};

std::shared_ptr<DownloadContext> createEd2kContext(const std::string& path,
                                                   int64_t size,
                                                   int32_t pieceLength)
{
  auto dctx = std::make_shared<DownloadContext>(pieceLength, size, path);
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.type = LinkType::FILE;
  attrs->link.name = "active.bin";
  attrs->link.size = size;
  attrs->link.hash.assign(HASH_LENGTH, '\x11');
  attrs->pieceHashes.push_back(std::string(HASH_LENGTH, '\x22'));
  attrs->pieceHashes.push_back(std::string(HASH_LENGTH, '\x33'));
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  return dctx;
}

std::shared_ptr<DefaultPieceStorage>
createPieceStorage(const std::shared_ptr<DownloadContext>& dctx,
                   const std::shared_ptr<Option>& option)
{
  auto pieceStorage = std::make_shared<DefaultPieceStorage>(dctx, option.get());
  pieceStorage->initStorage();
  pieceStorage->getDiskAdaptor()->openFile();
  return pieceStorage;
}

} // namespace

void Ed2kShareIndexTest::testActiveSourceExposesVerifiedPiecesOnly()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, ".");
  const std::string path = A2_TEST_OUT_DIR "/ed2k-share-active.bin";
  const auto dctx = createEd2kContext(path, 8, 4);
  auto pieceStorage = createPieceStorage(dctx, option);
  pieceStorage->completePiece(std::make_shared<Piece>(0, 4));

  auto source = createActiveSharedSource(dctx.get(), pieceStorage.get(), path);

  CPPUNIT_ASSERT(source);
  CPPUNIT_ASSERT(!source->complete());
  CPPUNIT_ASSERT_EQUAL(std::string("active.bin"), source->name());
  CPPUNIT_ASSERT_EQUAL((int64_t)8, source->size());
  auto bitfield = source->bitfield();
  CPPUNIT_ASSERT_EQUAL((size_t)2, bitfield.size());
  CPPUNIT_ASSERT(bitfield[0]);
  CPPUNIT_ASSERT(!bitfield[1]);
}

void Ed2kShareIndexTest::testActiveSourceRejectsUnverifiedRange()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, ".");
  const std::string path = A2_TEST_OUT_DIR "/ed2k-share-active-range.bin";
  {
    std::ofstream out(path.c_str(), std::ios::binary);
    out << "abcdefgh";
  }
  const auto dctx = createEd2kContext(path, 8, 4);
  auto pieceStorage = createPieceStorage(dctx, option);
  pieceStorage->completePiece(std::make_shared<Piece>(0, 4));

  auto source = createActiveSharedSource(dctx.get(), pieceStorage.get(), path);

  std::string data;
  CPPUNIT_ASSERT(source->readRange(data, 0, 4));
  CPPUNIT_ASSERT_EQUAL(std::string("abcd"), data);
  CPPUNIT_ASSERT(!source->readRange(data, 4, 8));
  CPPUNIT_ASSERT(!source->readRange(data, 2, 6));
}

void Ed2kShareIndexTest::testRequestGroupManFindsActiveSource()
{
  auto option = std::make_shared<Option>();
  option->put(PREF_DIR, ".");
  const std::string path = A2_TEST_OUT_DIR "/ed2k-share-rgman.bin";
  const auto dctx = createEd2kContext(path, 8, 4);
  auto pieceStorage = createPieceStorage(dctx, option);
  pieceStorage->completePiece(std::make_shared<Piece>(0, 4));

  auto group = std::make_shared<RequestGroup>(GroupId::create(), option);
  group->setDownloadContext(dctx);
  group->setPieceStorage(pieceStorage);
  RequestGroupMan rgman(std::vector<std::shared_ptr<RequestGroup>>{}, 1,
                        option.get());
  rgman.addRequestGroup(group);

  auto attrs = getEd2kAttrs(dctx);
  auto source = findSharedSource(&rgman, attrs->link.hash);
  CPPUNIT_ASSERT(source);
  CPPUNIT_ASSERT(!source->complete());
  CPPUNIT_ASSERT_EQUAL(std::string("active.bin"), source->name());
}

void Ed2kShareIndexTest::testOfferFilesPayloadSkipsLargeFilesWithoutServerSupport()
{
  std::vector<std::shared_ptr<SharedSource>> sources;
  sources.push_back(std::make_shared<TestSharedSource>(
      std::string(HASH_LENGTH, '\x40'), "small.bin", 123));
  sources.push_back(std::make_shared<TestSharedSource>(
      std::string(HASH_LENGTH, '\x41'), "large.bin",
      (int64_t)5 * 1024 * 1024 * 1024));

  std::string payload;
  CPPUNIT_ASSERT(createOfferFilesPayload(payload, sources, false, 10, 0, 0));
  size_t offset = 0;
  CPPUNIT_ASSERT_EQUAL((uint32_t)1,
                       readUInt32(readBytes(payload, offset, 4).data()));
  CPPUNIT_ASSERT_EQUAL(std::string(HASH_LENGTH, '\x40'),
                       readBytes(payload, offset, HASH_LENGTH));

  CPPUNIT_ASSERT(createOfferFilesPayload(payload, sources, true, 10, 0, 0));
  offset = 0;
  CPPUNIT_ASSERT_EQUAL((uint32_t)2,
                       readUInt32(readBytes(payload, offset, 4).data()));
}

} // namespace ed2k

} // namespace aria2
