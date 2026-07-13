#include "FeedbackURISelector.h"

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "array_fun.h"
#include "ServerStatMan.h"
#include "ServerStat.h"
#include "FileEntry.h"

namespace aria2 {

class FeedbackURISelectorTest {


private:
  FileEntry fileEntry_;

  std::shared_ptr<ServerStatMan> ssm;

  std::shared_ptr<FeedbackURISelector> sel;

public:
  void setUp()
  {
    fileEntry_.setUris(
        {"http://alpha/file", "ftp://alpha/file", "http://bravo/file"});

    ssm.reset(new ServerStatMan());
    sel.reset(new FeedbackURISelector(ssm));
  }

  void tearDown() {}

  void testSelect_withoutServerStat();

  void testSelect();

  void testSelect_withUsedHosts();

  void testSelect_skipErrorHost();
};

A2_TEST(FeedbackURISelectorTest, testSelect_withoutServerStat)
A2_TEST(FeedbackURISelectorTest, testSelect)
A2_TEST(FeedbackURISelectorTest, testSelect_withUsedHosts)
A2_TEST(FeedbackURISelectorTest, testSelect_skipErrorHost)

void FeedbackURISelectorTest::testSelect_withoutServerStat()
{
  std::vector<std::pair<size_t, std::string>> usedHosts;
  // Without ServerStat and usedHosts, selector returns first URI
  std::string uri = sel->select(&fileEntry_, usedHosts);
  REQUIRE_EQ(std::string("http://alpha/file"), uri);
  REQUIRE_EQ((size_t)2, fileEntry_.getRemainingUris().size());
}

void FeedbackURISelectorTest::testSelect()
{
  std::shared_ptr<ServerStat> bravo(new ServerStat("bravo", "http"));
  bravo->updateDownloadSpeed(100000);
  std::shared_ptr<ServerStat> alphaFTP(new ServerStat("alpha", "ftp"));
  alphaFTP->updateDownloadSpeed(80000);
  std::shared_ptr<ServerStat> alphaHTTP(new ServerStat("alpha", "http"));
  alphaHTTP->updateDownloadSpeed(180000);
  alphaHTTP->setError();
  std::vector<std::pair<size_t, std::string>> usedHosts;

  ssm->add(bravo);
  ssm->add(alphaFTP);
  ssm->add(alphaHTTP);

  REQUIRE_EQ(std::string("http://bravo/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ((size_t)2, fileEntry_.getRemainingUris().size());

  REQUIRE_EQ(std::string("ftp://alpha/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ((size_t)1, fileEntry_.getRemainingUris().size());
}

void FeedbackURISelectorTest::testSelect_withUsedHosts()
{
  std::vector<std::pair<size_t, std::string>> usedHosts;
  usedHosts.push_back(std::make_pair(1, "bravo"));
  usedHosts.push_back(std::make_pair(2, "alpha"));

  REQUIRE_EQ(std::string("http://bravo/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ((size_t)2, fileEntry_.getRemainingUris().size());

  REQUIRE_EQ(std::string("http://alpha/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ((size_t)1, fileEntry_.getRemainingUris().size());

  REQUIRE_EQ(std::string("ftp://alpha/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ((size_t)0, fileEntry_.getRemainingUris().size());
}

void FeedbackURISelectorTest::testSelect_skipErrorHost()
{
  std::shared_ptr<ServerStat> alphaHTTP(new ServerStat("alpha", "http"));
  alphaHTTP->setError();
  std::shared_ptr<ServerStat> alphaFTP(new ServerStat("alpha", "ftp"));
  alphaFTP->setError();
  std::vector<std::pair<size_t, std::string>> usedHosts;

  ssm->add(alphaHTTP);
  ssm->add(alphaFTP);

  REQUIRE_EQ(std::string("http://bravo/file"),
                       sel->select(&fileEntry_, usedHosts));
  REQUIRE_EQ((size_t)2, fileEntry_.getRemainingUris().size());
}

} // namespace aria2
