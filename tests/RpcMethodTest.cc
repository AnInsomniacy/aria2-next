#include "RpcMethod.h"

#include <cppunit/extensions/HelperMacros.h>

#include "DownloadEngine.h"
#include "SelectEventPoll.h"
#include "Option.h"
#include "RequestGroupMan.h"
#include "RequestGroup.h"
#include "RpcMethodImpl.h"
#include "OptionParser.h"
#include "OptionHandler.h"
#include "RpcRequest.h"
#include "RpcResponse.h"
#include "prefs.h"
#include "TestUtil.h"
#include "DownloadContext.h"
#include "FeatureConfig.h"
#include "util.h"
#include "array_fun.h"
#include "download_helper.h"
#include "FileEntry.h"
#include "Piece.h"
#include "PieceStorage.h"
#include "RpcMethodFactory.h"
#include "Ed2kAttribute.h"
#include "ed2k_hash.h"
#include "ed2k_link.h"
#include "ed2k_search.h"
#ifdef ENABLE_BITTORRENT
#  include "LibtorrentAttribute.h"
#  include "bittorrent_helper.h"
#endif // ENABLE_BITTORRENT

namespace aria2 {

namespace rpc {

class RpcMethodTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RpcMethodTest);
  CPPUNIT_TEST(testAuthorize);
  CPPUNIT_TEST(testAddUri);
  CPPUNIT_TEST(testAddUri_acceptsJsonBoolOption);
  CPPUNIT_TEST(testAddUri_withoutUri);
  CPPUNIT_TEST(testAddUri_notUri);
  CPPUNIT_TEST(testAddUri_withBadOption);
  CPPUNIT_TEST(testAddUri_withPosition);
  CPPUNIT_TEST(testAddUri_withBadPosition);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testAddTorrent);
  CPPUNIT_TEST(testAddTorrent_withoutTorrent);
  CPPUNIT_TEST(testAddTorrent_notBase64Torrent);
  CPPUNIT_TEST(testAddTorrent_withPosition);
  CPPUNIT_TEST(testAddTorrentRejectsDuplicateInfoHash);
#endif // ENABLE_BITTORRENT
  CPPUNIT_TEST(testGetOption);
  CPPUNIT_TEST(testChangeOption);
  CPPUNIT_TEST(testChangeOption_withBadOption);
  CPPUNIT_TEST(testChangeOption_withNotAllowedOption);
  CPPUNIT_TEST(testChangeOption_withoutGid);
  CPPUNIT_TEST(testChangeGlobalOption);
  CPPUNIT_TEST(testChangeGlobalOption_withBadOption);
  CPPUNIT_TEST(testChangeGlobalOption_withNotAllowedOption);
  CPPUNIT_TEST(testTellStatus_withoutGid);
  CPPUNIT_TEST(testTellWaiting);
  CPPUNIT_TEST(testTellWaiting_fail);
  CPPUNIT_TEST(testGetVersion);
  CPPUNIT_TEST(testNoSuchMethod);
  CPPUNIT_TEST(testEd2kSearchResults);
  CPPUNIT_TEST(testEd2kSearchResultLinkCreatesDownload);
  CPPUNIT_TEST(testGatherStoppedDownload);
  CPPUNIT_TEST(testGatherProgressEd2kStatus);
  CPPUNIT_TEST(testGatherProgressEd2kVisibleLengthStaysStableWhenPaused);
  CPPUNIT_TEST(testGatherStoppedDownloadEd2kVisibleLength);
#ifdef ENABLE_BITTORRENT
  CPPUNIT_TEST(testGatherProgressLibtorrentStatus);
  CPPUNIT_TEST(testGatherProgressLibtorrentMetadataDownloading);
  CPPUNIT_TEST(testGatherProgressLibtorrentMetadataReady);
  CPPUNIT_TEST(testGatherProgressLibtorrentUsesResumeFallbackWhileChecking);
  CPPUNIT_TEST(testGatherProgressLibtorrentUsesResumeFallbackBeforeStatus);
  CPPUNIT_TEST(testGatherProgressLibtorrentUsesResumeFallbackForEmptyLiveStatus);
  CPPUNIT_TEST(testGatherProgressLibtorrentFilesWithoutNativeBitfield);
  CPPUNIT_TEST(testGatherProgressLibtorrentAnnounceList);
  CPPUNIT_TEST(testGetPeersLibtorrentPreservesPeerId);
  CPPUNIT_TEST(testChangeOptionLibtorrentSelectFile);
  CPPUNIT_TEST(testUnpauseLibtorrentMagnetAfterMetadataSelectFile);
#endif // ENABLE_BITTORRENT
  CPPUNIT_TEST(testGatherProgressCommon);
  CPPUNIT_TEST(testGatherProgressCommonSeparatesInFlightProgress);
  CPPUNIT_TEST(testChangePosition);
  CPPUNIT_TEST(testChangePosition_fail);
  CPPUNIT_TEST(testGetSessionInfo);
  CPPUNIT_TEST(testChangeUri);
  CPPUNIT_TEST(testChangeUri_fail);
  CPPUNIT_TEST(testPause);
  CPPUNIT_TEST(testSystemMulticall);
  CPPUNIT_TEST(testSystemMulticall_fail);
  CPPUNIT_TEST(testSystemListMethods);
  CPPUNIT_TEST(testSystemListNotifications);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<DownloadEngine> e_;
  std::shared_ptr<Option> option_;

public:
  void setUp()
  {
    option_ = std::make_shared<Option>();
    option_->put(PREF_DIR, A2_TEST_OUT_DIR "/aria2_RpcMethodTest");
    option_->put(PREF_PIECE_LENGTH, "1048576");
    option_->put(PREF_MAX_DOWNLOAD_RESULT, "10");
    File(option_->get(PREF_DIR)).mkdirs();
    e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
    e_->setOption(option_.get());
    e_->setRequestGroupMan(make_unique<RequestGroupMan>(
        std::vector<std::shared_ptr<RequestGroup>>{}, 1, option_.get()));
  }

  void testAuthorize();
  void testAddUri();
  void testAddUri_acceptsJsonBoolOption();
  void testAddUri_withoutUri();
  void testAddUri_notUri();
  void testAddUri_withBadOption();
  void testAddUri_withPosition();
  void testAddUri_withBadPosition();
#ifdef ENABLE_BITTORRENT
  void testAddTorrent();
  void testAddTorrent_withoutTorrent();
  void testAddTorrent_notBase64Torrent();
  void testAddTorrent_withPosition();
  void testAddTorrentRejectsDuplicateInfoHash();
#endif // ENABLE_BITTORRENT
  void testGetOption();
  void testChangeOption();
  void testChangeOption_withBadOption();
  void testChangeOption_withNotAllowedOption();
  void testChangeOption_withoutGid();
  void testChangeGlobalOption();
  void testChangeGlobalOption_withBadOption();
  void testChangeGlobalOption_withNotAllowedOption();
  void testTellStatus_withoutGid();
  void testTellWaiting();
  void testTellWaiting_fail();
  void testGetVersion();
  void testNoSuchMethod();
  void testEd2kSearchResults();
  void testEd2kSearchResultLinkCreatesDownload();
  void testGatherStoppedDownload();
  void testGatherProgressEd2kStatus();
  void testGatherProgressEd2kVisibleLengthStaysStableWhenPaused();
  void testGatherStoppedDownloadEd2kVisibleLength();
#ifdef ENABLE_BITTORRENT
  void testGatherProgressLibtorrentStatus();
  void testGatherProgressLibtorrentMetadataDownloading();
  void testGatherProgressLibtorrentMetadataReady();
  void testGatherProgressLibtorrentUsesResumeFallbackWhileChecking();
  void testGatherProgressLibtorrentUsesResumeFallbackBeforeStatus();
  void testGatherProgressLibtorrentUsesResumeFallbackForEmptyLiveStatus();
  void testGatherProgressLibtorrentFilesWithoutNativeBitfield();
  void testGatherProgressLibtorrentAnnounceList();
  void testGetPeersLibtorrentPreservesPeerId();
  void testChangeOptionLibtorrentSelectFile();
  void testUnpauseLibtorrentMagnetAfterMetadataSelectFile();
