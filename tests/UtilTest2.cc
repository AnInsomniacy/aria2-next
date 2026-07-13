#include "util.h"

#include <cmath>
#include <cstring>
#include <string>
#include <iostream>

#include "a2doctest.h"

#include "FixedNumberRandomizer.h"
#include "DlAbortEx.h"
#include "BitfieldMan.h"
#include "ByteArrayDiskWriter.h"
#include "FileEntry.h"
#include "File.h"
#include "array_fun.h"
#include "BufferedFile.h"
#include "TestUtil.h"
#include "SocketCore.h"

namespace aria2 {

class UtilTest2 {


private:
public:
  void setUp() {}

  void testToUpper();
  void testToLower();
  void testUppercase();
  void testLowercase();
  void testPercentDecode();
  void testGetRealSize();
  void testAbbrevSize();
  void testToStream();
  void testIsNumber();
  void testIsLowercase();
  void testIsUppercase();
  void testMkdirs();
  void testConvertBitfield();
  void testParseIntSegments();
  void testParseIntSegments_invalidRange();
  void testParseIntNoThrow();
  void testParseUIntNoThrow();
  void testParseLLIntNoThrow();
  void testToString_binaryStream();
  void testItos();
  void testUitos();
  void testNtoh64();
  void testPercentEncode();
  void testPercentEncodeMini();
  void testHtmlEscape();
  void testJoinPath();
  void testParseIndexPath();
  void testCreateIndexPaths();
  void testGenerateRandomData();
  void testFromHex();
  void testParsePrioritizePieceRange();
  void testApplyDir();
  void testFixTaintedBasename();
  void testIsNumericHost();
  void testDetectDirTraversal();
  void testEscapePath();
  void testInSameCidrBlock();
  void testIsUtf8String();
  void testNextParam();
  void testNoProxyDomainMatch();
  void testInPrivateAddress();
  void testSecfmt();
  void testTlsHostnameMatch();
  void testParseDoubleNoThrow();
};

A2_TEST(UtilTest2, testToUpper)
A2_TEST(UtilTest2, testToLower)
A2_TEST(UtilTest2, testUppercase)
A2_TEST(UtilTest2, testLowercase)
A2_TEST(UtilTest2, testPercentDecode)
A2_TEST(UtilTest2, testGetRealSize)
A2_TEST(UtilTest2, testAbbrevSize)
A2_TEST(UtilTest2, testToStream)
A2_TEST(UtilTest2, testIsNumber)
A2_TEST(UtilTest2, testIsLowercase)
A2_TEST(UtilTest2, testIsUppercase)
A2_TEST(UtilTest2, testMkdirs)
A2_TEST(UtilTest2, testConvertBitfield)
A2_TEST(UtilTest2, testParseIntSegments)
A2_TEST(UtilTest2, testParseIntSegments_invalidRange)
A2_TEST(UtilTest2, testParseIntNoThrow)
A2_TEST(UtilTest2, testParseUIntNoThrow)
A2_TEST(UtilTest2, testParseLLIntNoThrow)
A2_TEST(UtilTest2, testToString_binaryStream)
A2_TEST(UtilTest2, testItos)
A2_TEST(UtilTest2, testUitos)
A2_TEST(UtilTest2, testNtoh64)
A2_TEST(UtilTest2, testPercentEncode)
A2_TEST(UtilTest2, testPercentEncodeMini)
A2_TEST(UtilTest2, testHtmlEscape)
A2_TEST(UtilTest2, testJoinPath)
A2_TEST(UtilTest2, testParseIndexPath)
A2_TEST(UtilTest2, testCreateIndexPaths)
A2_TEST(UtilTest2, testGenerateRandomData)
A2_TEST(UtilTest2, testFromHex)
A2_TEST(UtilTest2, testParsePrioritizePieceRange)
A2_TEST(UtilTest2, testApplyDir)
A2_TEST(UtilTest2, testFixTaintedBasename)
A2_TEST(UtilTest2, testIsNumericHost)
A2_TEST(UtilTest2, testDetectDirTraversal)
A2_TEST(UtilTest2, testEscapePath)
A2_TEST(UtilTest2, testInSameCidrBlock)
A2_TEST(UtilTest2, testIsUtf8String)
A2_TEST(UtilTest2, testNextParam)
A2_TEST(UtilTest2, testNoProxyDomainMatch)
A2_TEST(UtilTest2, testInPrivateAddress)
A2_TEST(UtilTest2, testSecfmt)
A2_TEST(UtilTest2, testTlsHostnameMatch)
A2_TEST(UtilTest2, testParseDoubleNoThrow)

class Printer {
public:
  template <class T> void operator()(T t) { std::cerr << t << ", "; }
};

void UtilTest2::testToUpper()
{
  std::string src = "608cabc0f2fa18c260cafd974516865c772363d5";
  std::string upp = "608CABC0F2FA18C260CAFD974516865C772363D5";

  REQUIRE_EQ(upp, util::toUpper(src));
}

void UtilTest2::testToLower()
{
  std::string src = "608CABC0F2FA18C260CAFD974516865C772363D5";
  std::string upp = "608cabc0f2fa18c260cafd974516865c772363d5";

  REQUIRE_EQ(upp, util::toLower(src));
}

void UtilTest2::testUppercase()
{
  std::string src = "608cabc0f2fa18c260cafd974516865c772363d5";
  std::string ans = "608CABC0F2FA18C260CAFD974516865C772363D5";
  util::uppercase(src);
  REQUIRE_EQ(ans, src);
}

void UtilTest2::testLowercase()
{
  std::string src = "608CABC0F2FA18C260CAFD974516865C772363D5";
  std::string ans = "608cabc0f2fa18c260cafd974516865c772363d5";
  util::lowercase(src);
  REQUIRE_EQ(ans, src);
}

void UtilTest2::testPercentDecode()
{
  std::string src = "http://aria2.sourceforge.net/aria2%200.7.0%20docs.html";
  REQUIRE_EQ(
      std::string("http://aria2.sourceforge.net/aria2 0.7.0 docs.html"),
      util::percentDecode(src.begin(), src.end()));

  std::string src2 = "aria2+aria2";
  REQUIRE_EQ(std::string("aria2+aria2"),
                       util::percentDecode(src2.begin(), src2.end()));

  std::string src3 = "%5t%20";
  REQUIRE_EQ(std::string("%5t "),
                       util::percentDecode(src3.begin(), src3.end()));

  std::string src4 = "%";
  REQUIRE_EQ(std::string("%"),
                       util::percentDecode(src4.begin(), src4.end()));

  std::string src5 = "%3";
  REQUIRE_EQ(std::string("%3"),
                       util::percentDecode(src5.begin(), src5.end()));

  std::string src6 = "%2f";
  REQUIRE_EQ(std::string("/"),
                       util::percentDecode(src6.begin(), src6.end()));
}

void UtilTest2::testGetRealSize()
{
  REQUIRE_EQ((int64_t)4_g, util::getRealSize("4096M"));
  REQUIRE_EQ((int64_t)1_k, util::getRealSize("1K"));
  REQUIRE_EQ((int64_t)4_g, util::getRealSize("4096m"));
  REQUIRE_EQ((int64_t)1_k, util::getRealSize("1k"));
  REQUIRE_EQ((int64_t)1572864, util::getRealSize("1.5M"));
  REQUIRE_EQ((int64_t)512, util::getRealSize("0.5K"));
  REQUIRE_EQ((int64_t)1, util::getRealSize("1.9"));
  REQUIRE_EQ((int64_t)1364, util::getRealSize("1.333K"));
  REQUIRE_EQ((int64_t)0,
                       util::getRealSize("0.0000000000000000000000001M"));
  try {
    util::getRealSize("");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace();
  }
  try {
    util::getRealSize("foo");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace();
  }
  try {
    util::getRealSize("-1");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace();
  }
  try {
    util::getRealSize("1.2.3K");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace();
  }
  try {
    util::getRealSize("1K2");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace();
  }
  try {
    util::getRealSize("9223372036854775807K");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace();
  }
  try {
    util::getRealSize("9223372036854775807M");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace();
  }
}

void UtilTest2::testAbbrevSize()
{
  REQUIRE_EQ(std::string("8,589,934,591Gi"),
                       util::abbrevSize(9223372036854775807LL));
  REQUIRE_EQ(std::string("4.0Gi"), util::abbrevSize(4_g));
  REQUIRE_EQ(std::string("1.0Ki"), util::abbrevSize(1_k));
  REQUIRE_EQ(std::string("0.9Ki"), util::abbrevSize(1023));
  REQUIRE_EQ(std::string("511"), util::abbrevSize(511));
  REQUIRE_EQ(std::string("0"), util::abbrevSize(0));
  REQUIRE_EQ(std::string("1.1Ki"), util::abbrevSize(1127));
  REQUIRE_EQ(std::string("1.5Mi"), util::abbrevSize(1572864));
}

void UtilTest2::testToStream()
{
  std::ostringstream os;
  std::shared_ptr<FileEntry> f1(new FileEntry("aria2.tar.bz2", 12300, 0));
  std::shared_ptr<FileEntry> f2(new FileEntry("aria2.txt", 556, 0));
  std::deque<std::shared_ptr<FileEntry>> entries;
  entries.push_back(f1);
  entries.push_back(f2);
  const char* filename = A2_TEST_OUT_DIR "/aria2_UtilTest2_testToStream";
  BufferedFile fp(filename, BufferedFile::WRITE);
  util::toStream(entries.begin(), entries.end(), fp);
  fp.close();
  REQUIRE_EQ(std::string("Files:\n"
                                   "idx|path/length\n"
                                   "===+======================================="
                                   "====================================\n"
                                   "  1|aria2.tar.bz2\n"
                                   "   |12KiB (12,300)\n"
                                   "---+---------------------------------------"
                                   "------------------------------------\n"
                                   "  2|aria2.txt\n"
                                   "   |556B (556)\n"
                                   "---+---------------------------------------"
                                   "------------------------------------\n"),
                       readFile(filename));
}

void UtilTest2::testIsNumber()
{
  std::string s = "000";
  REQUIRE_EQ(true, util::isNumber(s.begin(), s.end()));
  s = "a";
  REQUIRE_EQ(false, util::isNumber(s.begin(), s.end()));
  s = "0a";
  REQUIRE_EQ(false, util::isNumber(s.begin(), s.end()));
  s = "";
  REQUIRE_EQ(false, util::isNumber(s.begin(), s.end()));
  s = " ";
  REQUIRE_EQ(false, util::isNumber(s.begin(), s.end()));
}

void UtilTest2::testIsLowercase()
{
  std::string s = "alpha";
  REQUIRE_EQ(true, util::isLowercase(s.begin(), s.end()));
  s = "Alpha";
  REQUIRE_EQ(false, util::isLowercase(s.begin(), s.end()));
  s = "1alpha";
  REQUIRE_EQ(false, util::isLowercase(s.begin(), s.end()));
  s = "";
  REQUIRE_EQ(false, util::isLowercase(s.begin(), s.end()));
  s = " ";
  REQUIRE_EQ(false, util::isLowercase(s.begin(), s.end()));
}

void UtilTest2::testIsUppercase()
{
  std::string s = "ALPHA";
  REQUIRE_EQ(true, util::isUppercase(s.begin(), s.end()));
  s = "Alpha";
  REQUIRE_EQ(false, util::isUppercase(s.begin(), s.end()));
  s = "1ALPHA";
  REQUIRE_EQ(false, util::isUppercase(s.begin(), s.end()));
  s = "";
  REQUIRE_EQ(false, util::isUppercase(s.begin(), s.end()));
  s = " ";
  REQUIRE_EQ(false, util::isUppercase(s.begin(), s.end()));
}

void UtilTest2::testMkdirs()
{
  std::string dir = A2_TEST_OUT_DIR "/aria2-UtilTest2-testMkdirs";
  File d(dir);
  if (d.exists()) {
    REQUIRE(d.remove());
  }
  REQUIRE(!d.exists());
  util::mkdirs(dir);
  REQUIRE(d.isDir());

  std::string file = A2_TEST_DIR "/UtilTest2.cc";
  File f(file);
  REQUIRE(f.isFile());
  try {
    util::mkdirs(file);
    FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& ex) {
    std::cerr << ex.stackTrace() << std::endl;
  }
}

void UtilTest2::testConvertBitfield()
{
  BitfieldMan srcBitfield(384_k, 256_k * 256 + 1);
  BitfieldMan destBitfield(512_k, srcBitfield.getTotalLength());
  srcBitfield.setAllBit();
  srcBitfield.unsetBit(2); // <- range [768, 1152)
  // which corresponds to the index [1,2] in destBitfield
  util::convertBitfield(&destBitfield, &srcBitfield);

  REQUIRE_EQ(std::string("9fffffffffffffffffffffffffffffff80"),
                       util::toHex(destBitfield.getBitfield(),
                                   destBitfield.getBitfieldLength()));
}

void UtilTest2::testParseIntSegments()
{
  {
    auto sgl = util::parseIntSegments("1,3-8,10");

    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(1, sgl.next());
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(3, sgl.next());
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(4, sgl.next());
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(5, sgl.next());
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(6, sgl.next());
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(7, sgl.next());
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(8, sgl.next());
    REQUIRE(sgl.hasNext());
    REQUIRE_EQ(10, sgl.next());
    REQUIRE(!sgl.hasNext());
    REQUIRE_EQ(0, sgl.next());
  }
  {
    auto sgl = util::parseIntSegments(",,,1,,,3,,,");
    REQUIRE_EQ(1, sgl.next());
    REQUIRE_EQ(3, sgl.next());
    REQUIRE(!sgl.hasNext());
  }
}

void UtilTest2::testParseIntSegments_invalidRange()
{
  try {
    auto sgl = util::parseIntSegments("-1");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    auto sgl = util::parseIntSegments("1-");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    auto sgl = util::parseIntSegments("2147483648");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    auto sgl = util::parseIntSegments("2147483647-2147483648");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    auto sgl = util::parseIntSegments("1-2x");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    auto sgl = util::parseIntSegments("3x-4");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
}

void UtilTest2::testParseIntNoThrow()
{
  std::string s;
  int32_t n;
  s = " -1 ";
  REQUIRE(util::parseIntNoThrow(n, s));
  REQUIRE_EQ((int32_t)-1, n);

  s = "2147483647";
  REQUIRE(util::parseIntNoThrow(n, s));
  REQUIRE_EQ((int32_t)2147483647, n);

  s = "2147483648";
  REQUIRE(!util::parseIntNoThrow(n, s));
  s = "-2147483649";
  REQUIRE(!util::parseIntNoThrow(n, s));

  s = "12x";
  REQUIRE(!util::parseIntNoThrow(n, s));
  s = "";
  REQUIRE(!util::parseIntNoThrow(n, s));
}

void UtilTest2::testParseUIntNoThrow()
{
  std::string s;
  uint32_t n;
  s = " 2147483647 ";
  REQUIRE(util::parseUIntNoThrow(n, s));
  REQUIRE_EQ((uint32_t)INT32_MAX, n);
  s = "2147483648";
  REQUIRE(!util::parseUIntNoThrow(n, s));
  s = "-1";
  REQUIRE(!util::parseUIntNoThrow(n, s));
}

void UtilTest2::testParseLLIntNoThrow()
{
  std::string s;
  int64_t n;
  s = " 9223372036854775807 ";
  REQUIRE(util::parseLLIntNoThrow(n, s));
  REQUIRE_EQ((int64_t)INT64_MAX, n);
  s = "9223372036854775808";
  REQUIRE(!util::parseLLIntNoThrow(n, s));
  s = "-9223372036854775808";
  REQUIRE(util::parseLLIntNoThrow(n, s));
  REQUIRE_EQ((int64_t)INT64_MIN, n);
  s = "-9223372036854775809";
  REQUIRE(!util::parseLLIntNoThrow(n, s));
}

void UtilTest2::testToString_binaryStream()
{
  std::shared_ptr<DiskWriter> dw(new ByteArrayDiskWriter());
  std::string data(16_k + 256, 'a');
  dw->initAndOpenFile();
  dw->writeData((const unsigned char*)data.c_str(), data.size(), 0);

  std::string readData = util::toString(dw);

  REQUIRE_EQ(data, readData);
}

void UtilTest2::testItos()
{
  {
    int i = 0;
    REQUIRE_EQ(std::string("0"), util::itos(i));
  }
  {
    int i = 100;
    REQUIRE_EQ(std::string("100"), util::itos(i, true));
  }
  {
    int i = 100;
    REQUIRE_EQ(std::string("100"), util::itos(i));
  }
  {
    int i = 12345;
    REQUIRE_EQ(std::string("12,345"), util::itos(i, true));
  }
  {
    int i = 12345;
    REQUIRE_EQ(std::string("12345"), util::itos(i));
  }
  {
    int i = -12345;
    REQUIRE_EQ(std::string("-12,345"), util::itos(i, true));
  }
  {
    int64_t i = INT64_MAX;
    REQUIRE_EQ(std::string("9,223,372,036,854,775,807"),
                         util::itos(i, true));
  }
  {
    int64_t i = INT64_MIN;
    REQUIRE_EQ(std::string("-9,223,372,036,854,775,808"),
                         util::itos(i, true));
  }
}

void UtilTest2::testUitos()
{
  {
    uint16_t i = 12345;
    REQUIRE_EQ(std::string("12345"), util::uitos(i));
  }
  {
    int16_t i = -12345;
    REQUIRE_EQ(std::string("/.-,+"), util::uitos(i));
  }
}

void UtilTest2::testNtoh64()
{
  uint64_t x = 0xff00ff00ee00ee00LL;
#ifdef WORDS_BIGENDIAN
  REQUIRE_EQ(x, ntoh64(x));
  REQUIRE_EQ(x, hton64(x));
#else  // !WORDS_BIGENDIAN
  uint64_t y = 0x00ee00ee00ff00ffLL;
  REQUIRE_EQ(y, ntoh64(x));
  REQUIRE_EQ(x, hton64(y));
#endif // !WORDS_BIGENDIAN
}

void UtilTest2::testPercentEncode()
{
  REQUIRE_EQ(
      std::string("%3A%2F%3F%23%5B%5D%40%21%25%26%27%28%29%2A%2B%2C%3B%3D"),
      util::percentEncode(":/?#[]@!%&'()*+,;="));

  std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz"
                           "0123456789"
                           "-._~";
  REQUIRE_EQ(unreserved, util::percentEncode(unreserved));

  REQUIRE_EQ(std::string("1%5EA%20"), util::percentEncode("1^A "));
}

void UtilTest2::testPercentEncodeMini()
{
  REQUIRE_EQ(std::string("%80"),
                       util::percentEncodeMini({(char)0x80}));
}

void UtilTest2::testHtmlEscape()
{
  REQUIRE_EQ(std::string("aria2&lt;&gt;&quot;&#39;util"),
                       util::htmlEscape("aria2<>\"'util"));
}

void UtilTest2::testJoinPath()
{
  const std::string dir1dir2file[] = {"dir1", "dir2", "file"};
  REQUIRE_EQ(
      std::string("dir1/dir2/file"),
      util::joinPath(std::begin(dir1dir2file), std::end(dir1dir2file)));

  const std::string dirparentfile[] = {"dir", "..", "file"};
  REQUIRE_EQ(
      std::string("file"),
      util::joinPath(std::begin(dirparentfile), std::end(dirparentfile)));

  const std::string dirparentparentfile[] = {"dir", "..", "..", "file"};
  REQUIRE_EQ(std::string("file"),
                       util::joinPath(std::begin(dirparentparentfile),
                                      std::end(dirparentparentfile)));

  const std::string dirdotfile[] = {"dir", ".", "file"};
  REQUIRE_EQ(
      std::string("dir/file"),
      util::joinPath(std::begin(dirdotfile), std::end(dirdotfile)));

  const std::string empty[] = {};
  REQUIRE_EQ(std::string(""), util::joinPath(&empty[0], &empty[0]));

  const std::string parentdot[] = {"..", "."};
  REQUIRE_EQ(std::string(""), util::joinPath(std::begin(parentdot),
                                                       std::end(parentdot)));
}

void UtilTest2::testParseIndexPath()
{
  std::pair<size_t, std::string> p = util::parseIndexPath("1=foo");
  REQUIRE_EQ((size_t)1, p.first);
  REQUIRE_EQ(std::string("foo"), p.second);
  try {
    util::parseIndexPath("1X=foo");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    // success
  }
  try {
    util::parseIndexPath("1=");
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    // success
  }
}

void UtilTest2::testCreateIndexPaths()
{
  std::stringstream in("1=/tmp/myfile\n"
                       "100=/myhome/mypicture.png\n");
  std::vector<std::pair<size_t, std::string>> m = util::createIndexPaths(in);
  REQUIRE_EQ((size_t)2, m.size());
  REQUIRE_EQ((size_t)1, m[0].first);
  REQUIRE_EQ(std::string("/tmp/myfile"), m[0].second);
  REQUIRE_EQ((size_t)100, m[1].first);
  REQUIRE_EQ(std::string("/myhome/mypicture.png"), m[1].second);
}

void UtilTest2::testGenerateRandomData()
{
  using namespace std;

  // Simple sanity check
  unsigned char data1[25];
  memset(data1, 0, sizeof(data1));
  util::generateRandomData(data1, sizeof(data1));

  unsigned char data2[25];
  memset(data2, 0, sizeof(data2));
  util::generateRandomData(data2, sizeof(data2));

  REQUIRE(memcmp(data1, data2, sizeof(data1)) != 0);

  // Simple stddev/mean tests
  map<uint8_t, size_t> counts;
  uint8_t bytes[1 << 20];
  for (auto i = 0; i < 10; ++i) {
    util::generateRandomData(bytes, sizeof(bytes));
    for (auto b : bytes) {
      counts[b]++;
    }
  }
  REQUIRE_MESSAGE(counts.size() == 256, "Should see all kinds of bytes");
  double sum =
      accumulate(counts.begin(), counts.end(), 0.0,
                 [](double total, const decltype(counts)::value_type& elem) {
                   return total + elem.second;
                 });
  double mean = sum / counts.size();
  vector<double> diff(counts.size());
  transform(counts.begin(), counts.end(), diff.begin(),
            [&](const decltype(counts)::value_type& elem) -> double {
              return (double)elem.second - mean;
            });
  double sq_sum = inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
  double stddev = sqrt(sq_sum / counts.size());
  cout << "stddev: " << fixed << stddev << endl;
  REQUIRE_MESSAGE(stddev <= 320, "stddev makes sense (lower)");
  REQUIRE_MESSAGE(stddev >= 100, "stddev makes sense (upper)");
}

void UtilTest2::testFromHex()
{
  std::string src;
  std::string dest;

  src = "0011fF";
  dest = util::fromHex(src.begin(), src.end());
  REQUIRE_EQ((size_t)3, dest.size());
  REQUIRE_EQ((char)0x00, dest[0]);
  REQUIRE_EQ((char)0x11, dest[1]);
  REQUIRE_EQ((char)0xff, dest[2]);

  src = "0011f";
  REQUIRE(util::fromHex(src.begin(), src.end()).empty());

  src = "001g";
  REQUIRE(util::fromHex(src.begin(), src.end()).empty());
}

void UtilTest2::testParsePrioritizePieceRange()
{
  // piece index
  // 0     1     2     3     4     5     6     7
  // |     |              |                    |
  // file1 |              |                    |
  //       |              |                    |
  //       file2          |                    |
  //                    file3                  |
  //                      |                    |
  //                      file4                |
  constexpr size_t pieceLength = 1_k;
  std::vector<std::shared_ptr<FileEntry>> entries(4,
                                                  std::shared_ptr<FileEntry>());
  entries[0].reset(new FileEntry("file1", 1024, 0));
  entries[1].reset(new FileEntry("file2", 2560, entries[0]->getLastOffset()));
  entries[2].reset(new FileEntry("file3", 0, entries[1]->getLastOffset()));
  entries[3].reset(new FileEntry("file4", 3584, entries[2]->getLastOffset()));

  std::vector<size_t> result;
  util::parsePrioritizePieceRange(result, "head=1", entries, pieceLength);
  REQUIRE_EQ((size_t)3, result.size());
  REQUIRE_EQ((size_t)0, result[0]);
  REQUIRE_EQ((size_t)1, result[1]);
  REQUIRE_EQ((size_t)3, result[2]);
  result.clear();
  util::parsePrioritizePieceRange(result, "tail=1", entries, pieceLength);
  REQUIRE_EQ((size_t)3, result.size());
  REQUIRE_EQ((size_t)0, result[0]);
  REQUIRE_EQ((size_t)3, result[1]);
  REQUIRE_EQ((size_t)6, result[2]);
  result.clear();
  util::parsePrioritizePieceRange(result, "head=1K", entries, pieceLength);
  REQUIRE_EQ((size_t)4, result.size());
  REQUIRE_EQ((size_t)0, result[0]);
  REQUIRE_EQ((size_t)1, result[1]);
  REQUIRE_EQ((size_t)3, result[2]);
  REQUIRE_EQ((size_t)4, result[3]);
  result.clear();
  util::parsePrioritizePieceRange(result, "head", entries, pieceLength, 1_k);
  REQUIRE_EQ((size_t)4, result.size());
  REQUIRE_EQ((size_t)0, result[0]);
  REQUIRE_EQ((size_t)1, result[1]);
  REQUIRE_EQ((size_t)3, result[2]);
  REQUIRE_EQ((size_t)4, result[3]);
  result.clear();
  util::parsePrioritizePieceRange(result, "tail=1K", entries, pieceLength);
  REQUIRE_EQ((size_t)4, result.size());
  REQUIRE_EQ((size_t)0, result[0]);
  REQUIRE_EQ((size_t)2, result[1]);
  REQUIRE_EQ((size_t)3, result[2]);
  REQUIRE_EQ((size_t)6, result[3]);
  result.clear();
  util::parsePrioritizePieceRange(result, "tail", entries, pieceLength, 1_k);
  REQUIRE_EQ((size_t)4, result.size());
  REQUIRE_EQ((size_t)0, result[0]);
  REQUIRE_EQ((size_t)2, result[1]);
  REQUIRE_EQ((size_t)3, result[2]);
  REQUIRE_EQ((size_t)6, result[3]);
  result.clear();
  util::parsePrioritizePieceRange(result, "head=1,tail=1", entries,
                                  pieceLength);
  REQUIRE_EQ((size_t)4, result.size());
  REQUIRE_EQ((size_t)0, result[0]);
  REQUIRE_EQ((size_t)1, result[1]);
  REQUIRE_EQ((size_t)3, result[2]);
  REQUIRE_EQ((size_t)6, result[3]);
  result.clear();
  util::parsePrioritizePieceRange(result, "head=300M,tail=300M", entries,
                                  pieceLength);
  REQUIRE_EQ((size_t)7, result.size());
  for (size_t i = 0; i < 7; ++i) {
    REQUIRE_EQ(i, result[i]);
  }
  result.clear();
  util::parsePrioritizePieceRange(result, "", entries, pieceLength);
  REQUIRE(result.empty());
}

void UtilTest2::testApplyDir()
{
  REQUIRE_EQ(std::string("./pred"), util::applyDir("", "pred"));
  REQUIRE_EQ(std::string("/pred"), util::applyDir("/", "pred"));
  REQUIRE_EQ(std::string("./pred"), util::applyDir(".", "pred"));
  REQUIRE_EQ(std::string("/dl/pred"), util::applyDir("/dl", "pred"));
#ifndef __MINGW32__
  REQUIRE_EQ(std::string("/dev/null"),
                       util::applyDir(".", "/dev/null"));
#else  // __MINGW32__
  REQUIRE_EQ(std::string("C:/download/file"),
                       util::applyDir("D:/aria2", "C:/download/file"));
#endif // __MINGW32__
}

void UtilTest2::testFixTaintedBasename()
{
  REQUIRE_EQ(std::string("a%2Fb"), util::fixTaintedBasename("a/b"));
#ifdef __MINGW32__
  REQUIRE_EQ(std::string("a%5Cb"), util::fixTaintedBasename("a\\b"));
#else  // !__MINGW32__
  REQUIRE_EQ(std::string("a\\b"), util::fixTaintedBasename("a\\b"));
#endif // !__MINGW32__
}

void UtilTest2::testIsNumericHost()
{
  REQUIRE(util::isNumericHost("192.168.0.1"));
  REQUIRE(!util::isNumericHost("aria2.sf.net"));
  REQUIRE(util::isNumericHost("::1"));
}

void UtilTest2::testDetectDirTraversal()
{
  REQUIRE(util::detectDirTraversal("/foo"));
  REQUIRE(util::detectDirTraversal("./foo"));
  REQUIRE(util::detectDirTraversal("../foo"));
  REQUIRE(util::detectDirTraversal("foo/../bar"));
  REQUIRE(util::detectDirTraversal("foo/./bar"));
  REQUIRE(util::detectDirTraversal("foo/."));
  REQUIRE(util::detectDirTraversal("foo/.."));
  REQUIRE(util::detectDirTraversal("."));
  REQUIRE(util::detectDirTraversal(".."));
  REQUIRE(util::detectDirTraversal("/"));
  REQUIRE(util::detectDirTraversal("foo/"));
  REQUIRE(util::detectDirTraversal("\t"));
  REQUIRE(!util::detectDirTraversal("foo/bar"));
  REQUIRE(!util::detectDirTraversal("foo"));
}

void UtilTest2::testEscapePath()
{
  REQUIRE_EQ(std::string("foo%00bar%00%01"),
                       util::escapePath(std::string("foo") + (char)0x00 +
                                        std::string("bar") + (char)0x00 +
                                        (char)0x01));
#ifdef __MINGW32__
  REQUIRE_EQ(std::string("foo%5Cbar"), util::escapePath("foo\\bar"));
#else  // !__MINGW32__
  REQUIRE_EQ(std::string("foo\\bar"), util::escapePath("foo\\bar"));
#endif // !__MINGW32__
}

void UtilTest2::testInSameCidrBlock()
{
  REQUIRE(util::inSameCidrBlock("192.168.128.1", "192.168.0.1", 16));
  REQUIRE(!util::inSameCidrBlock("192.168.128.1", "192.168.0.1", 17));

  REQUIRE(util::inSameCidrBlock("192.168.0.1", "192.168.0.1", 32));
  REQUIRE(!util::inSameCidrBlock("192.168.0.1", "192.168.0.0", 32));

  REQUIRE(util::inSameCidrBlock("192.168.0.1", "10.0.0.1", 0));

  REQUIRE(util::inSameCidrBlock("2001:db8::2:1", "2001:db0::2:2", 28));
  REQUIRE(!util::inSameCidrBlock("2001:db8::2:1", "2001:db0::2:2", 29));

  REQUIRE(!util::inSameCidrBlock("2001:db8::2:1", "192.168.0.1", 8));
}

void UtilTest2::testIsUtf8String()
{
  REQUIRE(util::isUtf8("ascii"));
  // "Hello World" in Japanese UTF-8
  REQUIRE(
      util::isUtf8(fromHex("e38193e38293e381abe381a1e381afe4b896e7958c")));
  // "World" in Shift_JIS
  REQUIRE(!util::isUtf8(fromHex("90a28a") + "E"));
  // UTF8-2
  REQUIRE(util::isUtf8(fromHex("c280")));
  REQUIRE(util::isUtf8(fromHex("dfbf")));
  // UTF8-3
  REQUIRE(util::isUtf8(fromHex("e0a080")));
  REQUIRE(util::isUtf8(fromHex("e0bf80")));
  REQUIRE(util::isUtf8(fromHex("e18080")));
  REQUIRE(util::isUtf8(fromHex("ec8080")));
  REQUIRE(util::isUtf8(fromHex("ed8080")));
  REQUIRE(util::isUtf8(fromHex("ed9f80")));
  REQUIRE(util::isUtf8(fromHex("ee8080")));
  REQUIRE(util::isUtf8(fromHex("ef8080")));
  // UTF8-4
  REQUIRE(util::isUtf8(fromHex("f0908080")));
  REQUIRE(util::isUtf8(fromHex("f0bf8080")));
  REQUIRE(util::isUtf8(fromHex("f1808080")));
  REQUIRE(util::isUtf8(fromHex("f3808080")));
  REQUIRE(util::isUtf8(fromHex("f4808080")));
  REQUIRE(util::isUtf8(fromHex("f48f8080")));

  REQUIRE(util::isUtf8(""));
  REQUIRE(!util::isUtf8(fromHex("00")));
}

void UtilTest2::testNextParam()
{
  std::string s1 = "    :a  :  b=c :d=b::::g::";
  std::pair<std::string::iterator, bool> r;
  std::string name, value;
  r = util::nextParam(name, value, s1.begin(), s1.end(), ':');
  REQUIRE(r.second);
  REQUIRE_EQ(std::string("a"), name);
  REQUIRE_EQ(std::string(), value);

  r = util::nextParam(name, value, r.first, s1.end(), ':');
  REQUIRE(r.second);
  REQUIRE_EQ(std::string("b"), name);
  REQUIRE_EQ(std::string("c"), value);

  r = util::nextParam(name, value, r.first, s1.end(), ':');
  REQUIRE(r.second);
  REQUIRE_EQ(std::string("d"), name);
  REQUIRE_EQ(std::string("b"), value);

  r = util::nextParam(name, value, r.first, s1.end(), ':');
  REQUIRE(r.second);
  REQUIRE_EQ(std::string("g"), name);
  REQUIRE_EQ(std::string(), value);

  std::string s2 = "";
  r = util::nextParam(name, value, s2.begin(), s2.end(), ':');
  REQUIRE(!r.second);

  std::string s3 = "   ";
  r = util::nextParam(name, value, s3.begin(), s3.end(), ':');
  REQUIRE(!r.second);

  std::string s4 = ":::";
  r = util::nextParam(name, value, s4.begin(), s4.end(), ':');
  REQUIRE(!r.second);
}

void UtilTest2::testNoProxyDomainMatch()
{
  REQUIRE(util::noProxyDomainMatch("localhost", "localhost"));
  REQUIRE(util::noProxyDomainMatch("192.168.0.1", "192.168.0.1"));
  REQUIRE(util::noProxyDomainMatch("www.example.org", ".example.org"));
  REQUIRE(!util::noProxyDomainMatch("www.example.org", "example.org"));
  REQUIRE(!util::noProxyDomainMatch("192.168.0.1", "0.1"));
  REQUIRE(!util::noProxyDomainMatch("example.org", "example.com"));
  REQUIRE(!util::noProxyDomainMatch("example.org", "www.example.org"));
}

void UtilTest2::testInPrivateAddress()
{
  REQUIRE(!util::inPrivateAddress("localhost"));
  REQUIRE(util::inPrivateAddress("192.168.0.1"));
  // Only checks prefix..
  REQUIRE(util::inPrivateAddress("10."));
  REQUIRE(!util::inPrivateAddress("172."));
  REQUIRE(!util::inPrivateAddress("172.15.0.0"));
  REQUIRE(util::inPrivateAddress("172.16.0.0"));
  REQUIRE(util::inPrivateAddress("172.31.0.0"));
  REQUIRE(!util::inPrivateAddress("172.32.0.0"));
}

void UtilTest2::testSecfmt()
{
  REQUIRE_EQ(std::string("0s"), util::secfmt(0));
  REQUIRE_EQ(std::string("1s"), util::secfmt(1));
  REQUIRE_EQ(std::string("9s"), util::secfmt(9));
  REQUIRE_EQ(std::string("10s"), util::secfmt(10));
  REQUIRE_EQ(std::string("1m"), util::secfmt(60));
  REQUIRE_EQ(std::string("1m59s"), util::secfmt(119));
  REQUIRE_EQ(std::string("2m"), util::secfmt(120));
  REQUIRE_EQ(std::string("59m59s"), util::secfmt(3599));
  REQUIRE_EQ(std::string("1h"), util::secfmt(3600));
}

void UtilTest2::testTlsHostnameMatch()
{
  REQUIRE(util::tlsHostnameMatch("Foo.com", "foo.com"));
  REQUIRE(util::tlsHostnameMatch("*.a.com", "foo.a.com"));
  REQUIRE(!util::tlsHostnameMatch("*.a.com", "bar.foo.a.com"));
  REQUIRE(!util::tlsHostnameMatch("f*.com", "foo.com"));
  REQUIRE(!util::tlsHostnameMatch("*.com", "bar.com"));
  REQUIRE(util::tlsHostnameMatch("com", "com"));
  REQUIRE(!util::tlsHostnameMatch("foo.*", "foo.com"));
  REQUIRE(util::tlsHostnameMatch("a.foo.com", "A.foo.com"));
  REQUIRE(!util::tlsHostnameMatch("a.foo.com", "b.foo.com"));
  REQUIRE(!util::tlsHostnameMatch("*a.foo.com", "a.foo.com"));
  REQUIRE(util::tlsHostnameMatch("*a.foo.com", "ba.foo.com"));
  REQUIRE(!util::tlsHostnameMatch("a*.foo.com", "a.foo.com"));
  REQUIRE(util::tlsHostnameMatch("a*.foo.com", "ab.foo.com"));
  REQUIRE(!util::tlsHostnameMatch("foo.b*z.foo.com", "foo.baz.foo.com"));
  REQUIRE(util::tlsHostnameMatch("B*z.foo.com", "bAZ.Foo.com"));
  REQUIRE(!util::tlsHostnameMatch("b*z.foo.com", "bz.foo.com"));
  REQUIRE(!util::tlsHostnameMatch("*", "foo"));
  REQUIRE(!util::tlsHostnameMatch("*", ""));
  REQUIRE(util::tlsHostnameMatch("", ""));
  REQUIRE(!util::tlsHostnameMatch("xn--*.a.b", "xn--c.a.b"));
}

void UtilTest2::testParseDoubleNoThrow()
{
  double n;

  REQUIRE(util::parseDoubleNoThrow(n, " 123 "));
  REQUIRE_EQ(123., n);

  REQUIRE(util::parseDoubleNoThrow(n, "3.14"));
  REQUIRE_EQ(3.14, n);

  REQUIRE(util::parseDoubleNoThrow(n, "-3.14"));
  REQUIRE_EQ(-3.14, n);

  REQUIRE(!util::parseDoubleNoThrow(n, ""));
  REQUIRE(!util::parseDoubleNoThrow(n, "123x"));
}

} // namespace aria2
