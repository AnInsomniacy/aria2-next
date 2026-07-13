#include "HttpHeaderProcessor.h"

#include <iostream>

#include "a2doctest.h"

#include "HttpHeader.h"
#include "DlRetryEx.h"
#include "DlAbortEx.h"

namespace aria2 {

class HttpHeaderProcessorTest {


public:
  void testParse1();
  void testParse2();
  void testParse3();
  void testGetLastBytesProcessed();
  void testGetLastBytesProcessed_nullChar();
  void testGetHttpResponseHeader();
  void testGetHttpResponseHeader_statusOnly();
  void testGetHttpResponseHeader_ignoresEmptyFieldWithoutColon();
  void testGetHttpResponseHeader_insufficientStatusLength();
  void testGetHttpResponseHeader_nameStartsWs();
  void testGetHttpResponseHeader_teAndCl();
  void testBeyondLimit();
  void testGetHeaderString();
  void testGetHttpRequestHeader();
};

A2_TEST(HttpHeaderProcessorTest, testParse1)
A2_TEST(HttpHeaderProcessorTest, testParse2)
A2_TEST(HttpHeaderProcessorTest, testParse3)
A2_TEST(HttpHeaderProcessorTest, testGetLastBytesProcessed)
A2_TEST(HttpHeaderProcessorTest, testGetLastBytesProcessed_nullChar)
A2_TEST(HttpHeaderProcessorTest, testGetHttpResponseHeader)
A2_TEST(HttpHeaderProcessorTest, testGetHttpResponseHeader_statusOnly)
A2_TEST(HttpHeaderProcessorTest, testGetHttpResponseHeader_ignoresEmptyFieldWithoutColon)
A2_TEST(HttpHeaderProcessorTest, testGetHttpResponseHeader_insufficientStatusLength)
A2_TEST(HttpHeaderProcessorTest, testGetHttpResponseHeader_nameStartsWs)
A2_TEST(HttpHeaderProcessorTest, testGetHttpResponseHeader_teAndCl)
A2_TEST(HttpHeaderProcessorTest, testBeyondLimit)
A2_TEST(HttpHeaderProcessorTest, testGetHeaderString)
A2_TEST(HttpHeaderProcessorTest, testGetHttpRequestHeader)

void HttpHeaderProcessorTest::testParse1()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);
  std::string hd1 = "HTTP/1.1 200 OK\r\n";
  REQUIRE(!proc.parse(hd1));
  REQUIRE(proc.parse("\r\n"));
}

void HttpHeaderProcessorTest::testParse2()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);
  std::string hd1 = "HTTP/1.1 200 OK\n";
  REQUIRE(!proc.parse(hd1));
  REQUIRE(proc.parse("\n"));
}

void HttpHeaderProcessorTest::testParse3()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::SERVER_PARSER);
  std::string s = "GET / HTTP/1.1\r\n"
                  "Host: aria2.sourceforge.net\r\n"
                  "Connection: close \r\n"     // trailing white space (BWS)
                  "Accept-Encoding: text1\r\n" // Multi-line header
                  "  text2\r\n"
                  "  text3\r\n"
                  "Authorization: foo\r\n"
                  "Authorization: bar\r\n"
                  "Content-Type:\r\n"
                  "\r\n";
  REQUIRE(proc.parse(s));
  auto h = proc.getResult();
  REQUIRE_EQ(std::string("close"), h->find(HttpHeader::CONNECTION));
  REQUIRE_EQ(std::string("text1 text2 text3"),
                       h->find(HttpHeader::ACCEPT_ENCODING));
  REQUIRE_EQ(std::string("foo"),
                       h->findAll(HttpHeader::AUTHORIZATION)[0]);
  REQUIRE_EQ(std::string("bar"),
                       h->findAll(HttpHeader::AUTHORIZATION)[1]);
  REQUIRE_EQ(std::string(""), h->find(HttpHeader::CONTENT_TYPE));
  REQUIRE(h->defined(HttpHeader::CONTENT_TYPE));
}

