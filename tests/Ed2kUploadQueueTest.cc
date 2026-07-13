#include "Ed2kUploadQueue.h"

#include "a2doctest.h"

#include "ed2k_hash.h"

namespace aria2 {

namespace ed2k {

class Ed2kUploadQueueTest {

public:
  void testRejectsDuplicateUserHash();
  void testCreditsSortWaitingPeers();
};

A2_TEST(Ed2kUploadQueueTest, testRejectsDuplicateUserHash)
A2_TEST(Ed2kUploadQueueTest, testCreditsSortWaitingPeers)

void Ed2kUploadQueueTest::testRejectsDuplicateUserHash()
{
  UploadQueue queue(1);
  Endpoint first;
  first.host = "203.0.113.10";
  first.port = 4662;
  Endpoint duplicate;
  duplicate.host = "203.0.113.11";
  duplicate.port = 4662;
  Endpoint other;
  other.host = "203.0.113.12";
  other.port = 4662;
  const std::string userHash(HASH_LENGTH, '\x44');
  const std::string otherUserHash(HASH_LENGTH, '\x45');
  const std::string fileHash(HASH_LENGTH, '\x66');

  REQUIRE(queue.requestUpload(first, userHash, fileHash, 1000, nullptr));
  REQUIRE(!queue.requestUpload(duplicate, userHash, fileHash, 1001,
                                      nullptr));
  REQUIRE_EQ((size_t)1, queue.peers().size());
  REQUIRE(queue.isUploading(first));

  REQUIRE(!queue.requestUpload(other, otherUserHash, fileHash, 1002,
                                      nullptr));
  REQUIRE_EQ((size_t)2, queue.peers().size());
  REQUIRE_EQ((uint16_t)1, queue.queueRank(other));
}

void Ed2kUploadQueueTest::testCreditsSortWaitingPeers()
{
  UploadQueue queue(1);
  Endpoint active;
  active.host = "203.0.113.10";
  active.port = 4662;
  Endpoint older;
  older.host = "203.0.113.11";
  older.port = 4662;
  Endpoint credited;
  credited.host = "203.0.113.12";
  credited.port = 4662;
  const std::string activeHash(HASH_LENGTH, '\x40');
  const std::string olderHash(HASH_LENGTH, '\x41');
  const std::string creditedHash(HASH_LENGTH, '\x42');
  const std::string fileHash(HASH_LENGTH, '\x66');

  REQUIRE(queue.requestUpload(active, activeHash, fileHash, 1000,
                                     nullptr));
  REQUIRE(!queue.requestUpload(older, olderHash, fileHash, 1001,
                                      nullptr));
  queue.credits().addDownloaded(creditedHash, 4 * 1024 * 1024);
  REQUIRE(!queue.requestUpload(credited, creditedHash, fileHash, 1002,
                                      nullptr));

  REQUIRE_EQ((uint16_t)2, queue.queueRank(older));
  REQUIRE_EQ((uint16_t)1, queue.queueRank(credited));
}

} // namespace ed2k

} // namespace aria2
