#include "FileEntry.h"

#include "a2doctest.h"

#include "InorderURISelector.h"
#include "util.h"

namespace aria2 {

class FileEntryTest {


public:
  void setUp() {}

  void testRemoveURIWhoseHostnameIs();
  void testExtractURIResult();
  void testGetRequest();
  void testGetRequest_withoutUriReuse();
  void testGetRequest_withUniqueProtocol();
  void testGetRequest_withReferer();
  void testGetRequest_resetsTryCountAfterWake();
  void testReuseUri();
  void testAddUri();
  void testAddUris();
  void testInsertUri();
  void testRemoveUri();
  void testPutBackRequest();
};

A2_TEST(FileEntryTest, testRemoveURIWhoseHostnameIs)
A2_TEST(FileEntryTest, testExtractURIResult)
A2_TEST(FileEntryTest, testGetRequest)
A2_TEST(FileEntryTest, testGetRequest_withoutUriReuse)
A2_TEST(FileEntryTest, testGetRequest_withUniqueProtocol)
A2_TEST(FileEntryTest, testGetRequest_withReferer)
A2_TEST(FileEntryTest, testGetRequest_resetsTryCountAfterWake)
A2_TEST(FileEntryTest, testReuseUri)
A2_TEST(FileEntryTest, testAddUri)
A2_TEST(FileEntryTest, testAddUris)
A2_TEST(FileEntryTest, testInsertUri)
A2_TEST(FileEntryTest, testRemoveUri)
A2_TEST(FileEntryTest, testPutBackRequest)

namespace {
std::shared_ptr<FileEntry> createFileEntry()
{
  auto fileEntry = std::make_shared<FileEntry>();
  fileEntry->setUris(std::vector<std::string>{"http://localhost/aria2.zip",
                                              "ftp://localhost/aria2.zip",
                                              "http://mirror/aria2.zip"});
  return fileEntry;
}
} // namespace

void FileEntryTest::testRemoveURIWhoseHostnameIs()
{
  auto fileEntry = createFileEntry();
  fileEntry->removeURIWhoseHostnameIs("localhost");
  REQUIRE_EQ((size_t)1, fileEntry->getRemainingUris().size());
  REQUIRE_EQ(std::string("http://mirror/aria2.zip"),
                       fileEntry->getRemainingUris()[0]);
}

void FileEntryTest::testExtractURIResult()
{
  FileEntry fileEntry;
  fileEntry.addURIResult("http://timeout/file", error_code::TIME_OUT);
  fileEntry.addURIResult("http://finished/file", error_code::FINISHED);
  fileEntry.addURIResult("http://timeout/file2", error_code::TIME_OUT);
  fileEntry.addURIResult("http://unknownerror/file", error_code::UNKNOWN_ERROR);

  std::deque<URIResult> res;
  fileEntry.extractURIResult(res, error_code::TIME_OUT);
  REQUIRE_EQ((size_t)2, res.size());
  REQUIRE_EQ(std::string("http://timeout/file"), res[0].getURI());
  REQUIRE_EQ(std::string("http://timeout/file2"), res[1].getURI());

  REQUIRE_EQ((size_t)2, fileEntry.getURIResults().size());
  REQUIRE_EQ(std::string("http://finished/file"),
                       fileEntry.getURIResults()[0].getURI());
  REQUIRE_EQ(std::string("http://unknownerror/file"),
                       fileEntry.getURIResults()[1].getURI());

  res.clear();

  fileEntry.extractURIResult(res, error_code::TIME_OUT);
  REQUIRE(res.empty());
  REQUIRE_EQ((size_t)2, fileEntry.getURIResults().size());
}

void FileEntryTest::testGetRequest()
{
  auto fileEntry = createFileEntry();
  InorderURISelector selector{};
  std::vector<std::pair<size_t, std::string>> usedHosts;
  auto req = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE_EQ(std::string("localhost"), req->getHost());
  REQUIRE_EQ(std::string("http"), req->getProtocol());
  fileEntry->poolRequest(req);

  auto req2nd = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE_EQ(std::string("localhost"), req2nd->getHost());
  REQUIRE_EQ(std::string("http"), req2nd->getProtocol());

  auto req3rd = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE_EQ(std::string("mirror"), req3rd->getHost());
  REQUIRE_EQ(std::string("http"), req3rd->getProtocol());

  auto req4th = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE(!req4th);

  fileEntry->setMaxConnectionPerServer(2);

  auto req5th = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE_EQ(std::string("localhost"), req5th->getHost());
  REQUIRE_EQ(std::string("ftp"), req5th->getProtocol());

  auto req6th = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE_EQ(std::string("mirror"), req6th->getHost());
  REQUIRE_EQ(std::string("http"), req6th->getProtocol());

  auto req7th = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE(!req7th);
}

void FileEntryTest::testGetRequest_withoutUriReuse()
{
  std::vector<std::pair<size_t, std::string>> usedHosts;
  auto fileEntry = createFileEntry();
  fileEntry->setMaxConnectionPerServer(2);
  InorderURISelector selector{};
  auto req = fileEntry->getRequest(&selector, false, usedHosts);
  REQUIRE_EQ(std::string("localhost"), req->getHost());
  REQUIRE_EQ(std::string("http"), req->getProtocol());

  auto req2nd = fileEntry->getRequest(&selector, false, usedHosts);
  REQUIRE_EQ(std::string("localhost"), req2nd->getHost());
  REQUIRE_EQ(std::string("ftp"), req2nd->getProtocol());

  auto req3rd = fileEntry->getRequest(&selector, false, usedHosts);
  REQUIRE_EQ(std::string("mirror"), req3rd->getHost());
  REQUIRE_EQ(std::string("http"), req3rd->getProtocol());

  auto req4th = fileEntry->getRequest(&selector, false, usedHosts);
  REQUIRE(!req4th);
}

void FileEntryTest::testGetRequest_withUniqueProtocol()
{
  std::vector<std::pair<size_t, std::string>> usedHosts;
  auto fileEntry = createFileEntry();
  fileEntry->setUniqueProtocol(true);
  InorderURISelector selector{};
  auto req = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE_EQ(std::string("localhost"), req->getHost());
  REQUIRE_EQ(std::string("http"), req->getProtocol());

  auto req2nd = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE_EQ(std::string("mirror"), req2nd->getHost());
  REQUIRE_EQ(std::string("http"), req2nd->getProtocol());

  auto req3rd = fileEntry->getRequest(&selector, true, usedHosts);
  REQUIRE(!req3rd);

  REQUIRE_EQ((size_t)2, fileEntry->getRemainingUris().size());
  REQUIRE_EQ(std::string("ftp://localhost/aria2.zip"),
                       fileEntry->getRemainingUris()[0]);
  REQUIRE_EQ(std::string("http://mirror/aria2.zip"),
                       fileEntry->getRemainingUris()[1]);
}

void FileEntryTest::testGetRequest_withReferer()
{
  auto fileEntry = createFileEntry();
  InorderURISelector selector{};
  std::vector<std::pair<size_t, std::string>> usedHosts;
  auto req =
      fileEntry->getRequest(&selector, true, usedHosts, "http://referer");
  REQUIRE_EQ(std::string("http://referer"), req->getReferer());
  // URI is used as referer if "*" is given.
  req = fileEntry->getRequest(&selector, true, usedHosts, "*");
  REQUIRE_EQ(req->getUri(), req->getReferer());
}

void FileEntryTest::testGetRequest_resetsTryCountAfterWake()
{
  FileEntry fileEntry;
  fileEntry.addUri("http://example.org/file");
  InorderURISelector selector{};
  std::vector<std::pair<size_t, std::string>> usedHosts;

  auto req = fileEntry.getRequest(&selector, true, usedHosts);
  req->addTryCount();
  req->setResetTryCountAfterWake(true);
  req->setWakeTime(Timer::zero());
  fileEntry.poolRequest(req);

  auto reused = fileEntry.getRequest(&selector, true, usedHosts);

  REQUIRE_EQ(std::string("http://example.org/file"),
                       reused->getUri());
  REQUIRE_EQ(0, reused->getTryCount());
}

void FileEntryTest::testReuseUri()
{
  InorderURISelector selector{};
  auto fileEntry = createFileEntry();
  fileEntry->setMaxConnectionPerServer(3);
  size_t numUris = fileEntry->getRemainingUris().size();
  std::vector<std::pair<size_t, std::string>> usedHosts;
  for (size_t i = 0; i < numUris; ++i) {
    fileEntry->getRequest(&selector, false, usedHosts);
  }
  REQUIRE_EQ((size_t)0, fileEntry->getRemainingUris().size());
  fileEntry->addURIResult("http://localhost/aria2.zip",
                          error_code::UNKNOWN_ERROR);
  std::vector<std::string> ignore;
  fileEntry->reuseUri(ignore);
  REQUIRE_EQ((size_t)2, fileEntry->getRemainingUris().size());
  auto uris = fileEntry->getRemainingUris();
  REQUIRE_EQ(std::string("ftp://localhost/aria2.zip"), uris[0]);
  REQUIRE_EQ(std::string("http://mirror/aria2.zip"), uris[1]);
  for (size_t i = 0; i < 2; ++i) {
    fileEntry->getRequest(&selector, false, usedHosts);
  }
  REQUIRE_EQ((size_t)0, fileEntry->getRemainingUris().size());
  ignore.clear();
  ignore.push_back("mirror");
  fileEntry->reuseUri(ignore);
  REQUIRE_EQ((size_t)1, fileEntry->getRemainingUris().size());
  uris = fileEntry->getRemainingUris();
  REQUIRE_EQ(std::string("ftp://localhost/aria2.zip"), uris[0]);
}

void FileEntryTest::testAddUri()
{
  FileEntry file;
  REQUIRE(file.addUri("http://good"));
  REQUIRE(!file.addUri("bad"));
  // Test for percent-encode
  REQUIRE(file.addUri("http://host:80/file<with%2 %20space/"
                             "file with space;param%?a=/?"));

  REQUIRE_EQ(std::string("http://host:80"
                                   "/file%3Cwith%2%20%20space/"
                                   "file%20with%20space;param%"
                                   "?a=/?"),
                       file.getRemainingUris()[1]);
}

void FileEntryTest::testAddUris()
{
  FileEntry file;
  std::string uris[] = {"bad", "http://good"};
  REQUIRE_EQ((size_t)1, file.addUris(&uris[0], &uris[2]));
}

void FileEntryTest::testInsertUri()
{
  FileEntry file;
  REQUIRE(file.insertUri("http://example.org/1", 0));
  REQUIRE(file.insertUri("http://example.org/2", 0));
  REQUIRE(file.insertUri("http://example.org/3", 1));
  REQUIRE(file.insertUri("http://example.org/4", 5));
  auto& uris = file.getRemainingUris();
  REQUIRE_EQ(std::string("http://example.org/2"), uris[0]);
  REQUIRE_EQ(std::string("http://example.org/3"), uris[1]);
  REQUIRE_EQ(std::string("http://example.org/1"), uris[2]);
  REQUIRE_EQ(std::string("http://example.org/4"), uris[3]);
  // Test for percent-encode
  REQUIRE(file.insertUri("http://host:80/file<with%2 %20space/"
                                "file with space;param%?a=/?",
                                0));

  REQUIRE_EQ(std::string("http://host:80"
                                   "/file%3Cwith%2%20%20space/"
                                   "file%20with%20space;param%"
                                   "?a=/?"),
                       file.getRemainingUris()[0]);
}

void FileEntryTest::testRemoveUri()
{
  std::vector<std::pair<size_t, std::string>> usedHosts;
  InorderURISelector selector{};
  FileEntry file;
  file.addUri("http://example.org/");
  REQUIRE(file.removeUri("http://example.org/"));
  REQUIRE(file.getRemainingUris().empty());
  REQUIRE(!file.removeUri("http://example.org/"));

  file.addUri("http://example.org/");
  auto exampleOrgReq = file.getRequest(&selector, true, usedHosts);
  REQUIRE(!exampleOrgReq->removalRequested());
  REQUIRE_EQ((size_t)1, file.getSpentUris().size());
  REQUIRE(file.removeUri("http://example.org/"));
  REQUIRE(file.getSpentUris().empty());
  REQUIRE(exampleOrgReq->removalRequested());
  file.poolRequest(exampleOrgReq);
  REQUIRE_EQ((size_t)0, file.countPooledRequest());

  file.addUri("http://example.org/");
  exampleOrgReq = file.getRequest(&selector, true, usedHosts);
  file.poolRequest(exampleOrgReq);
  REQUIRE_EQ((size_t)1, file.countPooledRequest());
  REQUIRE(file.removeUri("http://example.org/"));
  REQUIRE_EQ((size_t)0, file.countPooledRequest());
  REQUIRE(file.getSpentUris().empty());

  file.addUri("http://example.org/");
  REQUIRE(!file.removeUri("http://example.net"));
}

void FileEntryTest::testPutBackRequest()
{
  auto fileEntry = createFileEntry();
  InorderURISelector selector{};
  std::vector<std::pair<size_t, std::string>> usedHosts;
  auto req1 = fileEntry->getRequest(&selector, false, usedHosts);
  auto req2 = fileEntry->getRequest(&selector, false, usedHosts);
  REQUIRE_EQ((size_t)1, fileEntry->getRemainingUris().size());
  fileEntry->poolRequest(req2);
  fileEntry->putBackRequest();
  auto& uris = fileEntry->getRemainingUris();
  REQUIRE_EQ((size_t)3, uris.size());
  REQUIRE_EQ(std::string("http://localhost/aria2.zip"), uris[0]);
  REQUIRE_EQ(std::string("http://mirror/aria2.zip"), uris[1]);
  REQUIRE_EQ(std::string("ftp://localhost/aria2.zip"), uris[2]);
}

} // namespace aria2