void HttpHeaderProcessorTest::testGetLastBytesProcessed()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);
  std::string hd1 = "HTTP/1.1 200 OK\r\n"
                    "\r\nputbackme";
  REQUIRE(proc.parse(hd1));
  REQUIRE_EQ((size_t)19, proc.getLastBytesProcessed());

  proc.clear();

  std::string hd2 = "HTTP/1.1 200 OK\n"
                    "\nputbackme";
  REQUIRE(proc.parse(hd2));
  REQUIRE_EQ((size_t)17, proc.getLastBytesProcessed());
}

void HttpHeaderProcessorTest::testGetLastBytesProcessed_nullChar()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);
  const char x[] = "HTTP/1.1 200 OK\r\n"
                   "foo: foo\0bar\r\n"
                   "\r\nputbackme";
  std::string hd1(&x[0], &x[sizeof(x) - 1]);
  REQUIRE(proc.parse(hd1));
  REQUIRE_EQ((size_t)33, proc.getLastBytesProcessed());
}

void HttpHeaderProcessorTest::testGetHttpResponseHeader()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);
  std::string hd = "HTTP/1.1 404 Not Found\r\n"
                   "Date: Mon, 25 Jun 2007 16:04:59 GMT\r\n"
                   "Server: Apache/2.2.3 (Debian)\r\n"
                   "Last-Modified: Tue, 12 Jun 2007 14:28:43 GMT\r\n"
                   "ETag: \"594065-23e3-50825cc0\"\r\n"
                   "Accept-Ranges: bytes\r\n"
                   "Content-Length: 9187\r\n"
                   "Connection: close\r\n"
                   "Content-Type: text/html; charset=UTF-8\r\n"
                   "\r\n"
                   "Content-Encoding: body";

  REQUIRE(proc.parse(hd));

  auto header = proc.getResult();
  REQUIRE_EQ(404, header->getStatusCode());
  REQUIRE_EQ(std::string("Not Found"), header->getReasonPhrase());
  REQUIRE_EQ(std::string("HTTP/1.1"), header->getVersion());
  REQUIRE_EQ(std::string("9187"),
                       header->find(HttpHeader::CONTENT_LENGTH));
  REQUIRE_EQ(std::string("text/html; charset=UTF-8"),
                       header->find(HttpHeader::CONTENT_TYPE));
  REQUIRE(!header->defined(HttpHeader::CONTENT_ENCODING));
}

void HttpHeaderProcessorTest::testGetHttpResponseHeader_statusOnly()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);

  std::string hd = "HTTP/1.1 200\r\n\r\n";
  REQUIRE(proc.parse(hd));
  auto header = proc.getResult();
  REQUIRE_EQ(200, header->getStatusCode());
}

void HttpHeaderProcessorTest::testGetHttpResponseHeader_ignoresEmptyFieldWithoutColon()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);

  std::string hd = "HTTP/1.1 200 OK\r\n"
                   "X-Empty-Header\r\n"
                   "Content-Length: 10\r\n"
                   "\r\n";
  REQUIRE(proc.parse(hd));

  auto header = proc.getResult();
  REQUIRE_EQ(200, header->getStatusCode());
  REQUIRE_EQ(std::string("10"),
                       header->find(HttpHeader::CONTENT_LENGTH));
}

void HttpHeaderProcessorTest::
    testGetHttpResponseHeader_insufficientStatusLength()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);

  std::string hd = "HTTP/1.1 20\r\n\r\n";
  try {
    proc.parse(hd);
    FAIL("Exception must be thrown.");
  }
  catch (DlAbortEx& ex) {
    // Success
  }
}