#endif // ENABLE_BITTORRENT
  void testGatherProgressCommon();
  void testGatherProgressCommonSeparatesInFlightProgress();
  void testChangePosition();
  void testChangePosition_fail();
  void testGetSessionInfo();
  void testChangeUri();
  void testChangeUri_fail();
  void testPause();
  void testSystemMulticall();
  void testSystemMulticall_fail();
  void testSystemListMethods();
  void testSystemListNotifications();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RpcMethodTest);

namespace {
std::string getString(const Dict* dict, const std::string& key)
{
  return downcast<String>(dict->get(key))->s();
}
} // namespace

namespace {
RpcRequest createReq(std::string methodName)
{
  return {std::move(methodName), List::g()};
}
} // namespace

void RpcMethodTest::testAuthorize()
{
  // Select RPC method which takes non-string parameter to make sure
  // that token: prefixed parameter is stripped before the call.
  TellActiveRpcMethod m;
  // no secret token set and no token: prefixed parameter is given
  {
    auto req = createReq(TellActiveRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  // no secret token set and token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    req.params->append("token:foo");
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  e_->getOption()->put(PREF_RPC_SECRET, "foo");
  // secret token set and no token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
  }
  // secret token set and token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    req.params->append("token:foo");
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  // secret token set and bad token: prefixed parameter is given
  {
    auto req = createReq(GetVersionRpcMethod::getMethodName());
    req.params->append("token:foo2");
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(1, res.code);
  }
}

void RpcMethodTest::testAddUri()
{
  AddUriRpcMethod m;
  {
    auto req = createReq(AddUriRpcMethod::getMethodName());
    auto urisParam = List::g();
    urisParam->append("http://localhost/");
    req.params->append(std::move(urisParam));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    const RequestGroupList& rgs = e_->getRequestGroupMan()->getReservedGroups();
    CPPUNIT_ASSERT_EQUAL((size_t)1, rgs.size());
    CPPUNIT_ASSERT_EQUAL(std::string("http://localhost/"),
                         (*rgs.begin())
                             ->getDownloadContext()
                             ->getFirstFileEntry()
                             ->getRemainingUris()
                             .front());
  }
  {
    auto req = createReq(AddUriRpcMethod::getMethodName());
    auto urisParam = List::g();
    urisParam->append("http://localhost/");
    req.params->append(std::move(urisParam));
    // with options
    auto opt = Dict::g();
    opt->put(PREF_DIR->k, "/sink");
    req.params->append(std::move(opt));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    a2_gid_t gid;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(gid, downcast<String>(res.param)->s().c_str()));
    CPPUNIT_ASSERT_EQUAL(std::string("/sink"),
                         findReservedGroup(e_->getRequestGroupMan().get(), gid)
                             ->getOption()
                             ->get(PREF_DIR));
  }
}

void RpcMethodTest::testAddUri_acceptsJsonBoolOption()
{
  option_->put(PREF_ENABLE_RPC, A2_V_TRUE);

  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("http://localhost/");
  req.params->append(std::move(urisParam));
  auto opt = Dict::g();
  opt->put(PREF_PAUSE->k, Bool::gTrue());
  req.params->append(std::move(opt));

  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const RequestGroupList& rgs = e_->getRequestGroupMan()->getReservedGroups();
  CPPUNIT_ASSERT_EQUAL((size_t)1, rgs.size());
  CPPUNIT_ASSERT((*rgs.begin())->isPauseRequested());
}

