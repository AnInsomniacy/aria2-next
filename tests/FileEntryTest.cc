#include "FileEntry.h"

#include <cppunit/extensions/HelperMacros.h>

#include "util.h"

namespace aria2 {

class FileEntryTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(FileEntryTest);
  CPPUNIT_TEST(testRemoveURIWhoseHostnameIs);
  CPPUNIT_TEST(testExtractURIResult);
  CPPUNIT_TEST(testGetRequest);
  CPPUNIT_TEST(testGetRequest_honorsMaxConnectionPerServer);
  CPPUNIT_TEST(testGetRequest_withReferer);
  CPPUNIT_TEST(testAddUri);
  CPPUNIT_TEST(testAddUris);
  CPPUNIT_TEST(testInsertUri);
  CPPUNIT_TEST(testRemoveUri);
  CPPUNIT_TEST(testPutBackRequest);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void testRemoveURIWhoseHostnameIs();
  void testExtractURIResult();
  void testGetRequest();
  void testGetRequest_honorsMaxConnectionPerServer();
  void testGetRequest_withReferer();
  void testAddUri();
  void testAddUris();
  void testInsertUri();
  void testRemoveUri();
  void testPutBackRequest();
};

CPPUNIT_TEST_SUITE_REGISTRATION(FileEntryTest);

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
  CPPUNIT_ASSERT_EQUAL((size_t)1, fileEntry->getRemainingUris().size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://mirror/aria2.zip"),
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
  CPPUNIT_ASSERT_EQUAL((size_t)2, res.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://timeout/file"), res[0].getURI());
  CPPUNIT_ASSERT_EQUAL(std::string("http://timeout/file2"), res[1].getURI());

  CPPUNIT_ASSERT_EQUAL((size_t)2, fileEntry.getURIResults().size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://finished/file"),
                       fileEntry.getURIResults()[0].getURI());
  CPPUNIT_ASSERT_EQUAL(std::string("http://unknownerror/file"),
                       fileEntry.getURIResults()[1].getURI());

  res.clear();

  fileEntry.extractURIResult(res, error_code::TIME_OUT);
  CPPUNIT_ASSERT(res.empty());
  CPPUNIT_ASSERT_EQUAL((size_t)2, fileEntry.getURIResults().size());
}

void FileEntryTest::testGetRequest()
{
  auto fileEntry = createFileEntry();
  fileEntry->setMaxConnectionPerServer(2);
  auto req = fileEntry->getRequest();
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), req->getHost());
  CPPUNIT_ASSERT_EQUAL(std::string("http"), req->getProtocol());

  auto req2nd = fileEntry->getRequest();
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), req2nd->getHost());
  CPPUNIT_ASSERT_EQUAL(std::string("ftp"), req2nd->getProtocol());

  auto req3rd = fileEntry->getRequest();
  CPPUNIT_ASSERT_EQUAL(std::string("mirror"), req3rd->getHost());
  CPPUNIT_ASSERT_EQUAL(std::string("http"), req3rd->getProtocol());

  auto req4th = fileEntry->getRequest();
  CPPUNIT_ASSERT(!req4th);
}

void FileEntryTest::testGetRequest_honorsMaxConnectionPerServer()
{
  auto fileEntry = createFileEntry();
  auto req = fileEntry->getRequest();
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), req->getHost());
  CPPUNIT_ASSERT_EQUAL(std::string("http"), req->getProtocol());

  auto req2nd = fileEntry->getRequest();
  CPPUNIT_ASSERT_EQUAL(std::string("mirror"), req2nd->getHost());
  CPPUNIT_ASSERT_EQUAL(std::string("http"), req2nd->getProtocol());

  auto req3rd = fileEntry->getRequest();
  CPPUNIT_ASSERT(!req3rd);

  CPPUNIT_ASSERT_EQUAL(std::string("ftp://localhost/aria2.zip"),
                       fileEntry->getRemainingUris()[0]);

  fileEntry->poolRequest(req);
  fileEntry->setMaxConnectionPerServer(2);

  auto req4th = fileEntry->getRequest();
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), req4th->getHost());
  CPPUNIT_ASSERT_EQUAL(std::string("ftp"), req4th->getProtocol());
}

