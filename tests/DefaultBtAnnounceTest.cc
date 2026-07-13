#include "DefaultBtAnnounce.h"

#include <iostream>

#include "a2doctest.h"

#include "TestUtil.h"
#include "Option.h"
#include "util.h"
#include "Exception.h"
#include "MockPieceStorage.h"
#include "MockPeerStorage.h"
#include "BtRuntime.h"
#include "AnnounceTier.h"
#include "FixedNumberRandomizer.h"
#include "FileEntry.h"
#include "prefs.h"
#include "DownloadContext.h"
#include "bittorrent_helper.h"
#include "array_fun.h"
#include "UDPTrackerRequest.h"
#include "SocketCore.h"

namespace aria2 {

class DefaultBtAnnounceTest {


private:
  std::shared_ptr<DownloadContext> dctx_;
  std::shared_ptr<MockPieceStorage> pieceStorage_;
  std::shared_ptr<MockPeerStorage> peerStorage_;
  std::shared_ptr<BtRuntime> btRuntime_;
  std::unique_ptr<Randomizer> randomizer_;
  Option* option_;

public:
  void setUp()
  {
    option_ = new Option();

    int64_t totalLength = 4_m;
    int32_t pieceLength = 256_k;

    static const unsigned char infoHash[] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23,
        0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67};

    std::string peerId = "-aria2-ultrafastdltl";

    dctx_.reset(new DownloadContext(pieceLength, totalLength));
    {
      auto torrentAttrs = make_unique<TorrentAttribute>();
      torrentAttrs->infoHash.assign(std::begin(infoHash), std::end(infoHash));
      dctx_->setAttribute(CTX_ATTR_BT, std::move(torrentAttrs));
    }
    dctx_->getNetStat().updateDownload(pieceLength * 5);
    dctx_->getNetStat().updateUpload(pieceLength * 6);
    bittorrent::setStaticPeerId(peerId);

    pieceStorage_.reset(new MockPieceStorage());
    pieceStorage_->setTotalLength(totalLength);
    pieceStorage_->setCompletedLength(pieceLength * 10);

    peerStorage_.reset(new MockPeerStorage());
    btRuntime_.reset(new BtRuntime());

    randomizer_.reset(new FixedNumberRandomizer());
  }

  void tearDown() { delete option_; }

  void testGetAnnounceUrl();
  void testGetAnnounceUrl_withQuery();
  void testGetAnnounceUrl_externalIP();
  void testNoMoreAnnounce();
  void testIsAllAnnounceFailed();
  void testURLOrderInStoppedEvent();
  void testURLOrderInCompletedEvent();
  void testProcessAnnounceResponse_malformed();
  void testProcessAnnounceResponse_failureReason();
  void testProcessAnnounceResponse();
  void testProcessUDPTrackerResponse();
};

A2_TEST(DefaultBtAnnounceTest, testGetAnnounceUrl)
A2_TEST(DefaultBtAnnounceTest, testGetAnnounceUrl_withQuery)
A2_TEST(DefaultBtAnnounceTest, testGetAnnounceUrl_externalIP)
A2_TEST(DefaultBtAnnounceTest, testNoMoreAnnounce)
A2_TEST(DefaultBtAnnounceTest, testIsAllAnnounceFailed)
A2_TEST(DefaultBtAnnounceTest, testURLOrderInStoppedEvent)
A2_TEST(DefaultBtAnnounceTest, testURLOrderInCompletedEvent)
A2_TEST(DefaultBtAnnounceTest, testProcessAnnounceResponse_malformed)
A2_TEST(DefaultBtAnnounceTest, testProcessAnnounceResponse_failureReason)
A2_TEST(DefaultBtAnnounceTest, testProcessAnnounceResponse)
A2_TEST(DefaultBtAnnounceTest, testProcessUDPTrackerResponse)

