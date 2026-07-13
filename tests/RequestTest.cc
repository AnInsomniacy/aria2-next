#include "Request.h"

#include "a2doctest.h"

#include "Netrc.h"
#include "DefaultAuthResolver.h"
#include "NetrcAuthResolver.h"
#include "uri.h"

namespace aria2 {

class RequestTest {


public:
  void testSetUri1();
  void testSetUri2();
  void testSetUri7();
  void testSetUri_supportsPersistentConnection();
  void testRedirectUri();
  void testRedirectUri2();
  void testRedirectUri_supportsPersistentConnection();
  void testRedirectUri_uriNormalization();
  void testResetUri();
  void testResetUri_supportsPersistentConnection();
  void testInnerLink();
  void testInnerLinkInReferer();
  void testGetURIHost();
};

A2_TEST(RequestTest, testSetUri1)
A2_TEST(RequestTest, testSetUri2)
A2_TEST(RequestTest, testSetUri7)
A2_TEST(RequestTest, testSetUri_supportsPersistentConnection)
A2_TEST(RequestTest, testRedirectUri)
A2_TEST(RequestTest, testRedirectUri2)
A2_TEST(RequestTest, testRedirectUri_supportsPersistentConnection)
A2_TEST(RequestTest, testRedirectUri_uriNormalization)
A2_TEST(RequestTest, testResetUri)
A2_TEST(RequestTest, testResetUri_supportsPersistentConnection)
A2_TEST(RequestTest, testInnerLink)
A2_TEST(RequestTest, testInnerLinkInReferer)
A2_TEST(RequestTest, testGetURIHost)

void RequestTest::testSetUri1()
{
  Request req;
  bool v = req.setUri("http://aria.rednoah.com/");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http://aria.rednoah.com/"), req.getUri());
  REQUIRE_EQ(std::string("http://aria.rednoah.com/"),
                       req.getCurrentUri());
  REQUIRE_EQ(std::string(""), req.getReferer());
  REQUIRE_EQ(std::string("http"), req.getProtocol());
  REQUIRE_EQ((uint16_t)80, req.getPort());
  REQUIRE_EQ(std::string("aria.rednoah.com"), req.getHost());
  REQUIRE_EQ(std::string("/"), req.getDir());
  REQUIRE_EQ(std::string(""), req.getFile());
  REQUIRE_EQ(std::string(""), req.getQuery());
  REQUIRE_EQ(std::string(""), req.getUsername());
  REQUIRE_EQ(std::string(""), req.getPassword());
  REQUIRE(!req.isIPv6LiteralAddress());
}

void RequestTest::testSetUri2()
{
  Request req;
  bool v = req.setUri("http://aria.rednoah.com:8080/index.html");
  req.setReferer("http://aria.rednoah.com:8080");

  REQUIRE(v);

  // referer is unchanged
  REQUIRE_EQ(std::string("http://aria.rednoah.com:8080"),
                       req.getReferer());
  REQUIRE_EQ(std::string("http"), req.getProtocol());
  REQUIRE_EQ((uint16_t)8080, req.getPort());
  REQUIRE_EQ(std::string("aria.rednoah.com"), req.getHost());
  REQUIRE_EQ(std::string("/"), req.getDir());
  REQUIRE_EQ(std::string("index.html"), req.getFile());
  REQUIRE_EQ(std::string(""), req.getQuery());
}

void RequestTest::testSetUri7()
{
  Request req;
  bool v = req.setUri("http://");

  REQUIRE(!v);
}

void RequestTest::testRedirectUri()
{
  Request req;
  req.supportsPersistentConnection(false);
  req.setUri("http://aria.rednoah.com:8080/aria2/index.html");

  // See port number is preserved.
  REQUIRE(req.redirectUri("/foo"));
  REQUIRE_EQ(std::string("http://aria.rednoah.com:8080/foo"),
                       req.getCurrentUri());
  REQUIRE_EQ(1, req.getRedirectCount());

  REQUIRE(req.redirectUri("http://aria.rednoah.co.jp/"));
  // persistent connection flag is set to be true after redirection
  REQUIRE(req.supportsPersistentConnection());
  // uri must be the same
  REQUIRE_EQ(std::string("http://aria.rednoah.com:8080/aria2/"
                                   "index.html"),
                       req.getUri());
  // currentUri must be updated
  REQUIRE_EQ(std::string("http://aria.rednoah.co.jp/"),
                       req.getCurrentUri());
  REQUIRE_EQ(std::string("http"), req.getProtocol());
  REQUIRE_EQ(std::string("aria.rednoah.co.jp"), req.getHost());
  REQUIRE_EQ((uint16_t)80, req.getPort());
  REQUIRE_EQ(std::string("/"), req.getDir());
  REQUIRE_EQ(std::string(""), req.getFile());
  REQUIRE_EQ(std::string(""), req.getQuery());
  // See redirect count is incremented.
  REQUIRE_EQ(2, req.getRedirectCount());

  // Give absolute path
  REQUIRE(req.redirectUri("/abspath/to/file"));
  REQUIRE_EQ(std::string("http://aria.rednoah.co.jp/abspath/to/file"),
                       req.getCurrentUri());
  REQUIRE_EQ(3, req.getRedirectCount());

  // Give relative path
  REQUIRE(req.redirectUri("relativepath/to/file"));
  REQUIRE_EQ(std::string("http://aria.rednoah.co.jp/abspath/to/"
                                   "relativepath/to/file"),
                       req.getCurrentUri());
  REQUIRE_EQ(4, req.getRedirectCount());

  // Give network-path reference
  REQUIRE(req.redirectUri("//host/to/file"));
  REQUIRE_EQ(std::string("http://host/to/file"), req.getCurrentUri());
  REQUIRE_EQ(5, req.getRedirectCount());

  // http:// in query part
  REQUIRE(req.redirectUri("/abspath?uri=http://foo"));
  REQUIRE_EQ(std::string("http://host/abspath?uri=http://foo"),
                       req.getCurrentUri());
  REQUIRE_EQ(6, req.getRedirectCount());
}

void RequestTest::testRedirectUri2()
{
  Request req;
  req.setUri("http://aria.rednoah.com/download.html");
  req.setReferer("http://aria.rednoah.com/");
  req.redirectUri("http://aria.rednoah.com/403.html");

  // referer must not be changed in redirection
  REQUIRE_EQ(std::string("http://aria.rednoah.com/"),
                       req.getReferer());
}

void RequestTest::testResetUri()
{
  Request req;
  req.setUri("http://aria.rednoah.com:8080/aria2/index.html");
  req.setReferer("http://aria.rednoah.com:8080/");
  req.redirectUri("ftp://aria.rednoah.co.jp/index_en.html?view=wide");

  bool v3 = req.resetUri();
  REQUIRE(v3);
  // currentUri must equal to uri
  REQUIRE_EQ(
      std::string("http://aria.rednoah.com:8080/aria2/index.html"),
      req.getUri());
  REQUIRE_EQ(req.getUri(), req.getCurrentUri());
  // referer is unchanged
  REQUIRE_EQ(std::string("http://aria.rednoah.com:8080/"),
                       req.getReferer());
  REQUIRE_EQ(std::string("http"), req.getProtocol());
  REQUIRE_EQ((uint16_t)8080, req.getPort());
  REQUIRE_EQ(std::string("aria.rednoah.com"), req.getHost());
  REQUIRE_EQ(std::string("/aria2/"), req.getDir());
  REQUIRE_EQ(std::string("index.html"), req.getFile());
  REQUIRE_EQ(std::string(""), req.getQuery());
}

void RequestTest::testInnerLink()
{
  Request req;
  bool v = req.setUri("http://aria.rednoah.com/index.html#download");
  REQUIRE(v);
  REQUIRE_EQ(std::string("http://aria.rednoah.com/index.html"
                                   "#download"),
                       req.getUri());
  REQUIRE_EQ(std::string("http://aria.rednoah.com/index.html"),
                       req.getCurrentUri());
  REQUIRE_EQ(std::string("index.html"), req.getFile());
  REQUIRE_EQ(std::string(""), req.getQuery());
}

void RequestTest::testInnerLinkInReferer()
{
  Request req;
  req.setReferer("http://aria.rednoah.com/home.html#top");
  REQUIRE_EQ(std::string("http://aria.rednoah.com/home.html"),
                       req.getReferer());
}

void RequestTest::testSetUri_supportsPersistentConnection()
{
  Request req;
  REQUIRE(req.setUri("http://host/file"));
  req.supportsPersistentConnection(false);
  REQUIRE(!req.supportsPersistentConnection());
  req.setUri("http://host/file");
  REQUIRE(req.supportsPersistentConnection());
}

void RequestTest::testResetUri_supportsPersistentConnection()
{
  Request req;
  REQUIRE(req.setUri("http://host/file"));
  req.supportsPersistentConnection(false);
  REQUIRE(!req.supportsPersistentConnection());
  req.resetUri();
  REQUIRE(req.supportsPersistentConnection());
}

void RequestTest::testRedirectUri_supportsPersistentConnection()
{
  Request req;
  REQUIRE(req.setUri("http://host/file"));
  req.supportsPersistentConnection(false);
  REQUIRE(!req.supportsPersistentConnection());
  req.redirectUri("http://host/file");
  REQUIRE(req.supportsPersistentConnection());
}

void RequestTest::testRedirectUri_uriNormalization()
{
  Request req;
  REQUIRE(req.setUri("http://host/file?a"));

  REQUIRE(req.redirectUri("/redir1"));
  REQUIRE_EQ(std::string("http://host/redir1"), req.getCurrentUri());

  REQUIRE(req.redirectUri("/redir2?b"));
  REQUIRE_EQ(std::string("http://host/redir2?b"),
                       req.getCurrentUri());

  REQUIRE(req.redirectUri("/redir3?c#d"));
  REQUIRE_EQ(std::string("http://host/redir3?c"),
                       req.getCurrentUri());

  REQUIRE(req.redirectUri("/redir4/gone/.././2nd/foo?a"));
  REQUIRE_EQ(std::string("http://host/redir4/2nd/foo?a"),
                       req.getCurrentUri());

  REQUIRE(req.redirectUri("../new2nd/bar?b"));
  REQUIRE_EQ(std::string("http://host/redir4/new2nd/bar?b"),
                       req.getCurrentUri());
}

void RequestTest::testGetURIHost()
{
  Request req;
  REQUIRE(req.setUri("http://[::1]"));
  REQUIRE_EQ(std::string("[::1]"), req.getURIHost());
}

} // namespace aria2