void RpcMethodTest::testAddUri_withoutUri()
{
  AddUriRpcMethod m;
  auto res = m.execute(createReq(AddUriRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddUri_notUri()
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("not uri");
  req.params->append(std::move(urisParam));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddUri_withBadOption()
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("http://localhost");
  req.params->append(std::move(urisParam));
  auto opt = Dict::g();
  opt->put(PREF_FILE_ALLOCATION->k, "badvalue");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddUri_withPosition()
{
  AddUriRpcMethod m;
  auto req1 = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam1 = List::g();
  urisParam1->append("http://uri1");
  req1.params->append(std::move(urisParam1));
  auto res1 = m.execute(std::move(req1), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res1.code);

  auto req2 = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam2 = List::g();
  urisParam2->append("http://uri2");
  req2.params->append(std::move(urisParam2));
  req2.params->append(Dict::g());
  req2.params->append(Integer::g(0));
  m.execute(std::move(req2), e_.get());

  std::string uri = getReservedGroup(e_->getRequestGroupMan().get(), 0)
                        ->getDownloadContext()
                        ->getFirstFileEntry()
                        ->getRemainingUris()[0];

  CPPUNIT_ASSERT_EQUAL(std::string("http://uri2"), uri);
}

void RpcMethodTest::testAddUri_withBadPosition()
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append("http://localhost/");
  req.params->append(std::move(urisParam));
  req.params->append(Dict::g());
  req.params->append(Integer::g(-1));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

#ifdef ENABLE_BITTORRENT
namespace {
RpcRequest createAddTorrentReq()
{
  auto req = createReq(AddTorrentRpcMethod::getMethodName());
  req.params->append(readFile(A2_TEST_DIR "/single.torrent"));
  auto uris = List::g();
  uris->append("http://localhost/aria2-0.8.2.tar.bz2");
  req.params->append(std::move(uris));
  return req;
}

RpcRequest createAddTorrentReq(const std::string& torrentFile)
{
  auto req = createReq(AddTorrentRpcMethod::getMethodName());
  req.params->append(readFile(torrentFile));
  auto uris = List::g();
  uris->append("http://localhost/aria2-0.8.2.tar.bz2");
  req.params->append(std::move(uris));
  return req;
}
} // namespace

void RpcMethodTest::testAddTorrent()
{
  File(e_->getOption()->get(PREF_DIR) +
       "/0a3893293e27ac0490424c06de4d09242215f0a6.torrent")
      .remove();
  AddTorrentRpcMethod m;
  {
    // Saving upload metadata is disabled by option.
    auto res = m.execute(createAddTorrentReq(), e_.get());
    CPPUNIT_ASSERT(!File(e_->getOption()->get(PREF_DIR) +
                         "/0a3893293e27ac0490424c06de4d09242215f0a6.torrent")
                        .exists());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    CPPUNIT_ASSERT_EQUAL(sizeof(a2_gid_t) * 2,
                         downcast<String>(res.param)->s().size());
  }
  e_->getOption()->put(PREF_RPC_SAVE_UPLOAD_METADATA, A2_V_TRUE);
  {
    auto res =
        m.execute(createAddTorrentReq(A2_TEST_DIR "/url-list-singleFile.torrent"),
                  e_.get());
    CPPUNIT_ASSERT(File(e_->getOption()->get(PREF_DIR) +
                        "/0df386cbc3088659f806d1221752a917a1d7fedd.torrent")
                       .exists());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    a2_gid_t gid;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(gid, downcast<String>(res.param)->s().c_str()));

    auto group = findReservedGroup(e_->getRequestGroupMan().get(), gid);
    CPPUNIT_ASSERT(group);
    auto dctx = group->getDownloadContext();
    CPPUNIT_ASSERT(dctx->hasAttribute(CTX_ATTR_LIBTORRENT));
    auto attrs = getLibtorrentAttrs(dctx);
    CPPUNIT_ASSERT_EQUAL((size_t)1, attrs->webSeedUris.size());
    CPPUNIT_ASSERT_EQUAL(std::string("http://localhost/aria2-0.8.2.tar.bz2"),
                         attrs->webSeedUris[0]);
  }
  {
    auto req = createAddTorrentReq(A2_TEST_DIR "/url-list-multiFile.torrent");
    // with options
    std::string dir = A2_TEST_OUT_DIR "/aria2_RpcMethodTest_testAddTorrent";
    File(dir).mkdirs();
    auto opt = Dict::g();
    opt->put(PREF_DIR->k, dir);
    File(dir + "/c9b87dea1eb0c36e917272dd5220ddf8a47f9654.torrent").remove();
    req.params->append(std::move(opt));

    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
    a2_gid_t gid;
    CPPUNIT_ASSERT_EQUAL(
        0, GroupId::toNumericId(gid, downcast<String>(res.param)->s().c_str()));
    auto group = findReservedGroup(e_->getRequestGroupMan().get(), gid);
    CPPUNIT_ASSERT(group->getDownloadContext()->hasAttribute(
        CTX_ATTR_LIBTORRENT));
    CPPUNIT_ASSERT(
        File(dir + "/c9b87dea1eb0c36e917272dd5220ddf8a47f9654.torrent")
            .exists());
  }
}

void RpcMethodTest::testAddTorrent_withoutTorrent()
{
  AddTorrentRpcMethod m;
  auto res =
      m.execute(createReq(AddTorrentRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddTorrent_notBase64Torrent()
{
  AddTorrentRpcMethod m;
  auto req = createReq(AddTorrentRpcMethod::getMethodName());
  req.params->append("not torrent");
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testAddTorrent_withPosition()
{
  AddTorrentRpcMethod m;
  auto req1 = createReq(AddTorrentRpcMethod::getMethodName());
  req1.params->append(readFile(A2_TEST_DIR "/test.torrent"));
  req1.params->append(List::g());
  req1.params->append(Dict::g());
  auto res1 = m.execute(std::move(req1), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res1.code);

  auto req2 = createReq(AddTorrentRpcMethod::getMethodName());
  req2.params->append(readFile(A2_TEST_DIR "/single.torrent"));
  req2.params->append(List::g());
  req2.params->append(Dict::g());
  req2.params->append(Integer::g(0));
  m.execute(std::move(req2), e_.get());

  CPPUNIT_ASSERT_EQUAL((size_t)1,
                       getReservedGroup(e_->getRequestGroupMan().get(), 0)
                           ->getDownloadContext()
                           ->getFileEntries()
                           .size());
}

void RpcMethodTest::testAddTorrentRejectsDuplicateInfoHash()
{
  AddTorrentRpcMethod m;
  auto res1 = m.execute(createAddTorrentReq(), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res1.code);

  auto res2 = m.execute(createAddTorrentReq(), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res2.code);
  auto error = downcast<Dict>(res2.param);
  CPPUNIT_ASSERT(error);
  auto message = downcast<String>(error->get("faultString"));
  CPPUNIT_ASSERT(message);
  CPPUNIT_ASSERT(message->s().find("Already downloading this torrent") !=
                 std::string::npos);
}

#endif // ENABLE_BITTORRENT


void RpcMethodTest::testGetOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->getOption()->put(PREF_DIR, "alpha");
  e_->getRequestGroupMan()->addReservedGroup(group);
  auto dr = createDownloadResult(error_code::FINISHED, "http://host/fin");
  dr->option->put(PREF_DIR, "bravo");
  e_->getRequestGroupMan()->addDownloadResult(dr);

  GetOptionRpcMethod m;
  auto req = createReq(GetOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const Dict* resopt = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL(std::string("alpha"),
                       downcast<String>(resopt->get(PREF_DIR->k))->s());

  req = createReq(GetOptionRpcMethod::getMethodName());
  req.params->append(dr->gid->toHex());
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resopt = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL(std::string("bravo"),
                       downcast<String>(resopt->get(PREF_DIR->k))->s());
  // Invalid GID
  req = createReq(GetOptionRpcMethod::getMethodName());
  req.params->append(GroupId::create()->toHex());
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testChangeOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_MAX_DOWNLOAD_LIMIT->k, "100K");
#ifdef ENABLE_BITTORRENT
  opt->put(PREF_BT_MAX_PEERS->k, "100");
  opt->put(PREF_MAX_UPLOAD_LIMIT->k, "50K");
#endif // ENABLE_BITTORRENT
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());

  auto option = group->getOption();

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL((int)100_k, group->getMaxDownloadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       option->get(PREF_MAX_DOWNLOAD_LIMIT));
#ifdef ENABLE_BITTORRENT
  CPPUNIT_ASSERT_EQUAL(std::string("100"), option->get(PREF_BT_MAX_PEERS));

  CPPUNIT_ASSERT_EQUAL((int)50_k, group->getMaxUploadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("51200"),
                       option->get(PREF_MAX_UPLOAD_LIMIT));
#endif // ENABLE_BITTORRENT
}

void RpcMethodTest::testChangeOption_withBadOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_MAX_DOWNLOAD_LIMIT->k, "badvalue");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testChangeOption_withNotAllowedOption()
{
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "100K");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  // The unacceptable options are just ignored.
  CPPUNIT_ASSERT_EQUAL(0, res.code);
}

void RpcMethodTest::testChangeOption_withoutGid()
{
  ChangeOptionRpcMethod m;
  auto res =
      m.execute(createReq(ChangeOptionRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testChangeGlobalOption()
{
  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "100K");
#ifdef ENABLE_BITTORRENT
  opt->put(PREF_MAX_OVERALL_UPLOAD_LIMIT->k, "50K");
#endif // ENABLE_BITTORRENT
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int)100_k, e_->getRequestGroupMan()->getMaxOverallDownloadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       e_->getOption()->get(PREF_MAX_OVERALL_DOWNLOAD_LIMIT));
#ifdef ENABLE_BITTORRENT
  CPPUNIT_ASSERT_EQUAL(
      (int)50_k, e_->getRequestGroupMan()->getMaxOverallUploadSpeedLimit());
  CPPUNIT_ASSERT_EQUAL(std::string("51200"),
                       e_->getOption()->get(PREF_MAX_OVERALL_UPLOAD_LIMIT));
#endif // ENABLE_BITTORRENT
}

void RpcMethodTest::testChangeGlobalOption_withBadOption()
{
  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_MAX_OVERALL_DOWNLOAD_LIMIT->k, "badvalue");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testChangeGlobalOption_withNotAllowedOption()
{
  ChangeGlobalOptionRpcMethod m;
  auto req = createReq(ChangeGlobalOptionRpcMethod::getMethodName());
  auto opt = Dict::g();
  opt->put(PREF_ENABLE_RPC->k, "100K");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());
  // The unacceptable options are just ignored.
  CPPUNIT_ASSERT_EQUAL(0, res.code);
}

void RpcMethodTest::testNoSuchMethod()
{
  NoSuchMethodRpcMethod m;
  auto res = m.execute(createReq("make.hamburger"), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("No such method: make.hamburger"),
                       getString(downcast<Dict>(res.param), "faultString"));
}

void RpcMethodTest::testEd2kSearchResults()
{
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, 0, A2_TEST_OUT_DIR "/ed2k-rpc-search");
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->searchActive = true;
  attrs->searchMoreResults = true;
  attrs->searchQuery.keyword = "movie";
  ed2k::SearchResultEntry entry;
  entry.hash = std::string(ed2k::HASH_LENGTH, '\x42');
  entry.name = "movie.mkv";
  entry.size = 123456789;
  entry.sourceCount = 8;
  entry.completeSourceCount = 5;
  entry.fileType = "Video";
  entry.extension = "mkv";
  entry.mediaTitle = "Movie";
  entry.mediaBitrate = 320;
  entry.sourceNetwork = "server|kad";
  ed2k::Link link;
  link.type = ed2k::LinkType::FILE;
  link.name = entry.name;
  link.size = entry.size;
  link.hash = entry.hash;
  entry.ed2kLink = ed2k::toFileLink(link);
  attrs->searchResults.push_back(entry);
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);

  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  const auto gid = group->getGID();
  e_->getRequestGroupMan()->addReservedGroup(group);

  GetEd2kSearchResultsRpcMethod m;
  auto req = createReq(GetEd2kSearchResultsRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(gid));
  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const auto body = downcast<Dict>(res.param);
  CPPUNIT_ASSERT(body);
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(gid), getString(body, "gid"));
  const auto moreResults = downcast<Bool>(body->get("moreResults"));
  CPPUNIT_ASSERT(moreResults);
  CPPUNIT_ASSERT(moreResults->val());
  const auto results = downcast<List>(body->get("results"));
  CPPUNIT_ASSERT(results);
  CPPUNIT_ASSERT_EQUAL((size_t)1, results->size());
  const auto result = downcast<Dict>(results->get(0));
  CPPUNIT_ASSERT(result);
  CPPUNIT_ASSERT_EQUAL(std::string("42424242424242424242424242424242"),
                       getString(result, "hash"));
  CPPUNIT_ASSERT_EQUAL(std::string("movie.mkv"), getString(result, "name"));
  CPPUNIT_ASSERT_EQUAL(std::string("123456789"),
                       getString(result, "length"));
  CPPUNIT_ASSERT_EQUAL(std::string("8"), getString(result, "sourceCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("5"),
                       getString(result, "completeSourceCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("server|kad"),
                       getString(result, "sourceNetwork"));
  CPPUNIT_ASSERT_EQUAL(entry.ed2kLink, getString(result, "ed2kLink"));
}

void RpcMethodTest::testEd2kSearchResultLinkCreatesDownload()
{
  ed2k::Link link;
  link.type = ed2k::LinkType::FILE;
  link.name = "movie.mkv";
  link.size = 123456789;
  link.hash = std::string(ed2k::HASH_LENGTH, '\x42');

  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append(ed2k::toFileLink(link));
  req.params->append(std::move(urisParam));
  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const auto& groups = e_->getRequestGroupMan()->getReservedGroups();
  CPPUNIT_ASSERT_EQUAL((size_t)1, groups.size());
  auto attrs =
      getEd2kAttrs((*groups.begin())->getDownloadContext());
  CPPUNIT_ASSERT(attrs);
  CPPUNIT_ASSERT_EQUAL(link.hash, attrs->link.hash);
  CPPUNIT_ASSERT_EQUAL(link.name, attrs->link.name);
  CPPUNIT_ASSERT_EQUAL(link.size, attrs->link.size);
}

void RpcMethodTest::testTellStatus_withoutGid()
{
  TellStatusRpcMethod m;
  auto res =
      m.execute(createReq(TellStatusRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

namespace {
void addUri(const std::string& uri, const std::shared_ptr<DownloadEngine>& e)
{
  AddUriRpcMethod m;
  auto req = createReq(AddUriRpcMethod::getMethodName());
  auto urisParam = List::g();
  urisParam->append(uri);
  req.params->append(std::move(urisParam));
  CPPUNIT_ASSERT_EQUAL(0, m.execute(std::move(req), e.get()).code);
}
} // namespace

#ifdef ENABLE_BITTORRENT
namespace {
void addTorrent(const std::string& torrentFile,
                const std::shared_ptr<DownloadEngine>& e)
{
  AddTorrentRpcMethod m;
  auto req = createReq(AddTorrentRpcMethod::getMethodName());
  req.params->append(readFile(torrentFile));
  auto res = m.execute(std::move(req), e.get());
}
} // namespace
#endif // ENABLE_BITTORRENT

void RpcMethodTest::testTellWaiting()
{
  addUri("http://1/", e_);
  addUri("http://2/", e_);
  addUri("http://3/", e_);
#ifdef ENABLE_BITTORRENT
  addTorrent(A2_TEST_DIR "/single.torrent", e_);
#else  // !ENABLE_BITTORRENT
  addUri("http://4/", e_);
#endif // !ENABLE_BITTORRENT
  auto& rgman = e_->getRequestGroupMan();
  TellWaitingRpcMethod m;
  auto req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(2));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const List* resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)2, resParams->size());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 1)->getGID()),
      getString(downcast<Dict>(resParams->get(0)), "gid"));
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 2)->getGID()),
      getString(downcast<Dict>(resParams->get(1)), "gid"));
  // waiting.size() == offset+num
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(3));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());
  // waiting.size() < offset+num
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(4));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());

  // offset = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(INT32_MAX));
  req.params->append(Integer::g(1));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());
  // num = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(1));
  req.params->append(Integer::g(INT32_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)3, resParams->size());
  // offset=INT32_MAX and num = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(INT32_MAX));
  req.params->append(Integer::g(INT32_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());
  // offset=INT32_MIN and num = INT32_MAX
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(INT32_MIN));
  req.params->append(Integer::g(INT32_MAX));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());

  // negative offset
  req = createReq(TellWaitingRpcMethod::getMethodName());
  req.params->append(Integer::g(-1));
  req.params->append(Integer::g(2));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)2, resParams->size());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 3)->getGID()),
      getString(downcast<Dict>(resParams->get(0)), "gid"));
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 2)->getGID()),
      getString(downcast<Dict>(resParams->get(1)), "gid"));
  // negative offset and size < num
  req = RpcRequest(TellWaitingRpcMethod::getMethodName(), List::g());
  req.params->append(Integer::g(-1));
  req.params->append(Integer::g(100));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)4, resParams->size());
  // negative offset and normalized offset < 0
  req = RpcRequest(TellWaitingRpcMethod::getMethodName(), List::g());
  req.params->append(Integer::g(-5));
  req.params->append(Integer::g(100));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)0, resParams->size());
  // negative offset and normalized offset == 0
  req = RpcRequest(TellWaitingRpcMethod::getMethodName(), List::g());
  req.params->append(Integer::g(-4));
  req.params->append(Integer::g(100));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)1, resParams->size());
}

