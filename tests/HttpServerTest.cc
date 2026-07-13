#include "HttpServer.h"

#include "a2doctest.h"

#include "SocketCore.h"
#include "a2functional.h"

namespace aria2 {

class HttpServerTest {

public:
  void testHttpBasicAuth();
};

A2_TEST(HttpServerTest, testHttpBasicAuth)

namespace {
std::unique_ptr<HttpServer> performHttpRequest(SocketCore& server,
                                               std::string request)
{
  auto endpoint = server.getAddrInfo();

  SocketCore client;
  client.establishConnection("localhost", endpoint.port);
  while (!client.isWritable(0)) {
  }

  auto inbound = server.acceptConnection();
  inbound->setBlockingMode();
  auto rv = make_unique<HttpServer>(inbound);

  client.writeData(request);
  while (!rv->receiveRequest()) {
  }
  return rv;
}
} // namespace

void HttpServerTest::testHttpBasicAuth()
{
  SocketCore server;
  server.bind(0);
  server.beginListen();
  server.setBlockingMode();

  {
    // Default is no auth
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    REQUIRE(req->authenticate());
  }

  {
    // Empty user-name and password should come out as no auth.
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    req->setUsernamePassword("", "");
    REQUIRE(req->authenticate());
  }

  {
    // Empty user-name but set password should also come out as no auth.
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    req->setUsernamePassword("", "pass");
    REQUIRE(req->authenticate());
  }

  {
    // Client provided credentials should be ignored when there is no auth.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("", "pass");
    REQUIRE(req->authenticate());
  }

  {
    // Correct client provided credentials should match.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user", "pass");
    REQUIRE(req->authenticate());
  }

  {
    // Correct client provided credentials should match (2).
    // Embedded nulls
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\nAuthorization: "
                "Basic dXNlcgBudWxsOnBhc3MAbnVsbA==\r\n\r\n");
    req->setUsernamePassword(std::string("user\0null", 9),
                             std::string("pass\0null", 9));
    REQUIRE(req->authenticate());
  }

  {
    // Correct client provided credentials should match (3).
    // Embedded, leading nulls
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "AHVzZXI6AHBhc3M=\r\n\r\n");
    req->setUsernamePassword(std::string("\0user", 5),
                             std::string("\0pass", 5));
    REQUIRE(req->authenticate());
  }

  {
    // Correct client provided credentials should match (3).
    // Whitespace
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "IHVzZXIJOgpwYXNzDQ==\r\n\r\n");
    req->setUsernamePassword(" user\t", "\npass\r");
    REQUIRE(req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user", "pass2");
    REQUIRE(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (2).
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user2", "pass");
    REQUIRE(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (3).
    // Embedded null in pass config.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword("user", std::string("pass\0three", 10));
    REQUIRE(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (4).
    // Embedded null in user config.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNz\r\n\r\n");
    req->setUsernamePassword(std::string("user\0four", 9), "pass");
    REQUIRE(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (5).
    // Embedded null in http auth.
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "dXNlcjpwYXNzAHRocmVl\r\n\r\n");
    req->setUsernamePassword("user", "pass");
    REQUIRE(!req->authenticate());
  }

  {
    // Wrong client provided credentials should NOT match (6).
    // Embedded null in http auth.
    // Embedded, leading nulls
    auto req = performHttpRequest(server, "GET / HTTP/1.1\r\nUser-Agent: "
                                          "aria2-test\r\nAuthorization: Basic "
                                          "AHVzZXI6AHBhc3M=\r\n\r\n");
    req->setUsernamePassword(std::string("\0user5", 6),
                             std::string("\0pass", 5));
    REQUIRE(!req->authenticate());
  }

  {
    // When there is a user and password, the client must actually provide auth.
    auto req = performHttpRequest(
        server, "GET / HTTP/1.1\r\nUser-Agent: aria2-test\r\n\r\n");
    req->setUsernamePassword("user", "pass");
    REQUIRE(!req->authenticate());
  }
}

} // namespace aria2
