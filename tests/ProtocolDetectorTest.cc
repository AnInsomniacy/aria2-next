#include "ProtocolDetector.h"

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"

namespace aria2 {

class ProtocolDetectorTest {


public:
  void setUp() {}

  void tearDown() {}

  void testIsStreamProtocol();
  void testGuessEd2kLink();
  void testGuessTorrentFile();
  void testGuessTorrentMagnet();
  void testGuessMetalinkFile();
};

A2_TEST(ProtocolDetectorTest, testIsStreamProtocol)
A2_TEST(ProtocolDetectorTest, testGuessEd2kLink)
A2_TEST(ProtocolDetectorTest, testGuessTorrentFile)
A2_TEST(ProtocolDetectorTest, testGuessTorrentMagnet)
A2_TEST(ProtocolDetectorTest, testGuessMetalinkFile)

void ProtocolDetectorTest::testIsStreamProtocol()
{
  ProtocolDetector detector;
  REQUIRE(detector.isStreamProtocol("http://localhost/index.html"));
  REQUIRE(detector.isStreamProtocol("https://localhost/index.html"));
  REQUIRE(detector.isStreamProtocol("ftp://localhost/index.html"));
  REQUIRE(detector.isStreamProtocol("sftp://localhost/index.html"));
  REQUIRE(!detector.isStreamProtocol("magnet:?xt=urn:btih:abc"));
  REQUIRE(!detector.isStreamProtocol("E://downloads/file.torrent"));
  REQUIRE(!detector.isStreamProtocol("/home/web/localhost/index.html"));
}

void ProtocolDetectorTest::testGuessEd2kLink()
{
  ProtocolDetector detector;
  REQUIRE(detector.guessEd2kLink(
      "ed2k://|file|aria2-next.bin|1|0123456789abcdef0123456789abcdef|/"));
  REQUIRE(detector.guessEd2kLink("ed2k://|server|127.0.0.1|4661|/"));
  REQUIRE(detector.guessEd2kLink("ed2k://|search|linux%20iso|/"));
  REQUIRE(!detector.guessEd2kLink("ed2k://|file|bad.bin|x|bad|/"));
  REQUIRE(!detector.guessEd2kLink("magnet:?xt=urn:btih:abc"));
}

void ProtocolDetectorTest::testGuessTorrentFile()
{
  ProtocolDetector detector;
  REQUIRE(detector.guessTorrentFile(A2_TEST_DIR "/test.torrent"));
  REQUIRE(!detector.guessTorrentFile("http://localhost/test.torrent"));
  REQUIRE(!detector.guessTorrentFile(A2_TEST_DIR "/test.xml"));
}

void ProtocolDetectorTest::testGuessTorrentMagnet()
{
  ProtocolDetector detector;
#ifdef ENABLE_BITTORRENT
  REQUIRE(detector.guessTorrentMagnet(
      "magnet:?xt=urn:btih:248d0a1cd08284299de78d5c1ed359bb46717d8c"));
  REQUIRE(!detector.guessTorrentMagnet("magnet:?"));
#else  // !ENABLE_BITTORRENT
  REQUIRE(!detector.guessTorrentMagnet(
      "magnet:?xt=urn:btih:248d0a1cd08284299de78d5c1ed359bb46717d8c"));
#endif // !ENABLE_BITTORRENT
}

void ProtocolDetectorTest::testGuessMetalinkFile()
{
  ProtocolDetector detector;
  REQUIRE(detector.guessMetalinkFile(A2_TEST_DIR "/test.xml"));
  REQUIRE(!detector.guessMetalinkFile("http://localhost/test.xml"));
  REQUIRE(!detector.guessMetalinkFile(A2_TEST_DIR "/test.torrent"));
}

} // namespace aria2