void RpcMethodTest::testTellWaiting_fail()
{
  TellWaitingRpcMethod m;
  auto res =
      m.execute(createReq(TellWaitingRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testGetVersion()
{
  GetVersionRpcMethod m;
  auto res =
      m.execute(createReq(GetVersionRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const Dict* resParams = downcast<Dict>(res.param);
  CPPUNIT_ASSERT_EQUAL(std::string(PACKAGE_VERSION),
                       getString(resParams, "version"));
  const List* featureList = downcast<List>(resParams->get("enabledFeatures"));
  std::string features;
  for (auto i = featureList->begin(); i != featureList->end(); ++i) {
    const String* s = downcast<String>(*i);
    features += s->s();
    features += ", ";
  }
  CPPUNIT_ASSERT_EQUAL(featureSummary() + ", ", features);
}

void RpcMethodTest::testGatherStoppedDownload()
{
  std::vector<std::shared_ptr<FileEntry>> fileEntries;
  std::vector<a2_gid_t> followedBy;
  followedBy.push_back(3);
  followedBy.push_back(4);
  auto d = std::make_shared<DownloadResult>();
  d->gid = GroupId::create();
  d->fileEntries = fileEntries;
  d->inMemoryDownload = false;
  d->sessionDownloadLength = UINT64_MAX;
  d->sessionTime = 1_s;
  d->result = error_code::FINISHED;
  d->followedBy = followedBy;
  d->following = 1;
  d->belongsTo = 2;
  auto entry = Dict::g();
  std::vector<std::string> keys;
  gatherStoppedDownload(entry.get(), d, keys);

  const List* followedByRes = downcast<List>(entry->get("followedBy"));
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(3),
                       downcast<String>(followedByRes->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(4),
                       downcast<String>(followedByRes->get(1))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(1),
                       downcast<String>(entry->get("following"))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(2),
                       downcast<String>(entry->get("belongsTo"))->s());

  keys.push_back("gid");

  entry = Dict::g();
  gatherStoppedDownload(entry.get(), d, keys);
  CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
  CPPUNIT_ASSERT(entry->containsKey("gid"));
}

void RpcMethodTest::testGatherProgressEd2kStatus()
{
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, 1024, A2_TEST_OUT_DIR "/ed2k-status.bin");
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.type = ed2k::LinkType::FILE;
  attrs->link.name = "ed2k-status.bin";
  attrs->link.size = 1024;
  attrs->link.hash = std::string(ed2k::HASH_LENGTH, '\x42');
  attrs->searchActive = true;
  attrs->searchMoreResults = true;
  attrs->searchResults.resize(2);
  attrs->pieceHashes.push_back(std::string(ed2k::HASH_LENGTH, '\x11'));
  attrs->aichRootHash = std::string(ed2k::AICH_HASH_LENGTH, '\x22');

  ed2k::ServerState server;
  server.endpoint.host = "203.0.113.10";
  server.endpoint.port = 4661;
  server.name = "server";
  server.connected = true;
  server.handshakeCompleted = true;
  server.highId = true;
  server.users = 10;
  server.files = 20;
  attrs->serverStates.push_back(server);

  ed2k::PeerState peer;
  peer.endpoint.host = "203.0.113.20";
  peer.endpoint.port = 4662;
  peer.sourceFlags = ed2k::PEER_SOURCE_SERVER;
  peer.queued = true;
  peer.queueRank = 7;
  attrs->peerStates.push_back(peer);

  ed2k::PeerState lowIdPeer;
  lowIdPeer.endpoint.host = "120.0.0.42";
  lowIdPeer.endpoint.port = 4662;
  lowIdPeer.lowId = true;
  lowIdPeer.callbackRequested = true;
  lowIdPeer.lowIdCallbackState = ed2k::LowIdCallbackState::REQUESTED;
  attrs->peerStates.push_back(lowIdPeer);

  attrs->kadRoutingTable = std::make_shared<ed2k::KadRoutingTable>(
      std::string(ed2k::HASH_LENGTH, '\x33'));
  ed2k::KadContact kadNode;
  kadNode.id = std::string(ed2k::HASH_LENGTH, '\x34');
  kadNode.host = "203.0.113.31";
  kadNode.udpPort = 4672;
  kadNode.tcpPort = 4662;
  kadNode.version = 8;
  attrs->kadRoutingTable->nodeSeen(kadNode, 100);
  ed2k::Endpoint router;
  router.host = "203.0.113.30";
  router.port = 4672;
  attrs->kadRoutingTable->addRouterNode(router);
  attrs->kadObservedAddresses.push_back("198.51.100.1");
  attrs->kadFirewalled = false;

  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  group->setRequestGroupMan(e_->getRequestGroupMan().get());

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {"ed2k"});

  CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
  auto ed2kStatus = downcast<Dict>(entry->get("ed2k"));
  CPPUNIT_ASSERT(ed2kStatus);
  CPPUNIT_ASSERT_EQUAL(std::string("42424242424242424242424242424242"),
                       getString(ed2kStatus, "hash"));
  CPPUNIT_ASSERT_EQUAL(std::string("ed2k-status.bin"),
                       getString(ed2kStatus, "name"));
  CPPUNIT_ASSERT_EQUAL(std::string("1024"), getString(ed2kStatus, "length"));
  CPPUNIT_ASSERT(ed2kStatus->containsKey("completedLength"));
  CPPUNIT_ASSERT(ed2kStatus->containsKey("inFlightCompletedLength"));
  CPPUNIT_ASSERT(ed2kStatus->containsKey("visibleCompletedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "completedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "inFlightCompletedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "visibleCompletedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       getString(ed2kStatus, "partHashCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("2222222222222222222222222222222222222222"),
                       getString(ed2kStatus, "aichRoot"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       getString(ed2kStatus, "serverCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       getString(ed2kStatus, "connectedServerCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("2"), getString(ed2kStatus, "peerCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       getString(ed2kStatus, "queuedPeerCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       getString(ed2kStatus, "lowIdPeerCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"),
                       getString(ed2kStatus, "callbackWaitingPeerCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"), getString(ed2kStatus, "kadNodeCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("1"), getString(ed2kStatus, "kadRouterCount"));
  CPPUNIT_ASSERT(!downcast<Bool>(ed2kStatus->get("kadFirewalled"))->val());
  CPPUNIT_ASSERT_EQUAL(std::string("2"),
                       getString(ed2kStatus, "searchResultCount"));
  CPPUNIT_ASSERT(downcast<Bool>(ed2kStatus->get("searchActive"))->val());
  CPPUNIT_ASSERT(downcast<Bool>(ed2kStatus->get("searchMoreResults"))->val());
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "sharedFileCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "uploadingPeerCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "waitingUploadPeerCount"));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "peerCreditCount"));
}

void RpcMethodTest::testGatherProgressEd2kVisibleLengthStaysStableWhenPaused()
{
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, 1024, A2_TEST_OUT_DIR "/ed2k-paused.bin");
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.type = ed2k::LinkType::FILE;
  attrs->link.name = "ed2k-paused.bin";
  attrs->link.size = 1024;
  attrs->link.hash = std::string(ed2k::HASH_LENGTH, '\x42');
  attrs->visibleCompletedLength = 768;
  dctx->setAttribute(CTX_ATTR_ED2K, attrs);
  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  group->setRequestGroupMan(e_->getRequestGroupMan().get());

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {"ed2k"});

  auto ed2kStatus = downcast<Dict>(entry->get("ed2k"));
  CPPUNIT_ASSERT(ed2kStatus);
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "completedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       getString(ed2kStatus, "inFlightCompletedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("768"),
                       getString(ed2kStatus, "visibleCompletedLength"));
}

void RpcMethodTest::testGatherStoppedDownloadEd2kVisibleLength()
{
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link.type = ed2k::LinkType::FILE;
  attrs->link.name = "ed2k-stopped.bin";
  attrs->link.size = 1024;
  attrs->link.hash = std::string(ed2k::HASH_LENGTH, '\x42');
  attrs->visibleCompletedLength = 900;

  auto result = std::make_shared<DownloadResult>();
  result->gid = GroupId::create();
  result->totalLength = 1024;
  result->completedLength = 0;
  result->inFlightCompletedLength = 0;
  result->result = error_code::IN_PROGRESS;
  result->attrs.resize(MAX_CTX_ATTR);
  result->attrs[CTX_ATTR_ED2K] = attrs;

  auto entry = Dict::g();
  gatherStoppedDownload(entry.get(), result, {"ed2k"});

  auto ed2kStatus = downcast<Dict>(entry->get("ed2k"));
  CPPUNIT_ASSERT(ed2kStatus);
  CPPUNIT_ASSERT_EQUAL(std::string("900"),
                       getString(ed2kStatus, "visibleCompletedLength"));
}

#ifdef ENABLE_BITTORRENT
void RpcMethodTest::testGatherProgressLibtorrentStatus()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->status.hasStatus = true;
  attrs->status.totalLength = 100_k;
  attrs->status.completedLength = 99_k;
  attrs->status.uploadedLength = 7_k;
  attrs->status.downloadSpeed = 1234;
  attrs->status.uploadSpeed = 55;
  attrs->status.connections = 8;
  attrs->status.seeders = 3;
  attrs->status.infoHash.assign(20, '\x01');
  attrs->status.name = "torrent.bin";
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  group->initPieceStorage();
  group->getPieceStorage()->markAllPiecesDone();

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {});

  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       getString(entry.get(), "totalLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("101376"),
                       getString(entry.get(), "completedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("1234"),
                       getString(entry.get(), "downloadSpeed"));
  CPPUNIT_ASSERT_EQUAL(std::string("55"), getString(entry.get(), "uploadSpeed"));
  CPPUNIT_ASSERT_EQUAL(std::string("7168"), getString(entry.get(), "uploadLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("8"), getString(entry.get(), "connections"));
  CPPUNIT_ASSERT_EQUAL(util::toHex(std::string(20, '\x01')),
                       getString(entry.get(), "infoHash"));
  CPPUNIT_ASSERT(entry->containsKey("bittorrent"));
  auto btDict = downcast<Dict>(entry->get("bittorrent"));
  auto infoDict = downcast<Dict>(btDict->get("info"));
  CPPUNIT_ASSERT_EQUAL(std::string("torrent.bin"),
                       downcast<String>(infoDict->get("name"))->s());
}

void RpcMethodTest::testGatherProgressLibtorrentMetadataDownloading()
{
  auto dctx = std::make_shared<DownloadContext>(0, 0, "magnet");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET,
      "magnet:?xt=urn:btih:0101010101010101010101010101010101010101"
      "&dn=Display%20Name",
      "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->status.hasStatus = true;
  attrs->status.hasMetadata = false;
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {"bittorrent"});

  auto btDict = downcast<Dict>(entry->get("bittorrent"));
  CPPUNIT_ASSERT(!btDict->containsKey("info"));
  CPPUNIT_ASSERT(btDict->containsKey("metadata"));
  auto metadataDict = downcast<Dict>(btDict->get("metadata"));
  CPPUNIT_ASSERT_EQUAL(std::string("downloading"),
                       downcast<String>(metadataDict->get("state"))->s());
  CPPUNIT_ASSERT(!downcast<Bool>(metadataDict->get("hasMetadata"))->val());
  CPPUNIT_ASSERT(metadataDict->containsKey("displayName"));
  CPPUNIT_ASSERT_EQUAL(std::string("Display Name"),
                       downcast<String>(metadataDict->get("displayName"))->s());
}

void RpcMethodTest::testGatherProgressLibtorrentMetadataReady()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET,
      "magnet:?xt=urn:btih:0101010101010101010101010101010101010101", "",
      std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->status.hasStatus = true;
  attrs->status.hasMetadata = true;
  attrs->status.name = "torrent.bin";
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {"bittorrent"});

  auto btDict = downcast<Dict>(entry->get("bittorrent"));
  auto infoDict = downcast<Dict>(btDict->get("info"));
  CPPUNIT_ASSERT_EQUAL(std::string("torrent.bin"),
                       downcast<String>(infoDict->get("name"))->s());
  CPPUNIT_ASSERT(btDict->containsKey("metadata"));
  auto metadataDict = downcast<Dict>(btDict->get("metadata"));
  CPPUNIT_ASSERT_EQUAL(std::string("ready"),
                       downcast<String>(metadataDict->get("state"))->s());
  CPPUNIT_ASSERT(downcast<Bool>(metadataDict->get("hasMetadata"))->val());
  CPPUNIT_ASSERT(!metadataDict->containsKey("displayName"));
}

void RpcMethodTest::testGatherProgressLibtorrentUsesResumeFallbackWhileChecking()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->status.hasStatus = true;
  attrs->status.checking = true;
  attrs->status.totalLength = 0;
  attrs->status.completedLength = 0;
  attrs->resumeStatus.hasStatus = true;
  attrs->resumeStatus.totalLength = 100_k;
  attrs->resumeStatus.completedLength = 99_k;
  attrs->resumeStatus.bitfield.assign(13, '\xff');
  attrs->resumeStatus.name = "torrent.bin";
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {});

  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       getString(entry.get(), "totalLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("101376"),
                       getString(entry.get(), "completedLength"));
  CPPUNIT_ASSERT(entry->containsKey("bittorrent"));
}

