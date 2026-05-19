#include "Ed2kSharedStore.h"

#include <cppunit/extensions/HelperMacros.h>

#include "DownloadResult.h"
#include "Ed2kAttribute.h"
#include "Ed2kSharedFile.h"
#include "File.h"
#include "FileEntry.h"
#include "Option.h"
#include "TestUtil.h"
#include "ed2k_link.h"
#include "ed2k_packet.h"
#include "ed2k_peer.h"
#include "prefs.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

class Ed2kSharedStoreTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(Ed2kSharedStoreTest);
  CPPUNIT_TEST(testAddCompletedDownload);
  CPPUNIT_TEST(testRejectMissingOrWrongSizeFile);
  CPPUNIT_TEST(testSharedFileStatePayload);
  CPPUNIT_TEST(testImportOptionFile);
  CPPUNIT_TEST(testSharedFileProtocolPayloads);
  CPPUNIT_TEST(testSharedFilePartPayload);
  CPPUNIT_TEST(testSharedFileAichAnswerPayload);
  CPPUNIT_TEST_SUITE_END();

public:
  void testAddCompletedDownload();
  void testRejectMissingOrWrongSizeFile();
  void testSharedFileStatePayload();
  void testImportOptionFile();
  void testSharedFileProtocolPayloads();
  void testSharedFilePartPayload();
  void testSharedFileAichAnswerPayload();
};

CPPUNIT_TEST_SUITE_REGISTRATION(Ed2kSharedStoreTest);

namespace {

std::shared_ptr<DownloadResult> createEd2kResult(const std::string& path,
                                                 int64_t size,
                                                 const std::string& hash)
{
  auto result = std::make_shared<DownloadResult>();
  result->result = error_code::FINISHED;
  result->totalLength = size;
  result->fileEntries.push_back(std::make_shared<FileEntry>(path, size, 0));
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.type = LinkType::FILE;
  attrs->link.name = "shared.bin";
  attrs->link.size = size;
  attrs->link.hash = hash;
  attrs->pieceHashes.push_back(hash);
  attrs->aichRootHash.assign(AICH_HASH_LENGTH, '\x22');
  result->attrs.push_back(attrs);
  return result;
}

} // namespace

void Ed2kSharedStoreTest::testAddCompletedDownload()
{
  const std::string hash(16, '\x11');
  const std::string path = A2_TEST_OUT_DIR "/ed2k-shared-store-completed.bin";
  createFile(path, 123);

  SharedStore store;
  CPPUNIT_ASSERT(store.addCompletedDownload(*createEd2kResult(path, 123, hash),
                                            1000));

  auto file = store.findByHash(hash);
  CPPUNIT_ASSERT(file);
  CPPUNIT_ASSERT_EQUAL(std::string("shared.bin"), file->name);
  CPPUNIT_ASSERT_EQUAL(path, file->path);
  CPPUNIT_ASSERT_EQUAL((int64_t)123, file->size);
  CPPUNIT_ASSERT_EQUAL((size_t)1, file->pieceHashes.size());
  CPPUNIT_ASSERT_EQUAL((int64_t)1000, file->lastHashTime);
  CPPUNIT_ASSERT(file->completed);
  CPPUNIT_ASSERT(file->origin == SharedOrigin::COMPLETED_DOWNLOAD);

  const std::string updatedPath =
      A2_TEST_OUT_DIR "/ed2k-shared-store-updated.bin";
  createFile(updatedPath, 123);
  CPPUNIT_ASSERT(store.addCompletedDownload(
      *createEd2kResult(updatedPath, 123, hash), 2000));
  CPPUNIT_ASSERT_EQUAL((size_t)1, store.size());
  file = store.findByHash(hash);
  CPPUNIT_ASSERT(file);
  CPPUNIT_ASSERT_EQUAL(updatedPath, file->path);
  CPPUNIT_ASSERT_EQUAL((int64_t)2000, file->lastHashTime);
}

