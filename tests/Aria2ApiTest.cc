#include "aria2api.h"

#include "a2doctest.h"

#include "TestUtil.h"
#include "prefs.h"
#include "OptionParser.h"
#include "OptionHandler.h"
#include "RequestGroupMan.h"
#include "Context.h"
#include "MultiUrlRequestInfo.h"
#include "DownloadEngine.h"
#include "Option.h"

namespace aria2 {

class Aria2ApiTest {


  Session* session_;

public:
  void setUp()
  {
    SessionConfig config;
    KeyVals options = {{"no-conf", "true"}};
    session_ = sessionNew(options, config);
  }

  void tearDown() { sessionFinal(session_); }

  void testAddUri();
  void testAddMetalink();
  void testAddTorrent();
  void testRemovePause();
  void testChangePosition();
  void testChangeOption();
  void testChangeGlobalOption();
  void testDownloadResultDH();
  void testGetFilesOutlivesDownloadHandle();
};

A2_TEST(Aria2ApiTest, testAddUri)
A2_TEST(Aria2ApiTest, testAddMetalink)
A2_TEST(Aria2ApiTest, testAddTorrent)
A2_TEST(Aria2ApiTest, testRemovePause)
A2_TEST(Aria2ApiTest, testChangePosition)
A2_TEST(Aria2ApiTest, testChangeOption)
A2_TEST(Aria2ApiTest, testChangeGlobalOption)
A2_TEST(Aria2ApiTest, testDownloadResultDH)
A2_TEST(Aria2ApiTest, testGetFilesOutlivesDownloadHandle)

void Aria2ApiTest::testAddUri()
{
  A2Gid gid;
  std::vector<std::string> uris(1);
  KeyVals options;
  uris[0] = "http://localhost/1";
  REQUIRE_EQ(0, addUri(session_, &gid, uris, options));
  REQUIRE(!isNull(gid));

  DownloadHandle* hd = getDownloadHandle(session_, gid);
  REQUIRE(hd);
  REQUIRE_EQ(1, hd->getNumFiles());
  FileData file = hd->getFile(1);
  REQUIRE_EQ((size_t)1, file.uris.size());
  REQUIRE_EQ(uris[0], file.uris[0].uri);
  deleteDownloadHandle(hd);

  options.push_back(KeyVals::value_type("file-allocation", "foo"));
  REQUIRE_EQ(-1, addUri(session_, &gid, uris, options));
}

void Aria2ApiTest::testAddMetalink()
{
  std::string metalinkPath = A2_TEST_DIR "/metalink4.xml";
  std::vector<A2Gid> gids;
  KeyVals options;
#ifdef ENABLE_METALINK
  REQUIRE_EQ(0, addMetalink(session_, &gids, metalinkPath, options));
#  ifdef ENABLE_BITTORRENT
  REQUIRE_EQ((size_t)2, gids.size());
#  else  // !ENABLE_BITTORRENT
  REQUIRE_EQ((size_t)1, gids.size());
#  endif // !ENABLE_BITTORRENT

  gids.clear();
  options.push_back(KeyVals::value_type("file-allocation", "foo"));
  REQUIRE_EQ(-1, addMetalink(session_, &gids, metalinkPath, options));
#else  // !ENABLE_METALINK
  REQUIRE_EQ(-1, addMetalink(session_, &gids, metalinkPath, options));
#endif // !ENABLE_METALINK
}

void Aria2ApiTest::testAddTorrent()
{
  std::string torrentPath = A2_TEST_DIR "/test.torrent";
  A2Gid gid;
  KeyVals options;
#ifdef ENABLE_BITTORRENT
  REQUIRE_EQ(0, addTorrent(session_, &gid, torrentPath, options));
  REQUIRE(!isNull(gid));

  options.push_back(KeyVals::value_type("file-allocation", "foo"));
  REQUIRE_EQ(-1, addTorrent(session_, &gid, torrentPath, options));
#else  // !ENABLE_BITTORRENT
  REQUIRE_EQ(-1, addTorrent(session_, &gid, torrentPath, options));
#endif // !ENABLE_BITTORRENT
}

void Aria2ApiTest::testRemovePause()
{
  A2Gid gid;
  std::vector<std::string> uris(1);
  KeyVals options;
  uris[0] = "http://localhost/1";
  REQUIRE_EQ(0, addUri(session_, &gid, uris, options));

  DownloadHandle* hd = getDownloadHandle(session_, gid);
  REQUIRE(hd);
  REQUIRE_EQ(DOWNLOAD_WAITING, hd->getStatus());
  deleteDownloadHandle(hd);

  REQUIRE_EQ(-1, pauseDownload(session_, (A2Gid)0));
  REQUIRE_EQ(0, pauseDownload(session_, gid));
  hd = getDownloadHandle(session_, gid);
  REQUIRE(hd);
  REQUIRE_EQ(DOWNLOAD_PAUSED, hd->getStatus());
  deleteDownloadHandle(hd);

  REQUIRE_EQ(-1, unpauseDownload(session_, (A2Gid)0));
  REQUIRE_EQ(0, unpauseDownload(session_, gid));
  hd = getDownloadHandle(session_, gid);
  REQUIRE(hd);
  REQUIRE_EQ(DOWNLOAD_WAITING, hd->getStatus());
  deleteDownloadHandle(hd);

  REQUIRE_EQ(-1, removeDownload(session_, (A2Gid)0));
  REQUIRE_EQ(0, removeDownload(session_, gid));
  hd = getDownloadHandle(session_, gid);
  REQUIRE(!hd);
}

void Aria2ApiTest::testChangePosition()
{
  int N = 10;
  std::vector<A2Gid> gids(N);
  std::vector<std::string> uris(1);
  KeyVals options;
  uris[0] = "http://localhost/";
  for (int i = 0; i < N; ++i) {
    REQUIRE_EQ(0, addUri(session_, &gids[i], uris, options));
  }
  REQUIRE_EQ(-1,
                       changePosition(session_, (A2Gid)0, -2, OFFSET_MODE_CUR));
  REQUIRE_EQ(2,
                       changePosition(session_, gids[4], -2, OFFSET_MODE_CUR));

  REQUIRE_EQ(5,
                       changePosition(session_, gids[4], 5, OFFSET_MODE_SET));

  REQUIRE_EQ(7,
                       changePosition(session_, gids[4], -2, OFFSET_MODE_END));
}

void Aria2ApiTest::testChangeOption()
{
  A2Gid gid;
  std::vector<std::string> uris(1);
  KeyVals options;
  uris[0] = "http://localhost/1";
  options.push_back(KeyVals::value_type("dir", "mydownload"));
  REQUIRE_EQ(0, addUri(session_, &gid, uris, options));

  DownloadHandle* hd = getDownloadHandle(session_, gid);
  REQUIRE(hd);
  REQUIRE_EQ(1, hd->getNumFiles());
  FileData file = hd->getFile(1);
  REQUIRE_EQ((size_t)1, file.uris.size());
  REQUIRE_EQ(uris[0], file.uris[0].uri);

  REQUIRE_EQ(std::string("mydownload"), hd->getOption("dir"));
  REQUIRE(hd->getOption("unknown").empty());
  KeyVals retopts = hd->getOptions();
  REQUIRE(std::find(retopts.begin(), retopts.end(),
                           KeyVals::value_type("dir", "mydownload")) !=
                 retopts.end());
  // Don't return hidden option
  REQUIRE(hd->getOption(PREF_STARTUP_IDLE_TIME->k).empty());
  deleteDownloadHandle(hd);
  // failure with null gid
  REQUIRE_EQ(-1, changeOption(session_, (A2Gid)0, options));
  // change option
  options.clear();
  options.push_back(KeyVals::value_type("dir", "newlocation"));
  REQUIRE_EQ(0, changeOption(session_, gid, options));
  hd = getDownloadHandle(session_, gid);
  REQUIRE_EQ(std::string("newlocation"), hd->getOption("dir"));
  deleteDownloadHandle(hd);
  // failure with bad option value
  options.clear();
  options.push_back(KeyVals::value_type("file-allocation", "foo"));
  REQUIRE_EQ(-1, changeOption(session_, gid, options));
}

void Aria2ApiTest::testChangeGlobalOption()
{
  REQUIRE_EQ(OptionParser::getInstance()
                           ->find(PREF_FILE_ALLOCATION)
                           ->getDefaultValue(),
                       getGlobalOption(session_, PREF_FILE_ALLOCATION->k));
  KeyVals options;
  options.push_back(KeyVals::value_type(PREF_FILE_ALLOCATION->k, "none"));
  REQUIRE_EQ(0, changeGlobalOption(session_, options));
  REQUIRE_EQ(std::string("none"),
                       getGlobalOption(session_, PREF_FILE_ALLOCATION->k));
  // Don't return hidden option
  REQUIRE(getGlobalOption(session_, PREF_STARTUP_IDLE_TIME->k).empty());
  // failure with bad option value
  options.clear();
  options.push_back(KeyVals::value_type(PREF_FILE_ALLOCATION->k, "foo"));
  REQUIRE_EQ(-1, changeGlobalOption(session_, options));
}

void Aria2ApiTest::testDownloadResultDH()
{
  std::shared_ptr<DownloadResult> dr1 =
      createDownloadResult(error_code::TIME_OUT, "http://example.org/timeout");
  dr1->option->put(PREF_DIR, "mydownload");
  std::shared_ptr<DownloadResult> dr2 = createDownloadResult(
      error_code::NETWORK_PROBLEM, "http://example.org/network");
  auto& gman =
      session_->context->reqinfo->getDownloadEngine()->getRequestGroupMan();
  gman->addDownloadResult(dr1);
  gman->addDownloadResult(dr2);

  DownloadHandle* hd = getDownloadHandle(session_, dr1->gid->getNumericId());
  REQUIRE_EQ(DOWNLOAD_ERROR, hd->getStatus());
  REQUIRE_EQ((int)error_code::TIME_OUT, hd->getErrorCode());
  REQUIRE_EQ(std::string("mydownload"), hd->getOption(PREF_DIR->k));
  // Don't return hidden option
  REQUIRE(hd->getOption(PREF_STARTUP_IDLE_TIME->k).empty());
  KeyVals retopts = hd->getOptions();
  REQUIRE(std::find(retopts.begin(), retopts.end(),
                           KeyVals::value_type(PREF_DIR->k, "mydownload")) !=
                 retopts.end());
  deleteDownloadHandle(hd);
}

void Aria2ApiTest::testGetFilesOutlivesDownloadHandle()
{
  A2Gid gid;
  std::vector<std::string> uris(1);
  KeyVals options;
  uris[0] = "http://localhost/1";
  REQUIRE_EQ(0, addUri(session_, &gid, uris, options));

  DownloadHandle* hd = getDownloadHandle(session_, gid);
  REQUIRE(hd);

  std::vector<FileData> files = hd->getFiles();
  deleteDownloadHandle(hd);

  REQUIRE_EQ((size_t)1, files.size());
  REQUIRE_EQ(1, files[0].index);
  REQUIRE_EQ((size_t)1, files[0].uris.size());
  REQUIRE_EQ(uris[0], files[0].uris[0].uri);

  std::vector<FileData>().swap(files);
}

} // namespace aria2