void RpcMethodTest::testGatherProgressLibtorrentUsesResumeFallbackBeforeStatus()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->resumeStatus.hasStatus = true;
  attrs->resumeStatus.totalLength = 100_k;
  attrs->resumeStatus.completedLength = 99_k;
  attrs->resumeStatus.name = "torrent.bin";
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {});

  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       getString(entry.get(), "totalLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("101376"),
                       getString(entry.get(), "completedLength"));
}

void RpcMethodTest::testGatherProgressLibtorrentUsesResumeFallbackForEmptyLiveStatus()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->status.hasStatus = true;
  attrs->status.checking = false;
  attrs->status.totalLength = 0;
  attrs->status.completedLength = 0;
  attrs->resumeStatus.hasStatus = true;
  attrs->resumeStatus.totalLength = 100_k;
  attrs->resumeStatus.completedLength = 99_k;
  attrs->resumeStatus.bitfield.assign(13, '\xff');
  attrs->resumeStatus.name = "torrent.bin";
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {});

  CPPUNIT_ASSERT_EQUAL(std::string("102400"),
                       getString(entry.get(), "totalLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("101376"),
                       getString(entry.get(), "completedLength"));
  CPPUNIT_ASSERT(entry->containsKey("bitfield"));
}

void RpcMethodTest::testGatherProgressLibtorrentAnnounceList()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->status.hasStatus = true;
  attrs->status.name = "torrent.bin";
  attrs->trackerUris.push_back("udp://tracker1.example:6969/announce");
  attrs->trackerTiers.push_back(0);
  attrs->trackerUris.push_back("https://tracker2.example/announce");
  attrs->trackerTiers.push_back(1);
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {"bittorrent"});

  auto btDict = downcast<Dict>(entry->get("bittorrent"));
  auto announceList = downcast<List>(btDict->get("announceList"));
  CPPUNIT_ASSERT_EQUAL((size_t)2, announceList->size());
  auto tier0 = downcast<List>(announceList->get(0));
  auto tier1 = downcast<List>(announceList->get(1));
  CPPUNIT_ASSERT_EQUAL(std::string("udp://tracker1.example:6969/announce"),
                       downcast<String>(tier0->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(std::string("https://tracker2.example/announce"),
                       downcast<String>(tier1->get(0))->s());
}

void RpcMethodTest::testGatherProgressLibtorrentFilesWithoutNativeBitfield()
{
  auto dctx = std::make_shared<DownloadContext>(0, 0, "torrent");
  std::vector<std::shared_ptr<FileEntry>> entries;
  entries.push_back(std::make_shared<FileEntry>("file1.bin", 2_g, 0));
  entries.push_back(std::make_shared<FileEntry>("file2.bin", 2_g + 1_m, 2_g));
  dctx->setFileEntries(entries.begin(), entries.end());
  dctx->setPieceLength(0);

  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/test.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  attrs->status.hasStatus = true;
  attrs->status.totalLength = 4_g + 1_m;
  attrs->status.completedLength = 0;
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group, {"files"});

  const List* files = downcast<List>(entry->get("files"));
  CPPUNIT_ASSERT_EQUAL((size_t)2, files->size());
  const Dict* file1 = downcast<Dict>(files->get(0));
  const Dict* file2 = downcast<Dict>(files->get(1));
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       downcast<String>(file1->get("completedLength"))->s());
  CPPUNIT_ASSERT_EQUAL(std::string("0"),
                       downcast<String>(file2->get("completedLength"))->s());
}