void Ed2kSharedStoreTest::testRejectMissingOrWrongSizeFile()
{
  const std::string hash(16, '\x33');
  const std::string missingPath =
      A2_TEST_OUT_DIR "/ed2k-shared-store-missing.bin";
  File(missingPath).remove();

  SharedStore store;
  CPPUNIT_ASSERT(!store.addCompletedDownload(
      *createEd2kResult(missingPath, 123, hash), 1000));
  CPPUNIT_ASSERT(!store.findByHash(hash));

  const std::string wrongSizePath =
      A2_TEST_OUT_DIR "/ed2k-shared-store-wrong-size.bin";
  createFile(wrongSizePath, 122);
  CPPUNIT_ASSERT(!store.addCompletedDownload(
      *createEd2kResult(wrongSizePath, 123, hash), 1000));
  CPPUNIT_ASSERT_EQUAL((size_t)0, store.size());
}

void Ed2kSharedStoreTest::testSharedFileStatePayload()
{
  const std::string path = A2_TEST_OUT_DIR "/ed2k-shared-store-state.bin";
  createFile(path, 123);

  SharedFile file;
  file.hash.assign(16, '\x55');
  file.aichRootHash.assign(20, '\x66');
  file.pieceHashes.push_back(file.hash);
  file.path = path;
  file.name = "state file.bin";
  file.size = 123;
  file.lastHashTime = 3000;
  file.origin = SharedOrigin::IMPORTED_FILE;
  file.completed = true;

  SharedFile parsed;
  CPPUNIT_ASSERT(
      parseSharedFileStatePayload(parsed, createSharedFileStatePayload(file)));
  CPPUNIT_ASSERT_EQUAL(file.hash, parsed.hash);
  CPPUNIT_ASSERT_EQUAL(file.aichRootHash, parsed.aichRootHash);
  CPPUNIT_ASSERT_EQUAL(file.pieceHashes[0], parsed.pieceHashes[0]);
  CPPUNIT_ASSERT_EQUAL(path, parsed.path);
  CPPUNIT_ASSERT_EQUAL(std::string("state file.bin"), parsed.name);
  CPPUNIT_ASSERT_EQUAL((int64_t)123, parsed.size);
  CPPUNIT_ASSERT_EQUAL((int64_t)3000, parsed.lastHashTime);
  CPPUNIT_ASSERT(parsed.origin == SharedOrigin::IMPORTED_FILE);
}

void Ed2kSharedStoreTest::testImportOptionFile()
{
  const std::string path = A2_TEST_OUT_DIR "/ed2k-shared-store-import.bin";
  createFile(path, 64);

  Option option;
  option.put(PREF_ED2K_SHARE_FILE, path + "\n");
  SharedStore store;
  CPPUNIT_ASSERT_EQUAL((size_t)1, store.importOptionFiles(&option, 4000));
  CPPUNIT_ASSERT_EQUAL((size_t)1, store.size());
  auto files = store.list();
  CPPUNIT_ASSERT_EQUAL(path, files[0].path);
  CPPUNIT_ASSERT_EQUAL(File(path).getBasename(), files[0].name);
  CPPUNIT_ASSERT_EQUAL((int64_t)64, files[0].size);
  CPPUNIT_ASSERT_EQUAL((int64_t)4000, files[0].lastHashTime);
  CPPUNIT_ASSERT(files[0].origin == SharedOrigin::IMPORTED_FILE);
  CPPUNIT_ASSERT_EQUAL(ed2k::md4Digest(readFile(path)), files[0].hash);
}

