#include "ChunkedDecodingStreamFilter.h"

#include <cstdlib>
#include <iostream>
#include "a2doctest.h"

#include "DlAbortEx.h"
#include "Segment.h"
#include "ByteArrayDiskWriter.h"
#include "SinkStreamFilter.h"
#include "MockSegment.h"
#include "a2functional.h"

namespace aria2 {

namespace {
const unsigned char* asBytes(const char* s)
{
  return reinterpret_cast<const unsigned char*>(s);
}

const unsigned char* asBytes(const std::string& s)
{
  return asBytes(s.data());
}
} // namespace

class ChunkedDecodingStreamFilterTest {


  std::unique_ptr<ChunkedDecodingStreamFilter> filter_;
  std::shared_ptr<ByteArrayDiskWriter> writer_;
  std::shared_ptr<Segment> segment_;

  void clearWriter() { writer_->setString(""); }

public:
  void setUp()
  {
    writer_ = std::make_shared<ByteArrayDiskWriter>();
    auto sinkFilter = make_unique<SinkStreamFilter>();
    sinkFilter->init();
    filter_ = make_unique<ChunkedDecodingStreamFilter>(std::move(sinkFilter));
    filter_->init();
    segment_ = std::make_shared<MockSegment>();
  }

  void testTransform();
  void testTransform_withoutTrailer();
  void testTransform_with2Trailers();
  void testTransform_largeChunkSize();
  void testTransform_tooLargeChunkSize();
  void testTransform_chunkSizeMismatch();
  void testGetName();
};

A2_TEST(ChunkedDecodingStreamFilterTest, testTransform)
A2_TEST(ChunkedDecodingStreamFilterTest, testTransform_withoutTrailer)
A2_TEST(ChunkedDecodingStreamFilterTest, testTransform_with2Trailers)
A2_TEST(ChunkedDecodingStreamFilterTest, testTransform_largeChunkSize)
A2_TEST(ChunkedDecodingStreamFilterTest, testTransform_tooLargeChunkSize)
A2_TEST(ChunkedDecodingStreamFilterTest, testTransform_chunkSizeMismatch)
A2_TEST(ChunkedDecodingStreamFilterTest, testGetName)

void ChunkedDecodingStreamFilterTest::testTransform()
{
  try {
    std::string msg = "a\r\n1234567890\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)10, r);
    REQUIRE_EQ(std::string("1234567890"), writer_->getString());
    REQUIRE_EQ((size_t)15, filter_->getBytesProcessed());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  clearWriter();
  try {
    // Feed extension; see it is ignored.
    std::string msg = "3;extensionIgnored\r\n123\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)3, r);
    REQUIRE_EQ(std::string("123"), writer_->getString());
    REQUIRE_EQ((size_t)25, filter_->getBytesProcessed());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }

  clearWriter();
  // Feed 2extensions; see it is ignored.
  try {
    std::string msg = "3;extension1;extension2;\r\n123\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)3, r);
    REQUIRE_EQ(std::string("123"), writer_->getString());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  clearWriter();
  // Not all chunk size is available
  try {
    std::string msg = "1";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)0, r);
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  clearWriter();
  try {
    std::string msg = "0\r\n1234567890123456\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)16, r);
    REQUIRE_EQ(std::string("1234567890123456"), writer_->getString());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  clearWriter();
  // Not all chunk data is available
  try {
    std::string msg = "10\r\n1234567890";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)10, r);
    REQUIRE_EQ(std::string("1234567890"), writer_->getString());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  clearWriter();
  try {
    std::string msg = "123456\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)6, r);
    REQUIRE_EQ(std::string("123456"), writer_->getString());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  clearWriter();
  // no trailing CR LF.
  try {
    std::string msg = "10\r\n1234567890123456";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)16, r);
    REQUIRE_EQ(std::string("1234567890123456"), writer_->getString());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  clearWriter();
  // feed only CR
  try {
    std::string msg = "\r";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)0, r);
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  // feed next LF
  try {
    std::string msg = "\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)0, r);
    REQUIRE_EQ(std::string(""), writer_->getString());
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  // feed 0 CR LF.
  try {
    std::string msg = "0\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)0, r);
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  // feed trailer
  try {
    std::string msg = "trailer\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)0, r);
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  // feed final CRLF
  try {
    std::string msg = "\r\n";
    ssize_t r = filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    REQUIRE_EQ((ssize_t)0, r);
  }
  catch (DlAbortEx& e) {
    FAIL(e.stackTrace());
  }
  // input is over
  REQUIRE(filter_->finished());
}

void ChunkedDecodingStreamFilterTest::testTransform_withoutTrailer()
{
  REQUIRE_EQ(
      (ssize_t)0, filter_->transform(writer_, segment_, asBytes("0\r\n\r\n"), 5));
  REQUIRE(filter_->finished());
}

void ChunkedDecodingStreamFilterTest::testTransform_with2Trailers()
{
  REQUIRE_EQ(
      (ssize_t)0,
      filter_->transform(writer_, segment_, asBytes("0\r\nt1\r\nt2\r\n\r\n"), 13));
  REQUIRE(filter_->finished());
}

void ChunkedDecodingStreamFilterTest::testTransform_largeChunkSize()
{
  // chunkSize should be under 2^63-1
  {
    std::string msg = "7fffffffffffffff\r\n";
    filter_->transform(writer_, segment_, asBytes(msg), msg.size());
  }
}

void ChunkedDecodingStreamFilterTest::testTransform_tooLargeChunkSize()
{
  // chunkSize 2^64 causes error
  {
    std::string msg = "ffffffffffffffff\r\n";
    try {
      filter_->transform(writer_, segment_, asBytes(msg), msg.size());
      FAIL("exception must be thrown.");
    }
    catch (DlAbortEx& e) {
      // success
    }
  }
}

void ChunkedDecodingStreamFilterTest::testTransform_chunkSizeMismatch()
{
  std::string msg = "3\r\n1234\r\n";
  try {
    filter_->transform(writer_, segment_, asBytes(msg), msg.size());
    FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    // success
  }
}

void ChunkedDecodingStreamFilterTest::testGetName()
{
  REQUIRE_EQ(std::string("ChunkedDecodingStreamFilter"),
                       filter_->getName());
}

} // namespace aria2