void RpcMethodTest::testGetPeersLibtorrentPreservesPeerId()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 100_k, "torrent.bin");
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/single.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  LibtorrentAttribute::Peer peer;
  peer.peerId = "-qB4250-abcdefghijkl";
  peer.ip = "127.0.0.1";
  peer.port = 6881;
  peer.bitfield.assign(1, static_cast<char>(0xf0));
  attrs->peers.push_back(peer);
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  e_->getRequestGroupMan()->addRequestGroup(group);

  GetPeersRpcMethod m;
  auto req = createReq(GetPeersRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  auto peers = downcast<List>(res.param.get());
  CPPUNIT_ASSERT_EQUAL((size_t)1, peers->size());
  auto peerDict = downcast<Dict>(peers->get(0));
  CPPUNIT_ASSERT_EQUAL(std::string("-qB4250-abcdefghijkl"),
                       downcast<String>(peerDict->get("peerId"))->s());
  CPPUNIT_ASSERT_EQUAL(std::string("f0"),
                       downcast<String>(peerDict->get("bitfield"))->s());
}

void RpcMethodTest::testChangeOptionLibtorrentSelectFile()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 2_k, "torrent");
  std::vector<std::shared_ptr<FileEntry>> entries;
  entries.push_back(std::make_shared<FileEntry>("file1", 1_k, 0));
  entries.push_back(std::make_shared<FileEntry>("file2", 1_k, 1_k));
  dctx->setFileEntries(entries.begin(), entries.end());
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::TORRENT_FILE,
      A2_TEST_DIR "/test.torrent", "", std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  auto attrsPtr = attrs.get();
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeOptionRpcMethod m;
  auto req = createReq(ChangeOptionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto opt = Dict::g();
  opt->put(PREF_SELECT_FILE->k, "2");
  req.params->append(std::move(opt));
  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(std::string("2"), attrsPtr->selectedFiles);
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrsPtr->filePriorities.size());
  CPPUNIT_ASSERT_EQUAL(0, attrsPtr->filePriorities[0]);
  CPPUNIT_ASSERT_EQUAL(4, attrsPtr->filePriorities[1]);
}

void RpcMethodTest::testUnpauseLibtorrentMagnetAfterMetadataSelectFile()
{
  auto dctx = std::make_shared<DownloadContext>(1_k, 2_k, "torrent");
  std::vector<std::shared_ptr<FileEntry>> entries;
  entries.push_back(std::make_shared<FileEntry>("file1", 1_k, 0));
  entries.push_back(std::make_shared<FileEntry>("file2", 1_k, 1_k));
  dctx->setFileEntries(entries.begin(), entries.end());
  auto attrs = make_unique<LibtorrentAttribute>(
      LibtorrentAttribute::SourceType::MAGNET,
      "magnet:?xt=urn:btih:0101010101010101010101010101010101010101", "",
      std::vector<std::string>{},
      "test_outdir/0101010101010101010101010101010101010101.aria2");
  auto attrsPtr = attrs.get();
  attrsPtr->pauseAfterMetadata = true;
  attrsPtr->metadataPauseApplied = true;
  dctx->setAttribute(CTX_ATTR_LIBTORRENT, std::move(attrs));

  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  group->setPauseRequested(true);
  e_->getRequestGroupMan()->addReservedGroup(group);

  {
    ChangeOptionRpcMethod m;
    auto req = createReq(ChangeOptionRpcMethod::getMethodName());
    req.params->append(GroupId::toHex(group->getGID()));
    auto opt = Dict::g();
    opt->put(PREF_SELECT_FILE->k, "2");
    req.params->append(std::move(opt));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }

  CPPUNIT_ASSERT_EQUAL(std::string("2"), attrsPtr->selectedFiles);
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrsPtr->filePriorities.size());
  CPPUNIT_ASSERT_EQUAL(0, attrsPtr->filePriorities[0]);
  CPPUNIT_ASSERT_EQUAL(4, attrsPtr->filePriorities[1]);

  UnpauseRpcMethod m;
  auto req = createReq(UnpauseRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(group->getGID()));
  auto res = m.execute(std::move(req), e_.get());

  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT(!group->isPauseRequested());
  CPPUNIT_ASSERT(attrsPtr->metadataPauseApplied);
  CPPUNIT_ASSERT_EQUAL((size_t)2, attrsPtr->filePriorities.size());
  CPPUNIT_ASSERT_EQUAL(0, attrsPtr->filePriorities[0]);
  CPPUNIT_ASSERT_EQUAL(4, attrsPtr->filePriorities[1]);
}
#endif // ENABLE_BITTORRENT