void FileEntryTest::testGetRequest_withReferer()
{
  auto fileEntry = createFileEntry();
  auto req = fileEntry->getRequest("http://referer");
  CPPUNIT_ASSERT_EQUAL(std::string("http://referer"), req->getReferer());
  // URI is used as referer if "*" is given.
  req = fileEntry->getRequest("*");
  CPPUNIT_ASSERT_EQUAL(req->getUri(), req->getReferer());
}

void FileEntryTest::testAddUri()
{
  FileEntry file;
  CPPUNIT_ASSERT(file.addUri("http://good"));
  CPPUNIT_ASSERT(!file.addUri("bad"));
  // Test for percent-encode
  CPPUNIT_ASSERT(file.addUri("http://host:80/file<with%2 %20space/"
                             "file with space;param%?a=/?"));

  CPPUNIT_ASSERT_EQUAL(std::string("http://host:80"
                                   "/file%3Cwith%2%20%20space/"
                                   "file%20with%20space;param%"
                                   "?a=/?"),
                       file.getRemainingUris()[1]);
}

void FileEntryTest::testAddUris()
{
  FileEntry file;
  std::string uris[] = {"bad", "http://good"};
  CPPUNIT_ASSERT_EQUAL((size_t)1, file.addUris(&uris[0], &uris[2]));
}

void FileEntryTest::testInsertUri()
{
  FileEntry file;
  CPPUNIT_ASSERT(file.insertUri("http://example.org/1", 0));
  CPPUNIT_ASSERT(file.insertUri("http://example.org/2", 0));
  CPPUNIT_ASSERT(file.insertUri("http://example.org/3", 1));
  CPPUNIT_ASSERT(file.insertUri("http://example.org/4", 5));
  auto& uris = file.getRemainingUris();
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/2"), uris[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/3"), uris[1]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/1"), uris[2]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/4"), uris[3]);
  // Test for percent-encode
  CPPUNIT_ASSERT(file.insertUri("http://host:80/file<with%2 %20space/"
                                "file with space;param%?a=/?",
                                0));

  CPPUNIT_ASSERT_EQUAL(std::string("http://host:80"
                                   "/file%3Cwith%2%20%20space/"
                                   "file%20with%20space;param%"
                                   "?a=/?"),
                       file.getRemainingUris()[0]);
}

void FileEntryTest::testRemoveUri()
{
  FileEntry file;
  file.addUri("http://example.org/");
  CPPUNIT_ASSERT(file.removeUri("http://example.org/"));
  CPPUNIT_ASSERT(file.getRemainingUris().empty());
  CPPUNIT_ASSERT(!file.removeUri("http://example.org/"));

  file.addUri("http://example.org/");
  auto exampleOrgReq = file.getRequest();
  CPPUNIT_ASSERT(!exampleOrgReq->removalRequested());
  CPPUNIT_ASSERT_EQUAL((size_t)1, file.getSpentUris().size());
  CPPUNIT_ASSERT(file.removeUri("http://example.org/"));
  CPPUNIT_ASSERT(file.getSpentUris().empty());
  CPPUNIT_ASSERT(exampleOrgReq->removalRequested());
  file.poolRequest(exampleOrgReq);

  file.addUri("http://example.org/");
  CPPUNIT_ASSERT(!file.removeUri("http://example.net"));
}

void FileEntryTest::testPutBackRequest()
{
  auto fileEntry = createFileEntry();
  auto req1 = fileEntry->getRequest();
  auto req2 = fileEntry->getRequest();
  CPPUNIT_ASSERT_EQUAL((size_t)1, fileEntry->getRemainingUris().size());
  fileEntry->poolRequest(req2);
  fileEntry->putBackRequest();
  auto& uris = fileEntry->getRemainingUris();
  CPPUNIT_ASSERT_EQUAL((size_t)2, uris.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://localhost/aria2.zip"), uris[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("ftp://localhost/aria2.zip"), uris[1]);
}

} // namespace aria2