namespace {
template <typename InputIterator>
std::unique_ptr<List> createAnnounceTier(InputIterator first,
                                         InputIterator last)
{
  auto announceTier = List::g();
  for (; first != last; ++first) {
    announceTier->append(String::g(*first));
  }
  return announceTier;
}
} // namespace

namespace {
std::unique_ptr<List> createAnnounceTier(const std::string& uri)
{
  auto announceTier = List::g();
  announceTier->append(String::g(uri));
  return announceTier;
}
} // namespace

namespace {
void setAnnounceList(const std::shared_ptr<DownloadContext>& dctx,
                     const List* announceList)
{
  std::vector<std::vector<std::string>> dest;
  for (auto tierIter = announceList->begin(), eoi = announceList->end();
       tierIter != eoi; ++tierIter) {
    std::vector<std::string> ntier;
    const List* tier = downcast<List>(*tierIter);
    for (auto uriIter = tier->begin(), eoi2 = tier->end(); uriIter != eoi2;
         ++uriIter) {
      const String* uri = downcast<String>(*uriIter);
      ntier.push_back(uri->s());
    }
    dest.push_back(std::move(ntier));
  }
  bittorrent::getTorrentAttrs(dctx)->announceList.swap(dest);
}
} // namespace

void DefaultBtAnnounceTest::testNoMoreAnnounce()
{
  auto announceList = List::g();
  announceList->append(createAnnounceTier("http://localhost/announce"));
  announceList->append(createAnnounceTier("http://backup/announce"));

  setAnnounceList(dctx_, announceList.get());

  DefaultBtAnnounce btAnnounce(dctx_.get(), option_);
  btAnnounce.setPieceStorage(pieceStorage_);
  btAnnounce.setPeerStorage(peerStorage_);
  btAnnounce.setBtRuntime(btRuntime_);
  btAnnounce.setRandomizer(randomizer_.get());
  btAnnounce.setTcpPort(6989);

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=started&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceFailure();

  REQUIRE_EQ(
      std::string("http://backup/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=started&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  pieceStorage_->setAllDownloadFinished(true);

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=completed&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  REQUIRE_EQ(
      std::string("http://backup/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=completed&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  btRuntime_->setHalt(true);

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=0&no_peer_id=1&port=6989&event=stopped&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  REQUIRE_EQ(
      std::string("http://backup/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=0&no_peer_id=1&port=6989&event=stopped&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  REQUIRE(btAnnounce.noMoreAnnounce());
}

void DefaultBtAnnounceTest::testGetAnnounceUrl()
{
  auto announceList = List::g();
  announceList->append(createAnnounceTier("http://localhost/announce"));
  setAnnounceList(dctx_, announceList.get());

  DefaultBtAnnounce btAnnounce(dctx_.get(), option_);
  btAnnounce.setPieceStorage(pieceStorage_);
  btAnnounce.setPeerStorage(peerStorage_);
  btAnnounce.setBtRuntime(btRuntime_);
  btAnnounce.setRandomizer(randomizer_.get());
  btAnnounce.setTcpPort(6989);
  std::shared_ptr<UDPTrackerRequest> req;

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=started&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());
  req = btAnnounce.createUDPTrackerRequest("localhost", 80, 6989);
  REQUIRE_EQ(std::string("localhost"), req->remoteAddr);
  REQUIRE_EQ((uint16_t)80, req->remotePort);
  REQUIRE_EQ((int)UDPT_ACT_ANNOUNCE, req->action);
  REQUIRE_EQ(bittorrent::getInfoHashString(dctx_),
                       util::toHex(req->infohash));
  REQUIRE_EQ(std::string("-aria2-ultrafastdltl"), req->peerId);
  REQUIRE_EQ((int64_t)1310720, req->downloaded);
  REQUIRE_EQ((int64_t)1572864, req->left);
  REQUIRE_EQ((int64_t)1572864, req->uploaded);
  REQUIRE_EQ((int)UDPT_EVT_STARTED, req->event);
  REQUIRE_EQ((uint32_t)0, req->ip);
  REQUIRE_EQ((int32_t)50, req->numWant);
  REQUIRE_EQ((uint16_t)6989, req->port);
  REQUIRE_EQ((uint16_t)0, req->extensions);

  btAnnounce.announceSuccess();

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&supportcrypto=1"),
      btAnnounce.getAnnounceUrl());
  req = btAnnounce.createUDPTrackerRequest("localhost", 80, 6989);
  REQUIRE_EQ((int)UDPT_ACT_ANNOUNCE, req->action);
  REQUIRE_EQ((int)UDPT_EVT_NONE, req->event);

  btAnnounce.announceSuccess();

  pieceStorage_->setAllDownloadFinished(true);

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=completed&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());
  req = btAnnounce.createUDPTrackerRequest("localhost", 80, 6989);
  REQUIRE_EQ((int)UDPT_ACT_ANNOUNCE, req->action);
  REQUIRE_EQ((int)UDPT_EVT_COMPLETED, req->event);

  btAnnounce.announceSuccess();

  btRuntime_->setHalt(true);

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=0&no_peer_id=1&port=6989&event=stopped&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());
  req = btAnnounce.createUDPTrackerRequest("localhost", 80, 6989);
  REQUIRE_EQ((int)UDPT_ACT_ANNOUNCE, req->action);
  REQUIRE_EQ((int)UDPT_EVT_STOPPED, req->event);
}

void DefaultBtAnnounceTest::testGetAnnounceUrl_withQuery()
{
  auto announceList = List::g();
  announceList->append(createAnnounceTier("http://localhost/announce?k=v"));
  setAnnounceList(dctx_, announceList.get());

  DefaultBtAnnounce btAnnounce(dctx_.get(), option_);
  btAnnounce.setPieceStorage(pieceStorage_);
  btAnnounce.setPeerStorage(peerStorage_);
  btAnnounce.setBtRuntime(btRuntime_);
  btAnnounce.setRandomizer(randomizer_.get());
  btAnnounce.setTcpPort(6989);

  REQUIRE_EQ(
      std::string(
          "http://localhost/announce?k=v&"
          "info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%01%23Eg&"
          "peer_id=-aria2-ultrafastdltl&"
          "uploaded=1572864&downloaded=1310720&left=1572864&compact=1&"
          "key=fastdltl&numwant=50&no_peer_id=1&port=6989&event=started&"
          "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());
}

void DefaultBtAnnounceTest::testGetAnnounceUrl_externalIP()
{
  auto announceList = List::g();
  announceList->append(createAnnounceTier("http://localhost/announce"));
  setAnnounceList(dctx_, announceList.get());

  option_->put(PREF_BT_EXTERNAL_IP, "192.168.1.1");
  DefaultBtAnnounce btAnnounce(dctx_.get(), option_);
  btAnnounce.setPieceStorage(pieceStorage_);
  btAnnounce.setPeerStorage(peerStorage_);
  btAnnounce.setBtRuntime(btRuntime_);
  btAnnounce.setRandomizer(randomizer_.get());
  btAnnounce.setTcpPort(6989);

  REQUIRE_EQ(
      std::string(
          "http://localhost/announce?"
          "info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%01%23Eg&"
          "peer_id=-aria2-ultrafastdltl&"
          "uploaded=1572864&downloaded=1310720&left=1572864&compact=1&"
          "key=fastdltl&numwant=50&no_peer_id=1&port=6989&event=started&"
          "supportcrypto=1&ip=192.168.1.1"),
      btAnnounce.getAnnounceUrl());

  std::shared_ptr<UDPTrackerRequest> req;
  req = btAnnounce.createUDPTrackerRequest("localhost", 80, 6989);
  char host[NI_MAXHOST];
  int rv = inetNtop(AF_INET, &req->ip, host, sizeof(host));
  REQUIRE_EQ(0, rv);
  REQUIRE_EQ(std::string("192.168.1.1"), std::string(host));
}

void DefaultBtAnnounceTest::testIsAllAnnounceFailed()
{
  auto announceList = List::g();
  announceList->append(createAnnounceTier("http://localhost/announce"));
  announceList->append(createAnnounceTier("http://backup/announce"));
  setAnnounceList(dctx_, announceList.get());

  DefaultBtAnnounce btAnnounce(dctx_.get(), option_);
  btAnnounce.setPieceStorage(pieceStorage_);
  btAnnounce.setPeerStorage(peerStorage_);
  btAnnounce.setBtRuntime(btRuntime_);
  btAnnounce.setRandomizer(randomizer_.get());
  btAnnounce.setTcpPort(6989);

  REQUIRE_EQ(
      std::string("http://localhost/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=started&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceFailure();

  REQUIRE_EQ(
      std::string("http://backup/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=started&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceFailure();

  REQUIRE(!btAnnounce.isAnnounceReady());
  REQUIRE_EQ(std::string(""), btAnnounce.getAnnounceUrl());
  REQUIRE(btAnnounce.isAllAnnounceFailed());

  btAnnounce.resetAnnounce();

  REQUIRE(!btAnnounce.isAllAnnounceFailed());
}

void DefaultBtAnnounceTest::testURLOrderInStoppedEvent()
{
  const char* urls[] = {"http://localhost1/announce",
                        "http://localhost2/announce"};

  auto announceList = List::g();
  announceList->append(createAnnounceTier(std::begin(urls), std::end(urls)));
  setAnnounceList(dctx_, announceList.get());

  DefaultBtAnnounce btAnnounce(dctx_.get(), option_);
  btAnnounce.setPieceStorage(pieceStorage_);
  btAnnounce.setPeerStorage(peerStorage_);
  btAnnounce.setBtRuntime(btRuntime_);
  btAnnounce.setRandomizer(randomizer_.get());
  btAnnounce.setTcpPort(6989);

  REQUIRE_EQ(
      std::string("http://localhost1/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=started&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  btRuntime_->setHalt(true);

  REQUIRE_EQ(
      std::string("http://localhost1/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=0&no_peer_id=1&port=6989&event=stopped&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceFailure();

  REQUIRE_EQ(
      std::string("http://localhost2/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=0&no_peer_id=1&port=6989&event=stopped&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();
}

void DefaultBtAnnounceTest::testURLOrderInCompletedEvent()
{
  const char* urls[] = {"http://localhost1/announce",
                        "http://localhost2/announce"};

  auto announceList = List::g();
  announceList->append(createAnnounceTier(std::begin(urls), std::end(urls)));
  setAnnounceList(dctx_, announceList.get());

  DefaultBtAnnounce btAnnounce(dctx_.get(), option_);
  btAnnounce.setPieceStorage(pieceStorage_);
  btAnnounce.setPeerStorage(peerStorage_);
  btAnnounce.setBtRuntime(btRuntime_);
  btAnnounce.setRandomizer(randomizer_.get());
  btAnnounce.setTcpPort(6989);

  REQUIRE_EQ(
      std::string("http://localhost1/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=started&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();

  pieceStorage_->setAllDownloadFinished(true);

  REQUIRE_EQ(
      std::string("http://localhost1/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=completed&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceFailure();

  REQUIRE_EQ(
      std::string("http://localhost2/"
                  "announce?info_hash=%01%23Eg%89%AB%CD%EF%01%23Eg%89%AB%CD%EF%"
                  "01%23Eg&peer_id=-aria2-ultrafastdltl&uploaded=1572864&"
                  "downloaded=1310720&left=1572864&compact=1&key=fastdltl&"
                  "numwant=50&no_peer_id=1&port=6989&event=completed&"
                  "supportcrypto=1"),
      btAnnounce.getAnnounceUrl());

  btAnnounce.announceSuccess();
}

void DefaultBtAnnounceTest::testProcessAnnounceResponse_malformed()
{
  try {
    std::string res = "i123e";
    DefaultBtAnnounce(dctx_.get(), option_)
        .processAnnounceResponse(
            reinterpret_cast<const unsigned char*>(res.c_str()), res.size());
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

void DefaultBtAnnounceTest::testProcessAnnounceResponse_failureReason()
{
  try {
    std::string res = "d14:failure reason11:hello worlde";
    DefaultBtAnnounce(dctx_.get(), option_)
        .processAnnounceResponse(
            reinterpret_cast<const unsigned char*>(res.c_str()), res.size());
    FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
    std::cerr << e.stackTrace() << std::endl;
  }
}

void DefaultBtAnnounceTest::testProcessAnnounceResponse()
{
  std::string res = "d"
                    "15:warning message11:hello world"
                    "10:tracker id3:foo"
                    "8:intervali3000e"
                    "12:min intervali1800e"
                    "8:completei100e"
                    "10:incompletei200e";
  res += "5:peers6:";
  res += fromHex("c0a800011ae1");
  res += "6:peers618:";
  res += fromHex("100210354527354678541237324732171ae1");
  res += "e";

  DefaultBtAnnounce an(dctx_.get(), option_);
  an.setPeerStorage(peerStorage_);
  an.setBtRuntime(btRuntime_);
  an.processAnnounceResponse(
      reinterpret_cast<const unsigned char*>(res.c_str()), res.size());
  REQUIRE_EQ(std::string("foo"), an.getTrackerID());
  REQUIRE_EQ((int64_t)3000, an.getInterval().count());
  REQUIRE_EQ((int64_t)1800, an.getMinInterval().count());
  REQUIRE_EQ(100, an.getComplete());
  REQUIRE_EQ(200, an.getIncomplete());
  REQUIRE_EQ((size_t)2, peerStorage_->getUnusedPeers().size());
  std::shared_ptr<Peer> peer = peerStorage_->getUnusedPeers()[0];
  REQUIRE_EQ(std::string("192.168.0.1"), peer->getIPAddress());
  peer = peerStorage_->getUnusedPeers()[1];
  REQUIRE_EQ(std::string("1002:1035:4527:3546:7854:1237:3247:3217"),
                       peer->getIPAddress());
}

void DefaultBtAnnounceTest::testProcessUDPTrackerResponse()
{
  std::shared_ptr<UDPTrackerRequest> req(new UDPTrackerRequest());
  req->action = UDPT_ACT_ANNOUNCE;
  std::shared_ptr<UDPTrackerReply> reply(new UDPTrackerReply());
  reply->interval = 1800;
  reply->leechers = 200;
  reply->seeders = 100;
  for (int i = 0; i < 2; ++i) {
    reply->peers.push_back(
        std::make_pair("192.168.0." + util::uitos(i + 1), 6890 + i));
  }
  req->reply = reply;
  DefaultBtAnnounce an(dctx_.get(), option_);
  an.setPeerStorage(peerStorage_);
  an.setBtRuntime(btRuntime_);
  an.processUDPTrackerResponse(req);
  REQUIRE_EQ((int64_t)1800, an.getInterval().count());
  REQUIRE_EQ((int64_t)1800, an.getMinInterval().count());
  REQUIRE_EQ(100, an.getComplete());
  REQUIRE_EQ(200, an.getIncomplete());
  REQUIRE_EQ((size_t)2, peerStorage_->getUnusedPeers().size());
  for (int i = 0; i < 2; ++i) {
    std::shared_ptr<Peer> peer;
    peer = peerStorage_->getUnusedPeers()[i];
    REQUIRE_EQ("192.168.0." + util::uitos(i + 1),
                         peer->getIPAddress());
    REQUIRE_EQ((uint16_t)(6890 + i), peer->getPort());
  }
}

} // namespace aria2