void RpcMethodTest::testGatherProgressCommon()
{
  auto dctx = std::make_shared<DownloadContext>(0, 0, "aria2.tar.bz2");
  std::string uris[] = {"http://localhost/aria2.tar.bz2"};
  dctx->getFirstFileEntry()->addUris(std::begin(uris), std::end(uris));
  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  std::vector<std::shared_ptr<RequestGroup>> followedBy;
  for (int i = 0; i < 2; ++i) {
    followedBy.push_back(
        std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)));
  }

  group->followedBy(followedBy.begin(), followedBy.end());
  auto leader = GroupId::create();
  group->following(leader->getNumericId());
  auto parent = GroupId::create();
  group->belongsTo(parent->getNumericId());

  auto entry = Dict::g();
  std::vector<std::string> keys;
  gatherProgressCommon(entry.get(), group, keys);

  const List* followedByRes = downcast<List>(entry->get("followedBy"));
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(followedBy[0]->getGID()),
                       downcast<String>(followedByRes->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(GroupId::toHex(followedBy[1]->getGID()),
                       downcast<String>(followedByRes->get(1))->s());
  CPPUNIT_ASSERT_EQUAL(leader->toHex(),
                       downcast<String>(entry->get("following"))->s());
  CPPUNIT_ASSERT_EQUAL(parent->toHex(),
                       downcast<String>(entry->get("belongsTo"))->s());
  const List* files = downcast<List>(entry->get("files"));
  CPPUNIT_ASSERT_EQUAL((size_t)1, files->size());
  const Dict* file = downcast<Dict>(files->get(0));
  CPPUNIT_ASSERT_EQUAL(std::string("aria2.tar.bz2"),
                       downcast<String>(file->get("path"))->s());
  CPPUNIT_ASSERT_EQUAL(
      uris[0],
      downcast<String>(
          downcast<Dict>(downcast<List>(file->get("uris"))->get(0))->get("uri"))
          ->s());
  CPPUNIT_ASSERT_EQUAL(e_->getOption()->get(PREF_DIR),
                       downcast<String>(entry->get("dir"))->s());

  keys.push_back("gid");
  entry = Dict::g();
  gatherProgressCommon(entry.get(), group, keys);

  CPPUNIT_ASSERT_EQUAL((size_t)1, entry->size());
  CPPUNIT_ASSERT(entry->containsKey("gid"));
}

void RpcMethodTest::testGatherProgressCommonSeparatesInFlightProgress()
{
  auto dctx = std::make_shared<DownloadContext>(1_m, 256_m, "file.bin");
  auto group =
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_));
  group->setDownloadContext(dctx);
  group->initPieceStorage();
  group->getPieceStorage()->markPiecesDone(250_m);

  std::vector<std::shared_ptr<Piece>> inFlightPieces;
  for (int i = 0; i < 2; ++i) {
    auto p = std::make_shared<Piece>(250 + i, 1_m);
    for (int j = 0; j < 32; ++j) {
      p->completeBlock(j);
    }
    inFlightPieces.push_back(p);
  }
  group->getPieceStorage()->addInFlightPiece(inFlightPieces);

  auto entry = Dict::g();
  gatherProgressCommon(entry.get(), group,
                       {"completedLength", "inFlightCompletedLength"});

  CPPUNIT_ASSERT_EQUAL(std::string("262144000"),
                       getString(entry.get(), "completedLength"));
  CPPUNIT_ASSERT_EQUAL(std::string("1048576"),
                       getString(entry.get(), "inFlightCompletedLength"));
}

void RpcMethodTest::testChangePosition()
{
  e_->getRequestGroupMan()->addReservedGroup(
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)));
  e_->getRequestGroupMan()->addReservedGroup(
      std::make_shared<RequestGroup>(GroupId::create(), util::copy(option_)));

  a2_gid_t gid = getReservedGroup(e_->getRequestGroupMan().get(), 0)->getGID();
  ChangePositionRpcMethod m;
  auto req = createReq(ChangePositionRpcMethod::getMethodName());
  req.params->append(GroupId::toHex(gid));
  req.params->append(Integer::g(1));
  req.params->append("POS_SET");
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL((int64_t)1, downcast<Integer>(res.param)->i());
  CPPUNIT_ASSERT_EQUAL(
      gid, getReservedGroup(e_->getRequestGroupMan().get(), 1)->getGID());
}

void RpcMethodTest::testChangePosition_fail()
{
  ChangePositionRpcMethod m;
  auto res =
      m.execute(createReq(ChangePositionRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  auto req = createReq(ChangePositionRpcMethod::getMethodName());
  req.params->append("1");
  req.params->append(Integer::g(2));
  req.params->append("bad keyword");
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

namespace {
RpcRequest createChangeUriReq(a2_gid_t gid, size_t fileIndex)
{
  auto req = createReq(ChangeUriRpcMethod::getMethodName());

  req.params->append(GroupId::toHex(gid));   // GID
  req.params->append(Integer::g(fileIndex)); // index of FileEntry
  auto removeuris = List::g();
  removeuris->append("http://example.org/mustremove1");
  removeuris->append("http://example.org/mustremove2");
  removeuris->append("http://example.org/notexist");
  req.params->append(std::move(removeuris));
  return req;
}
} // namespace

void RpcMethodTest::testChangeUri()
{
  std::shared_ptr<FileEntry> files[3];
  for (int i = 0; i < 3; ++i) {
    files[i].reset(new FileEntry());
  }
  files[1]->addUri("http://example.org/aria2.tar.bz2");
  files[1]->addUri("http://example.org/mustremove1");
  files[1]->addUri("http://example.org/mustremove2");
  auto dctx = std::make_shared<DownloadContext>();
  dctx->setFileEntries(&files[0], &files[3]);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeUriRpcMethod m;
  auto req = createChangeUriReq(group->getGID(), 2);
  auto adduris = List::g();
  adduris->append("http://example.org/added1");
  adduris->append("http://example.org/added2");
  adduris->append("baduri");
  adduris->append("http://example.org/added3");
  req.params->append(std::move(adduris));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)2, downcast<Integer>(downcast<List>(res.param)->get(0))->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)3, downcast<Integer>(downcast<List>(res.param)->get(1))->i());
  CPPUNIT_ASSERT_EQUAL((size_t)0, files[0]->getRemainingUris().size());
  CPPUNIT_ASSERT_EQUAL((size_t)0, files[2]->getRemainingUris().size());
  std::deque<std::string> uris = files[1]->getRemainingUris();
  CPPUNIT_ASSERT_EQUAL((size_t)4, uris.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/aria2.tar.bz2"),
                       uris[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1"), uris[1]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added2"), uris[2]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added3"), uris[3]);

  req = createChangeUriReq(group->getGID(), 2);
  // Change adduris
  adduris = List::g();
  adduris->append("http://example.org/added1-1");
  adduris->append("http://example.org/added1-2");
  req.params->append(std::move(adduris));
  // Set position parameter
  req.params->append(Integer::g(2));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)0, downcast<Integer>(downcast<List>(res.param)->get(0))->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)2, downcast<Integer>(downcast<List>(res.param)->get(1))->i());
  uris = files[1]->getRemainingUris();
  CPPUNIT_ASSERT_EQUAL((size_t)6, uris.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-1"), uris[2]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-2"), uris[3]);

  // Change index of FileEntry
  req = createChangeUriReq(group->getGID(), 1);
  adduris = List::g();
  adduris->append("http://example.org/added1-1");
  adduris->append("http://example.org/added1-2");
  req.params->append(std::move(adduris));
  // Set position far beyond the size of uris in FileEntry.
  req.params->append(Integer::g(1000));
  res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)0, downcast<Integer>(downcast<List>(res.param)->get(0))->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)2, downcast<Integer>(downcast<List>(res.param)->get(1))->i());
  uris = files[0]->getRemainingUris();
  CPPUNIT_ASSERT_EQUAL((size_t)2, uris.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-1"), uris[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("http://example.org/added1-2"), uris[1]);
}

