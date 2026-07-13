#include "HttpHeader.h"

#include "a2doctest.h"

#include "a2io.h"
#include "Range.h"
#include "DlAbortEx.h"

namespace aria2 {

class HttpHeaderTest {


public:
  void testGetRange();
  void testGetRangeAcceptsLargeContentLength();
  void testFindAll();
  void testClearField();
  void testFieldContains();
  void testRemove();
};

A2_TEST(HttpHeaderTest, testGetRange)
A2_TEST(HttpHeaderTest, testGetRangeAcceptsLargeContentLength)
A2_TEST(HttpHeaderTest, testFindAll)
A2_TEST(HttpHeaderTest, testClearField)
A2_TEST(HttpHeaderTest, testFieldContains)
A2_TEST(HttpHeaderTest, testRemove)

static_assert(sizeof(a2_off_t) >= 8, "a2_off_t must support large files");

void HttpHeaderTest::testGetRange()
{
  {
    HttpHeader httpHeader;
    httpHeader.put(
        HttpHeader::CONTENT_RANGE,
        "9223372036854775800-9223372036854775801/9223372036854775807");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)9223372036854775800LL, range.startByte);
    REQUIRE_EQ((int64_t)9223372036854775801LL, range.endByte);
    REQUIRE_EQ((int64_t)9223372036854775807LL, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(
        HttpHeader::CONTENT_RANGE,
        "9223372036854775800-9223372036854775801/9223372036854775807");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)9223372036854775800LL, range.startByte);
    REQUIRE_EQ((int64_t)9223372036854775801LL, range.endByte);
    REQUIRE_EQ((int64_t)9223372036854775807LL, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes */1024");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)0, range.startByte);
    REQUIRE_EQ((int64_t)0, range.endByte);
    REQUIRE_EQ((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0-9/*");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)0, range.startByte);
    REQUIRE_EQ((int64_t)0, range.endByte);
    REQUIRE_EQ((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes */*");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)0, range.startByte);
    REQUIRE_EQ((int64_t)0, range.endByte);
    REQUIRE_EQ((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)0, range.startByte);
    REQUIRE_EQ((int64_t)0, range.endByte);
    REQUIRE_EQ((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0/");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)0, range.startByte);
    REQUIRE_EQ((int64_t)0, range.endByte);
    REQUIRE_EQ((int64_t)0, range.entityLength);
  }
  {
    // Support for non-compliant server
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes=0-1023/1024");

    Range range = httpHeader.getRange();

    REQUIRE_EQ((int64_t)0, range.startByte);
    REQUIRE_EQ((int64_t)1023, range.endByte);
    REQUIRE_EQ((int64_t)1024, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes 0-/3");
    try {
      httpHeader.getRange();
      FAIL("Exception must be thrown");
    }
    catch (const DlAbortEx& e) {
      // success
    }
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::CONTENT_RANGE, "bytes -0/3");
    try {
      httpHeader.getRange();
      FAIL("Exception must be thrown");
    }
    catch (const DlAbortEx& e) {
      // success
    }
  }
}

void HttpHeaderTest::testGetRangeAcceptsLargeContentLength()
{
  HttpHeader httpHeader;
  httpHeader.put(HttpHeader::CONTENT_LENGTH, "6797948928");

  Range range = httpHeader.getRange();

  REQUIRE_EQ((int64_t)0, range.startByte);
  REQUIRE_EQ((int64_t)6797948927LL, range.endByte);
  REQUIRE_EQ((int64_t)6797948928LL, range.entityLength);
}

void HttpHeaderTest::testFindAll()
{
  HttpHeader h;
  h.put(HttpHeader::LINK, "100");
  h.put(HttpHeader::LINK, "101");
  h.put(HttpHeader::CONNECTION, "200");

  std::vector<std::string> r(h.findAll(HttpHeader::LINK));
  REQUIRE_EQ((size_t)2, r.size());
  REQUIRE_EQ(std::string("100"), r[0]);
  REQUIRE_EQ(std::string("101"), r[1]);
}

void HttpHeaderTest::testClearField()
{
  HttpHeader h;
  h.setStatusCode(200);
  h.setVersion("HTTP/1.1");
  h.put(HttpHeader::LINK, "Bar");

  REQUIRE_EQ(std::string("Bar"), h.find(HttpHeader::LINK));

  h.clearField();

  REQUIRE_EQ(std::string(""), h.find(HttpHeader::LINK));
  REQUIRE_EQ(200, h.getStatusCode());
  REQUIRE_EQ(std::string("HTTP/1.1"), h.getVersion());
}

void HttpHeaderTest::testFieldContains()
{
  HttpHeader h;
  h.put(HttpHeader::CONNECTION, "Keep-Alive, Upgrade");
  h.put(HttpHeader::UPGRADE, "WebSocket");
  h.put(HttpHeader::SEC_WEBSOCKET_VERSION, "13");
  h.put(HttpHeader::SEC_WEBSOCKET_VERSION, "8, 7");
  REQUIRE(h.fieldContains(HttpHeader::CONNECTION, "upgrade"));
  REQUIRE(h.fieldContains(HttpHeader::CONNECTION, "keep-alive"));
  REQUIRE(!h.fieldContains(HttpHeader::CONNECTION, "close"));
  REQUIRE(h.fieldContains(HttpHeader::UPGRADE, "websocket"));
  REQUIRE(!h.fieldContains(HttpHeader::UPGRADE, "spdy"));
  REQUIRE(h.fieldContains(HttpHeader::SEC_WEBSOCKET_VERSION, "13"));
  REQUIRE(h.fieldContains(HttpHeader::SEC_WEBSOCKET_VERSION, "8"));
  REQUIRE(!h.fieldContains(HttpHeader::SEC_WEBSOCKET_VERSION, "6"));
}

void HttpHeaderTest::testRemove()
{
  HttpHeader h;
  h.put(HttpHeader::CONNECTION, "close");
  h.put(HttpHeader::TRANSFER_ENCODING, "chunked");
  h.put(HttpHeader::TRANSFER_ENCODING, "gzip");

  h.remove(HttpHeader::TRANSFER_ENCODING);

  REQUIRE(!h.defined(HttpHeader::TRANSFER_ENCODING));
  REQUIRE(h.defined(HttpHeader::CONNECTION));
}

} // namespace aria2