void Ed2kSharedStoreTest::testSharedFileProtocolPayloads()
{
  const std::string path = A2_TEST_OUT_DIR "/ed2k-shared-protocol.bin";
  createFile(path, 123);

  SharedFile file;
  file.hash.assign(16, '\x77');
  file.pieceHashes.push_back(file.hash);
  file.path = path;
  file.name = "protocol.bin";
  file.size = 123;
  file.completed = true;

  auto name = createSharedFileNameAnswerPayload(file);
  CPPUNIT_ASSERT_EQUAL(file.hash, name.substr(0, HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL((uint16_t)file.name.size(),
                       readUInt16(name.data() + HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL(file.name, name.substr(HASH_LENGTH + 2));

  std::vector<bool> bitfield;
  CPPUNIT_ASSERT(parseFileStatusPayload(
      bitfield, createSharedFileStatusPayload(file), file.hash));
  CPPUNIT_ASSERT_EQUAL((size_t)1, bitfield.size());
  CPPUNIT_ASSERT(bitfield[0]);

  std::vector<std::string> hashes;
  std::string payload;
  CPPUNIT_ASSERT(createSharedFileHashSetPayload(payload, file));
  CPPUNIT_ASSERT(parseHashSetAnswerPayload(hashes, payload, file.hash));
  CPPUNIT_ASSERT(hashes.empty());

  file.size = static_cast<int64_t>(PIECE_LENGTH);
  file.hash = rootHash(std::vector<std::string>{
      std::string(HASH_LENGTH, '\x33'), md4Digest("")});
  file.pieceHashes.clear();
  file.pieceHashes.push_back(std::string(HASH_LENGTH, '\x33'));
  file.pieceHashes.push_back(md4Digest(""));
  CPPUNIT_ASSERT(createSharedFileHashSetPayload(payload, file));
  CPPUNIT_ASSERT(parseHashSetAnswerPayload(hashes, payload, file.hash));
  CPPUNIT_ASSERT_EQUAL((size_t)2, hashes.size());
  CPPUNIT_ASSERT_EQUAL(md4Digest(""), hashes[1]);

  file.pieceHashes.clear();
  file.size = static_cast<int64_t>(PIECE_LENGTH) + 1;
  CPPUNIT_ASSERT(!createSharedFileHashSetPayload(payload, file));
}

void Ed2kSharedStoreTest::testSharedFilePartPayload()
{
  const std::string path = A2_TEST_OUT_DIR "/ed2k-shared-part.bin";
  {
    std::ofstream out(path.c_str(), std::ios::binary);
    out << "0123456789abcdef";
  }

  SharedFile file;
  file.hash.assign(16, '\x88');
  file.path = path;
  file.name = "part.bin";
  file.size = 16;
  file.completed = true;

  PartRange range;
  range.begin = 2;
  range.end = 6;
  std::string payload;
  CPPUNIT_ASSERT(createSharedFilePartPayload(payload, file, range, false));
  CPPUNIT_ASSERT_EQUAL(file.hash, payload.substr(0, HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL((uint32_t)2, readUInt32(payload.data() + HASH_LENGTH));
  CPPUNIT_ASSERT_EQUAL((uint32_t)6,
                       readUInt32(payload.data() + HASH_LENGTH + 4));
  CPPUNIT_ASSERT_EQUAL(std::string("2345"), payload.substr(HASH_LENGTH + 8));

  std::vector<PartRange> ranges;
  std::string hash;
  CPPUNIT_ASSERT(parsePartRequestPayload(
      ranges, hash, createRequestPartsPayload(file.hash,
                                              std::vector<PartRange>(1, range),
                                              false),
      false));
  CPPUNIT_ASSERT_EQUAL(file.hash, hash);
  CPPUNIT_ASSERT_EQUAL((size_t)1, ranges.size());
  CPPUNIT_ASSERT_EQUAL((int64_t)2, ranges[0].begin);
  CPPUNIT_ASSERT_EQUAL((int64_t)6, ranges[0].end);

  range.end = 17;
  CPPUNIT_ASSERT(!createSharedFilePartPayload(payload, file, range, false));
}

void Ed2kSharedStoreTest::testSharedFileAichAnswerPayload()
{
  const std::string path = A2_TEST_OUT_DIR "/ed2k-shared-aich.bin";
  std::string block0(EMBLOCK_LENGTH, 'a');
  std::string block1(EMBLOCK_LENGTH, 'b');
  std::string block2(100, 'c');
  const auto data = block0 + block1 + block2;
  {
    std::ofstream out(path.c_str(), std::ios::binary);
    out << data;
  }

  SharedFile file;
  file.hash.assign(16, '\x99');
  file.path = path;
  file.name = "aich.bin";
  file.size = data.size();
  file.completed = true;
  file.aichRootHash = aichRootHash(data.data(), data.size());

  std::string payload;
  CPPUNIT_ASSERT(createSharedFileAichAnswerPayload(
      payload, file, 0, file.aichRootHash));

  AichAnswer answer;
  CPPUNIT_ASSERT(parseAichAnswerPayload(answer, payload, file.hash));
  CPPUNIT_ASSERT(!answer.failed);
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, answer.partIndex);
  CPPUNIT_ASSERT_EQUAL(file.aichRootHash, answer.rootHash);

  AichRecoveryData recovery;
  CPPUNIT_ASSERT(parseAichRecoveryData(recovery, answer.recoveryData,
                                       data.size(), false));
  CPPUNIT_ASSERT(verifyAichRecoveryData(recovery, file.aichRootHash,
                                        data.size(), 0));
}

} // namespace ed2k

} // namespace aria2