namespace {
RpcRequest createChangeUriEmptyReq(a2_gid_t gid, size_t fileIndex)
{
  auto req = createReq(ChangeUriRpcMethod::getMethodName());

  req.params->append(GroupId::toHex(gid));   // GID
  req.params->append(Integer::g(fileIndex)); // index of FileEntry
  req.params->append(List::g());             // remove uris
  req.params->append(List::g());             // append uris
  return req;
}
} // namespace

void RpcMethodTest::testChangeUri_fail()
{
  std::shared_ptr<FileEntry> files[3];
  for (int i = 0; i < 3; ++i) {
    files[i] = std::make_shared<FileEntry>();
  }
  auto dctx = std::make_shared<DownloadContext>();
  dctx->setFileEntries(&files[0], &files[3]);
  auto group = std::make_shared<RequestGroup>(GroupId::create(), option_);
  group->setDownloadContext(dctx);
  e_->getRequestGroupMan()->addReservedGroup(group);

  ChangeUriRpcMethod m;
  auto req = createChangeUriEmptyReq(group->getGID(), 1);
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 0);
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because 2nd argument is less than 1.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(GroupId::create()->getNumericId(), 1);
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because the given GID does not exist.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 4);
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because FileEntry#3 does not exist.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 1);
  req.params->set(1, String::g("0"));
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because index of FileEntry is string.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 1);
  req.params->set(2, String::g("http://url"));
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because 3rd param is not list.
  CPPUNIT_ASSERT_EQUAL(1, res.code);

  req = createChangeUriEmptyReq(group->getGID(), 1);
  req.params->set(2, List::g());
  req.params->set(3, String::g("http://url"));
  res = m.execute(std::move(req), e_.get());
  // RPC request fails because 4th param is not list.
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testGetSessionInfo()
{
  GetSessionInfoRpcMethod m;
  auto res =
      m.execute(createReq(GetSessionInfoRpcMethod::getMethodName()), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  CPPUNIT_ASSERT_EQUAL(util::toHex(e_->getSessionId()),
                       getString(downcast<Dict>(res.param), "sessionId"));
}

void RpcMethodTest::testPause()
{
  std::vector<std::string> uris{
      "http://url1",
      "http://url2",
      "http://url3",
  };
  option_->put(PREF_FORCE_SEQUENTIAL, A2_V_TRUE);
  std::vector<std::shared_ptr<RequestGroup>> groups;
  createRequestGroupForUri(groups, option_, uris);
  CPPUNIT_ASSERT_EQUAL((size_t)3, groups.size());
  e_->getRequestGroupMan()->addReservedGroup(groups);
  {
    PauseRpcMethod m;
    auto req = createReq(PauseRpcMethod::getMethodName());
    req.params->append(GroupId::toHex(groups[0]->getGID()));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  CPPUNIT_ASSERT(groups[0]->isPauseRequested());
  {
    UnpauseRpcMethod m;
    auto req = createReq(UnpauseRpcMethod::getMethodName());
    req.params->append(GroupId::toHex(groups[0]->getGID()));
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  CPPUNIT_ASSERT(!groups[0]->isPauseRequested());
  {
    PauseAllRpcMethod m;
    auto req = createReq(PauseAllRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  for (size_t i = 0; i < groups.size(); ++i) {
    CPPUNIT_ASSERT(groups[i]->isPauseRequested());
  }
  {
    UnpauseAllRpcMethod m;
    auto req = createReq(UnpauseAllRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  for (size_t i = 0; i < groups.size(); ++i) {
    CPPUNIT_ASSERT(!groups[i]->isPauseRequested());
  }
  {
    ForcePauseAllRpcMethod m;
    auto req = createReq(ForcePauseAllRpcMethod::getMethodName());
    auto res = m.execute(std::move(req), e_.get());
    CPPUNIT_ASSERT_EQUAL(0, res.code);
  }
  for (size_t i = 0; i < groups.size(); ++i) {
    CPPUNIT_ASSERT(groups[i]->isPauseRequested());
  }
}

void RpcMethodTest::testSystemMulticall()
{
  SystemMulticallRpcMethod m;
  auto req = createReq("system.multicall");
  auto reqparams = List::g();
  for (int i = 0; i < 2; ++i) {
    auto dict = Dict::g();
    dict->put("methodName", AddUriRpcMethod::getMethodName());
    auto params = List::g();
    auto urisParam = List::g();
    urisParam->append("http://localhost/" + util::itos(i));
    params->append(std::move(urisParam));
    dict->put("params", std::move(params));
    reqparams->append(std::move(dict));
  }
  {
    auto dict = Dict::g();
    dict->put("methodName", "not exists");
    dict->put("params", List::g());
    reqparams->append(std::move(dict));
  }
  {
    reqparams->append("not struct");
  }
  {
    auto dict = Dict::g();
    dict->put("methodName", "system.multicall");
    dict->put("params", List::g());
    reqparams->append(std::move(dict));
  }
  {
    // missing params
    auto dict = Dict::g();
    dict->put("methodName", GetVersionRpcMethod::getMethodName());
    reqparams->append(std::move(dict));
  }
  {
    auto dict = Dict::g();
    dict->put("methodName", GetVersionRpcMethod::getMethodName());
    dict->put("params", List::g());
    reqparams->append(std::move(dict));
  }
  req.params->append(std::move(reqparams));
  auto res = m.execute(std::move(req), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);
  const List* resParams = downcast<List>(res.param);
  CPPUNIT_ASSERT_EQUAL((size_t)7, resParams->size());
  auto& rgman = e_->getRequestGroupMan();
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 0)->getGID()),
      downcast<String>(downcast<List>(resParams->get(0))->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(
      GroupId::toHex(getReservedGroup(rgman.get(), 1)->getGID()),
      downcast<String>(downcast<List>(resParams->get(1))->get(0))->s());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)1,
      downcast<Integer>(downcast<Dict>(resParams->get(2))->get("faultCode"))
          ->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)1,
      downcast<Integer>(downcast<Dict>(resParams->get(3))->get("faultCode"))
          ->i());
  CPPUNIT_ASSERT_EQUAL(
      (int64_t)1,
      downcast<Integer>(downcast<Dict>(resParams->get(4))->get("faultCode"))
          ->i());
  CPPUNIT_ASSERT(downcast<List>(resParams->get(5)));
  CPPUNIT_ASSERT(downcast<List>(resParams->get(6)));
}

void RpcMethodTest::testSystemMulticall_fail()
{
  SystemMulticallRpcMethod m;
  auto res = m.execute(createReq("system.multicall"), e_.get());
  CPPUNIT_ASSERT_EQUAL(1, res.code);
}

void RpcMethodTest::testSystemListMethods()
{
  SystemListMethodsRpcMethod m;
  auto res = m.execute(createReq("system.listMethods"), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);

  const auto resParams = downcast<List>(res.param);
  auto& allNames = allMethodNames();

  CPPUNIT_ASSERT_EQUAL(allNames.size(), resParams->size());

  for (size_t i = 0; i < allNames.size(); ++i) {
    const auto s = downcast<String>(resParams->get(i));
    CPPUNIT_ASSERT(s);
    CPPUNIT_ASSERT_EQUAL(allNames[i], s->s());
  }
}

void RpcMethodTest::testSystemListNotifications()
{
  SystemListNotificationsRpcMethod m;
  auto res = m.execute(createReq("system.listNotifications"), e_.get());
  CPPUNIT_ASSERT_EQUAL(0, res.code);

  const auto resParams = downcast<List>(res.param);
  auto& allNames = allNotificationsNames();

  CPPUNIT_ASSERT_EQUAL(allNames.size(), resParams->size());

  for (size_t i = 0; i < allNames.size(); ++i) {
    const auto s = downcast<String>(resParams->get(i));
    CPPUNIT_ASSERT(s);
    CPPUNIT_ASSERT_EQUAL(allNames[i], s->s());
  }
}

} // namespace rpc

} // namespace aria2
