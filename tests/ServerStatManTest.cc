#include "ServerStatMan.h"

#include <iostream>

#include "a2doctest.h"

#include "ServerStat.h"
#include "Exception.h"
#include "util.h"
#include "BufferedFile.h"
#include "TestUtil.h"

namespace aria2 {

class ServerStatManTest {


public:
  void setUp() {}

  void tearDown() {}

  void testAddAndFind();
  void testSave();
  void testLoad();
  void testRemoveStaleServerStat();
};

A2_TEST(ServerStatManTest, testAddAndFind)
A2_TEST(ServerStatManTest, testSave)
A2_TEST(ServerStatManTest, testLoad)
A2_TEST(ServerStatManTest, testRemoveStaleServerStat)

void ServerStatManTest::testAddAndFind()
{
  std::shared_ptr<ServerStat> localhost_http(
      new ServerStat("localhost", "http"));
  std::shared_ptr<ServerStat> localhost_ftp(new ServerStat("localhost", "ftp"));
  std::shared_ptr<ServerStat> mirror(new ServerStat("mirror", "http"));

  ServerStatMan ssm;
  REQUIRE(ssm.add(localhost_http));
  REQUIRE(!ssm.add(localhost_http));
  REQUIRE(ssm.add(localhost_ftp));
  REQUIRE(ssm.add(mirror));

  {
    std::shared_ptr<ServerStat> r = ssm.find("localhost", "http");
    REQUIRE(r);
    REQUIRE_EQ(std::string("localhost"), r->getHostname());
    REQUIRE_EQ(std::string("http"), r->getProtocol());
  }
  {
    std::shared_ptr<ServerStat> r = ssm.find("mirror", "ftp");
    REQUIRE(!r);
  }
}

void ServerStatManTest::testSave()
{
  std::shared_ptr<ServerStat> localhost_http(
      new ServerStat("localhost", "http"));
  localhost_http->setDownloadSpeed(25000);
  localhost_http->setSingleConnectionAvgSpeed(100);
  localhost_http->setMultiConnectionAvgSpeed(101);
  localhost_http->setCounter(5);
  localhost_http->setLastUpdated(Time(1210000000));
  std::shared_ptr<ServerStat> localhost_ftp(new ServerStat("localhost", "ftp"));
  localhost_ftp->setDownloadSpeed(30000);
  localhost_ftp->setLastUpdated(Time(1210000001));
  std::shared_ptr<ServerStat> mirror(new ServerStat("mirror", "http"));
  mirror->setDownloadSpeed(0);
  mirror->setStatus(ServerStat::A2_ERROR);
  mirror->setLastUpdated(Time(1210000002));

  ServerStatMan ssm;
  REQUIRE(ssm.add(localhost_http));
  REQUIRE(ssm.add(localhost_ftp));
  REQUIRE(ssm.add(mirror));

  const char* filename = A2_TEST_OUT_DIR "/aria2_ServerStatManTest_testSave";
  REQUIRE(ssm.save(filename));
  REQUIRE_EQ(std::string("host=localhost, protocol=ftp,"
                                   " dl_speed=30000,"
                                   " sc_avg_speed=0,"
                                   " mc_avg_speed=0,"
                                   " last_updated=1210000001,"
                                   " counter=0,"
                                   " status=OK\n"

                                   "host=localhost, protocol=http,"
                                   " dl_speed=25000,"
                                   " sc_avg_speed=100,"
                                   " mc_avg_speed=101,"
                                   " last_updated=1210000000,"
                                   " counter=5,"
                                   " status=OK\n"

                                   "host=mirror, protocol=http,"
                                   " dl_speed=0,"
                                   " sc_avg_speed=0,"
                                   " mc_avg_speed=0,"
                                   " last_updated=1210000002,"
                                   " counter=0,"
                                   " status=ERROR\n"),
                       readFile(filename));
}

void ServerStatManTest::testLoad()
{
  const char* filename = A2_TEST_OUT_DIR "/aria2_ServerStatManTest_testLoad";
  std::string in =
      "host=localhost, protocol=ftp, dl_speed=30000, last_updated=1210000001, "
      "status=OK\n"
      "host=localhost, protocol=http, dl_speed=25000, sc_avg_speed=101, "
      "mc_avg_speed=102, last_updated=1210000000, counter=6, status=OK\n"
      "host=mirror, protocol=http, dl_speed=0, last_updated=1210000002, "
      "status=ERROR\n";
  BufferedFile fp(filename, BufferedFile::WRITE);
  REQUIRE_EQ((size_t)in.size(), fp.write(in.data(), in.size()));
  REQUIRE(fp.close() != EOF);

  ServerStatMan ssm;
  REQUIRE(ssm.load(filename));

  std::shared_ptr<ServerStat> localhost_http = ssm.find("localhost", "http");
  REQUIRE(localhost_http);
  REQUIRE_EQ(std::string("localhost"), localhost_http->getHostname());
  REQUIRE_EQ(std::string("http"), localhost_http->getProtocol());
  REQUIRE_EQ(25000, localhost_http->getDownloadSpeed());
  REQUIRE_EQ(101, localhost_http->getSingleConnectionAvgSpeed());
  REQUIRE_EQ(102, localhost_http->getMultiConnectionAvgSpeed());
  REQUIRE_EQ(6, localhost_http->getCounter());
  REQUIRE_EQ(static_cast<time_t>(1210000000),
                       localhost_http->getLastUpdated().getTimeFromEpoch());
  REQUIRE_EQ(ServerStat::OK, localhost_http->getStatus());

  std::shared_ptr<ServerStat> mirror = ssm.find("mirror", "http");
  REQUIRE(mirror);
  REQUIRE_EQ(ServerStat::A2_ERROR, mirror->getStatus());
}

void ServerStatManTest::testRemoveStaleServerStat()
{
  Time now;
  std::shared_ptr<ServerStat> localhost_http(
      new ServerStat("localhost", "http"));
  localhost_http->setDownloadSpeed(25000);
  localhost_http->setLastUpdated(now);
  std::shared_ptr<ServerStat> localhost_ftp(new ServerStat("localhost", "ftp"));
  localhost_ftp->setDownloadSpeed(30000);
  localhost_ftp->setLastUpdated(Time(1210000001));
  std::shared_ptr<ServerStat> mirror(new ServerStat("mirror", "http"));
  mirror->setDownloadSpeed(0);
  mirror->setStatus(ServerStat::A2_ERROR);
  mirror->setLastUpdated(Time(1210000002));

  ServerStatMan ssm;
  REQUIRE(ssm.add(localhost_http));
  REQUIRE(ssm.add(localhost_ftp));
  REQUIRE(ssm.add(mirror));

  ssm.removeStaleServerStat(24_h);

  REQUIRE(ssm.find("localhost", "http"));
  REQUIRE(!ssm.find("localhost", "ftp"));
  REQUIRE(!ssm.find("mirror", "http"));
}

} // namespace aria2
