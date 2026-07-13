#include "AbstractCommand.h"

#include <iostream>
#include "a2doctest.h"

#include "Option.h"
#include "prefs.h"
#include "SocketCore.h"
#include "SocketRecvBuffer.h"

namespace aria2 {

class AbstractCommandTest {


public:
  void setUp() {}

  void tearDown() {}

  void testGetProxyUri();
};

A2_TEST(AbstractCommandTest, testGetProxyUri)

void AbstractCommandTest::testGetProxyUri()
{
  Option op;
  REQUIRE_EQ(std::string(), getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY, "http://hu:hp@httpproxy/");
  op.put(PREF_FTP_PROXY, "ftp://fu:fp@ftpproxy/");
  REQUIRE_EQ(std::string("http://hu:hp@httpproxy/"),
                       getProxyUri("http", &op));
  REQUIRE_EQ(std::string("ftp://fu:fp@ftpproxy/"),
                       getProxyUri("ftp", &op));

  op.put(PREF_ALL_PROXY, "http://au:ap@allproxy/");
  REQUIRE_EQ(std::string("http://au:ap@allproxy/"),
                       getProxyUri("https", &op));

  op.put(PREF_ALL_PROXY_USER, "aunew");
  op.put(PREF_ALL_PROXY_PASSWD, "apnew");
  REQUIRE_EQ(std::string("http://aunew:apnew@allproxy/"),
                       getProxyUri("https", &op));

  op.put(PREF_HTTPS_PROXY, "http://hsu:hsp@httpsproxy/");
  REQUIRE_EQ(std::string("http://hsu:hsp@httpsproxy/"),
                       getProxyUri("https", &op));

  REQUIRE_EQ(std::string(), getProxyUri("unknown", &op));

  op.put(PREF_HTTP_PROXY_USER, "hunew");
  REQUIRE_EQ(std::string("http://hunew:hp@httpproxy/"),
                       getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY_PASSWD, "hpnew");
  REQUIRE_EQ(std::string("http://hunew:hpnew@httpproxy/"),
                       getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY_USER, "");
  REQUIRE_EQ(std::string("http://httpproxy/"),
                       getProxyUri("http", &op));
}

} // namespace aria2
