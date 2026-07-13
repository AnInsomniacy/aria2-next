#include "DownloadContext.h"

#include "a2doctest.h"

#include "FileEntry.h"
#include "array_fun.h"

namespace aria2 {

class DownloadContextTest {


public:
  void testFindFileEntryByOffset();
  void testGetPieceHash();
  void testGetNumPieces();
  void testGetBasePath();
  void testSetFileFilter();
};

A2_TEST(DownloadContextTest, testFindFileEntryByOffset)
A2_TEST(DownloadContextTest, testGetPieceHash)
A2_TEST(DownloadContextTest, testGetNumPieces)
A2_TEST(DownloadContextTest, testGetBasePath)
A2_TEST(DownloadContextTest, testSetFileFilter)

void DownloadContextTest::testFindFileEntryByOffset()
{
  DownloadContext ctx;

  REQUIRE(!ctx.findFileEntryByOffset(0));

  const std::shared_ptr<FileEntry> fileEntries[] = {
      std::shared_ptr<FileEntry>(new FileEntry("file1", 1000, 0)),
      std::shared_ptr<FileEntry>(new FileEntry("file2", 0, 1000)),
      std::shared_ptr<FileEntry>(new FileEntry("file3", 0, 1000)),
      std::shared_ptr<FileEntry>(new FileEntry("file4", 2000, 1000)),
      std::shared_ptr<FileEntry>(new FileEntry("file5", 3000, 3000)),
      std::shared_ptr<FileEntry>(new FileEntry("file6", 0, 6000))};
  ctx.setFileEntries(std::begin(fileEntries), std::end(fileEntries));

  REQUIRE_EQ(std::string("file1"),
                       ctx.findFileEntryByOffset(0)->getPath());
  REQUIRE_EQ(std::string("file4"),
                       ctx.findFileEntryByOffset(1500)->getPath());
  REQUIRE_EQ(std::string("file5"),
                       ctx.findFileEntryByOffset(5999)->getPath());
  REQUIRE(!ctx.findFileEntryByOffset(6000));
}

void DownloadContextTest::testGetPieceHash()
{
  DownloadContext ctx;
  const std::string pieceHashes[] = {"hash1", "hash2", "shash3"};
  ctx.setPieceHashes("sha-1", &pieceHashes[0], &pieceHashes[3]);
  REQUIRE_EQ(std::string("hash1"), ctx.getPieceHash(0));
  REQUIRE_EQ(std::string(""), ctx.getPieceHash(3));
}

void DownloadContextTest::testGetNumPieces()
{
  DownloadContext ctx(345, 9889, "");
  REQUIRE_EQ((size_t)29, ctx.getNumPieces());
}

void DownloadContextTest::testGetBasePath()
{
  DownloadContext ctx(0, 0, "");
  REQUIRE_EQ(std::string(""), ctx.getBasePath());
  ctx.getFirstFileEntry()->setPath("aria2.tar.bz2");
  REQUIRE_EQ(std::string("aria2.tar.bz2"), ctx.getBasePath());
}

void DownloadContextTest::testSetFileFilter()
{
  DownloadContext ctx;
  std::vector<std::shared_ptr<FileEntry>> files;
  for (int i = 0; i < 10; ++i) {
    files.push_back(std::shared_ptr<FileEntry>(new FileEntry("file", 1, i)));
  }
  ctx.setFileEntries(files.begin(), files.end());
  auto sgl = util::parseIntSegments("6-8,2-4");
  sgl.normalize();
  ctx.setFileFilter(std::move(sgl));
  const std::vector<std::shared_ptr<FileEntry>>& res = ctx.getFileEntries();
  REQUIRE(!res[0]->isRequested());
  REQUIRE(res[1]->isRequested());
  REQUIRE(res[2]->isRequested());
  REQUIRE(res[3]->isRequested());
  REQUIRE(!res[4]->isRequested());
  REQUIRE(res[5]->isRequested());
  REQUIRE(res[6]->isRequested());
  REQUIRE(res[7]->isRequested());
  REQUIRE(!res[8]->isRequested());
  REQUIRE(!res[9]->isRequested());
}

} // namespace aria2
