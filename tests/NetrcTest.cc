#include "Netrc.h"

#include <iostream>
#include "a2doctest.h"

#include "Exception.h"
#include "a2functional.h"

namespace aria2 {

class NetrcTest {


private:
public:
  void setUp() {}

  void testFindAuthenticator();
  void testParse();
  void testParse_fileNotFound();
  void testParse_emptyfile();
  void testParse_malformedNetrc();
};

A2_TEST(NetrcTest, testFindAuthenticator)
A2_TEST(NetrcTest, testParse)
A2_TEST(NetrcTest, testParse_fileNotFound)
A2_TEST(NetrcTest, testParse_emptyfile)
A2_TEST(NetrcTest, testParse_malformedNetrc)

void NetrcTest::testFindAuthenticator()
{
  Netrc netrc;
  netrc.addAuthenticator(make_unique<Authenticator>(
      "host1", "tujikawa", "tujikawapasswd", "tujikawaaccount"));
  netrc.addAuthenticator(make_unique<Authenticator>(
      "host2", "aria2", "aria2password", "aria2account"));
  netrc.addAuthenticator(make_unique<Authenticator>(".my.domain", "dmname",
                                                    "dmpass", "dmaccount"));
  netrc.addAuthenticator(make_unique<DefaultAuthenticator>(
      "default", "defaultpassword", "defaultaccount"));

  auto aria2auth = netrc.findAuthenticator("host2");
  REQUIRE(aria2auth);
  REQUIRE_EQ(std::string("aria2"), aria2auth->getLogin());
  REQUIRE_EQ(std::string("aria2password"), aria2auth->getPassword());
  REQUIRE_EQ(std::string("aria2account"), aria2auth->getAccount());

  auto defaultauth = netrc.findAuthenticator("host3");
  REQUIRE(defaultauth);
  REQUIRE_EQ(std::string("default"), defaultauth->getLogin());
  REQUIRE_EQ(std::string("defaultpassword"),
                       defaultauth->getPassword());
  REQUIRE_EQ(std::string("defaultaccount"),
                       defaultauth->getAccount());

  auto domainMatchAuth = netrc.findAuthenticator("host3.my.domain");
  REQUIRE(domainMatchAuth);
  REQUIRE_EQ(std::string("dmname"), domainMatchAuth->getLogin());

  auto domainMatchAuth2 = netrc.findAuthenticator("my.domain");
  REQUIRE(domainMatchAuth2);
  REQUIRE_EQ(std::string("default"), domainMatchAuth2->getLogin());
}

void NetrcTest::testParse()
{
  Netrc netrc;
  netrc.parse(A2_TEST_DIR "/sample.netrc");
  auto itr = std::begin(netrc.getAuthenticators());

  const auto& tujikawaauth = *itr;
  REQUIRE(tujikawaauth);
  REQUIRE_EQ(std::string("host1"), tujikawaauth->getMachine());
  REQUIRE_EQ(std::string("tujikawa"), tujikawaauth->getLogin());
  REQUIRE_EQ(std::string("tujikawapassword"),
                       tujikawaauth->getPassword());
  REQUIRE_EQ(std::string("tujikawaaccount"),
                       tujikawaauth->getAccount());
  ++itr;
  const auto& aria2auth = *itr;
  REQUIRE(aria2auth);
  REQUIRE_EQ(std::string("host2"), aria2auth->getMachine());
  REQUIRE_EQ(std::string("aria2"), aria2auth->getLogin());
  REQUIRE_EQ(std::string("aria2password"), aria2auth->getPassword());
  REQUIRE_EQ(std::string("aria2account"), aria2auth->getAccount());
  ++itr;
  const auto& defaultauth = *itr;
  REQUIRE(defaultauth);
  REQUIRE_EQ(std::string("anonymous"), defaultauth->getLogin());
  REQUIRE_EQ(std::string("ARIA2@USER"), defaultauth->getPassword());
  REQUIRE_EQ(std::string("ARIA2@ACCT"), defaultauth->getAccount());
}

void NetrcTest::testParse_fileNotFound()
{
  Netrc netrc;
  try {
    netrc.parse("");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

void NetrcTest::testParse_emptyfile()
{
  Netrc netrc;
  netrc.parse(A2_TEST_DIR "/emptyfile");

  REQUIRE_EQ((size_t)0, netrc.getAuthenticators().size());
}

void NetrcTest::testParse_malformedNetrc()
{
  Netrc netrc;
  try {
    netrc.parse(A2_TEST_DIR "/malformed.netrc");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

} // namespace aria2