void HttpHeaderProcessorTest::testGetHttpResponseHeader_nameStartsWs()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);

  std::string hd = "HTTP/1.1 200\r\n"
                   " foo:bar\r\n"
                   "\r\n";
  try {
    proc.parse(hd);
    FAIL("Exception must be thrown.");
  }
  catch (DlAbortEx& ex) {
    // Success
  }

  proc.clear();
  hd = "HTTP/1.1 200\r\n"
       ":foo:bar\r\n"
       "\r\n";
  try {
    proc.parse(hd);
    FAIL("Exception must be thrown.");
  }
  catch (DlAbortEx& ex) {
    // Success
  }

  proc.clear();
  hd = "HTTP/1.1 200\r\n"
       ":foo\r\n"
       "\r\n";
  try {
    proc.parse(hd);
    FAIL("Exception must be thrown.");
  }
  catch (DlAbortEx& ex) {
    // Success
  }
}

void HttpHeaderProcessorTest::testGetHttpResponseHeader_teAndCl()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);

  std::string hd = "HTTP/1.1 200\r\n"
                   "Content-Length: 200\r\n"
                   "Transfer-Encoding: chunked\r\n"
                   "Content-Range: 1-200/300\r\n"
                   "\r\n";

  REQUIRE(proc.parse(hd));

  auto httpHeader = proc.getResult();
  REQUIRE_EQ(std::string("chunked"),
                       httpHeader->find(HttpHeader::TRANSFER_ENCODING));
  REQUIRE(!httpHeader->defined(HttpHeader::CONTENT_LENGTH));
  REQUIRE(!httpHeader->defined(HttpHeader::CONTENT_RANGE));
}

void HttpHeaderProcessorTest::testBeyondLimit()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);

  std::string hd1 = "HTTP/1.1 200 OK\r\n";
  std::string hd2 = std::string(1025, 'A');

  proc.parse(hd1);
  try {
    proc.parse(hd2);
    FAIL("Exception must be thrown.");
  }
  catch (DlAbortEx& ex) {
    // Success
  }
}

void HttpHeaderProcessorTest::testGetHeaderString()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::CLIENT_PARSER);
  std::string hd = "HTTP/1.1 200 OK\r\n"
                   "Date: Mon, 25 Jun 2007 16:04:59 GMT\r\n"
                   "Server: Apache/2.2.3 (Debian)\r\n"
                   "Last-Modified: Tue, 12 Jun 2007 14:28:43 GMT\r\n"
                   "ETag: \"594065-23e3-50825cc0\"\r\n"
                   "Accept-Ranges: bytes\r\n"
                   "Content-Length: 9187\r\n"
                   "Connection: close\r\n"
                   "Content-Type: text/html; charset=UTF-8\r\n"
                   "\r\nputbackme";

  REQUIRE(proc.parse(hd));

  REQUIRE_EQ(
      std::string("HTTP/1.1 200 OK\r\n"
                  "Date: Mon, 25 Jun 2007 16:04:59 GMT\r\n"
                  "Server: Apache/2.2.3 (Debian)\r\n"
                  "Last-Modified: Tue, 12 Jun 2007 14:28:43 GMT\r\n"
                  "ETag: \"594065-23e3-50825cc0\"\r\n"
                  "Accept-Ranges: bytes\r\n"
                  "Content-Length: 9187\r\n"
                  "Connection: close\r\n"
                  "Content-Type: text/html; charset=UTF-8\r\n"
                  "\r\n"),
      proc.getHeaderString());
}

void HttpHeaderProcessorTest::testGetHttpRequestHeader()
{
  HttpHeaderProcessor proc(HttpHeaderProcessor::SERVER_PARSER);
  std::string request = "GET /index.html HTTP/1.1\r\n"
                        "Host: host\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "Content-Encoding: body";

  REQUIRE(proc.parse(request));

  auto httpHeader = proc.getResult();
  REQUIRE_EQ(std::string("GET"), httpHeader->getMethod());
  REQUIRE_EQ(std::string("/index.html"),
                       httpHeader->getRequestPath());
  REQUIRE_EQ(std::string("HTTP/1.1"), httpHeader->getVersion());
  REQUIRE_EQ(std::string("close"),
                       httpHeader->find(HttpHeader::CONNECTION));
  REQUIRE(!httpHeader->defined(HttpHeader::CONTENT_ENCODING));
}

} // namespace aria2
