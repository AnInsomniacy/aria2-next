#include "download_handlers.h"

#include "a2doctest.h"

#include "RequestGroup.h"
#include "Option.h"
#include "DownloadContext.h"
#include "MemoryBufferPreDownloadHandler.h"
#include "FileEntry.h"
#include "RequestGroupCriteria.h"

namespace aria2 {

class DownloadHandlersTest {


private:
  std::shared_ptr<Option> option_;

public:
  void setUp() { option_ = std::make_shared<Option>(); }
  void testGetMemoryPreDownloadHandler();
#ifdef ENABLE_METALINK
  void testGetMetalinkPreDownloadHandler_extension();
  void testGetMetalinkPreDownloadHandler_contentType();
#endif // ENABLE_METALINK

#ifdef ENABLE_BITTORRENT
  void testGetBtPreDownloadHandler_extension();
  void testGetBtPreDownloadHandler_contentType();
#endif // ENABLE_BITTORRENT
};

A2_TEST(DownloadHandlersTest, testGetMemoryPreDownloadHandler)
#ifdef ENABLE_METALINK
A2_TEST(DownloadHandlersTest, testGetMetalinkPreDownloadHandler_extension)
A2_TEST(DownloadHandlersTest, testGetMetalinkPreDownloadHandler_contentType)
#endif // ENABLE_METALINK
#ifdef ENABLE_BITTORRENT
A2_TEST(DownloadHandlersTest, testGetBtPreDownloadHandler_extension)
A2_TEST(DownloadHandlersTest, testGetBtPreDownloadHandler_contentType)
#endif // ENABLE_BITTORRENT

void DownloadHandlersTest::testGetMemoryPreDownloadHandler()
{
  REQUIRE(
      download_handlers::getMemoryPreDownloadHandler()->canHandle(nullptr));
}

#ifdef ENABLE_METALINK

void DownloadHandlersTest::testGetMetalinkPreDownloadHandler_extension()
{
  auto dctx = std::make_shared<DownloadContext>(0, 0, "test.metalink");
  RequestGroup rg(GroupId::create(), option_);
  rg.setDownloadContext(dctx);

  auto handler = download_handlers::getMetalinkPreDownloadHandler();

  REQUIRE(handler->canHandle(&rg));

  dctx->getFirstFileEntry()->setPath("test.metalink2");
  REQUIRE(!handler->canHandle(&rg));
}

void DownloadHandlersTest::testGetMetalinkPreDownloadHandler_contentType()
{
  auto dctx = std::make_shared<DownloadContext>(0, 0, "test");
  dctx->getFirstFileEntry()->setContentType("application/metalink+xml");
  RequestGroup rg(GroupId::create(), option_);
  rg.setDownloadContext(dctx);

  auto handler = download_handlers::getMetalinkPreDownloadHandler();

  REQUIRE(handler->canHandle(&rg));

  dctx->getFirstFileEntry()->setContentType("application/octet-stream");
  REQUIRE(!handler->canHandle(&rg));
}

#endif // ENABLE_METALINK

#ifdef ENABLE_BITTORRENT

void DownloadHandlersTest::testGetBtPreDownloadHandler_extension()
{
  auto dctx =
      std::make_shared<DownloadContext>(0, 0, A2_TEST_DIR "/test.torrent");
  RequestGroup rg(GroupId::create(), option_);
  rg.setDownloadContext(dctx);

  auto handler = download_handlers::getBtPreDownloadHandler();

  REQUIRE(handler->canHandle(&rg));

  dctx->getFirstFileEntry()->setPath(A2_TEST_DIR "/test.torrent2");
  REQUIRE(!handler->canHandle(&rg));
}

void DownloadHandlersTest::testGetBtPreDownloadHandler_contentType()
{
  auto dctx = std::make_shared<DownloadContext>(0, 0, "test");
  dctx->getFirstFileEntry()->setContentType("application/x-bittorrent");
  RequestGroup rg(GroupId::create(), option_);
  rg.setDownloadContext(dctx);

  auto handler = download_handlers::getBtPreDownloadHandler();

  REQUIRE(handler->canHandle(&rg));

  dctx->getFirstFileEntry()->setContentType("application/octet-stream");
  REQUIRE(!handler->canHandle(&rg));
}

#endif // ENABLE_BITTORRENT

} // namespace aria2
