#include <ios>
#include <string>
#include "StreamStore.h"

#include <fstream>

#include "a2doctest.h"

#include "File.h"
#include "TestUtil.h"

namespace aria2 {

TEST_CASE("StreamStoreTest.testResumeLookupAndCleanup")
{
  const std::string path = A2_TEST_OUT_DIR "/stream-state.db";
  File(path).remove();
  File(path + "-wal").remove();
  File(path + "-shm").remove();

  StreamStore store(path);
  REQUIRE(store.open());

  StreamState saved;
  saved.gid = "0000000000000001";
  saved.uri = "https://example.test/file.bin";
  saved.path = A2_TEST_OUT_DIR "/file.bin";
  saved.etag = "\"revision-one\"";
  saved.totalLength = 4096;
  saved.completedLength = 1024;
  saved.completedRanges = {{0, 512}, {2048, 2560}};
  REQUIRE(store.save(saved));

  StreamState restored;
  REQUIRE_FALSE(store.load(restored, "0000000000000002", saved.path));
  REQUIRE(store.load(restored, saved.gid, saved.path));
  REQUIRE_EQ(saved.gid, restored.gid);
  REQUIRE_EQ(saved.completedLength, restored.completedLength);
  REQUIRE_EQ(saved.completedRanges, restored.completedRanges);

  saved.gid = "0000000000000002";
  saved.completedLength = 2048;
  saved.completedRanges = {{0, 2048}};
  REQUIRE(store.save(saved));
  REQUIRE(!store.load(restored, "0000000000000001", ""));
  REQUIRE(store.remove("0000000000000001"));
  REQUIRE(store.load(restored, saved.gid, saved.path));
  REQUIRE(store.remove(saved.gid));
  REQUIRE(!store.load(restored, saved.gid, saved.path));
}

TEST_CASE("StreamStoreTest.testPrunesMissingPayloads")
{
  const std::string database = A2_TEST_OUT_DIR "/stream-prune.db";
  const std::string payload = A2_TEST_OUT_DIR "/stream-prune.bin";
  File(database).remove();
  File(database + "-wal").remove();
  File(database + "-shm").remove();
  {
    std::ofstream output(payload, std::ios::binary);
    output.put('x');
  }
  {
    StreamStore store(database);
    REQUIRE(store.open());
    StreamState state;
    state.gid = "0000000000000003";
    state.uri = "https://example.test/prune.bin";
    state.path = payload;
    state.totalLength = 2;
    state.completedLength = 1;
    state.completedRanges = {{0, 1}};
    REQUIRE(store.save(state));
  }
  {
    StreamStore cached(database);
    REQUIRE(cached.open());
    StreamState value;
    REQUIRE(cached.load(value, "0000000000000003", payload));
    CHECK_EQ(1, value.completedLength);
    value.completedLength = 2;
    CHECK_FALSE(cached.save(value));
    REQUIRE(cached.load(value, value.gid, payload));
    CHECK_EQ(1, value.completedLength);
  }
  REQUIRE(File(payload).remove());
  StreamStore reopened(database);
  REQUIRE(reopened.open());
  StreamState restored;
  REQUIRE(!reopened.load(restored, "0000000000000003", payload));
}

} // namespace aria2
