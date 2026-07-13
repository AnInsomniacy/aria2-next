#include "MetalinkParserController.h"

#include "a2doctest.h"

#include "Metalinker.h"
#include "MetalinkEntry.h"
#include "MetalinkResource.h"
#include "MetalinkMetaurl.h"
#include "FileEntry.h"
#include "Checksum.h"
#include "ChunkChecksum.h"
#include "Signature.h"

namespace aria2 {

class MetalinkParserControllerTest {


private:
public:
  void setUp() {}

  void tearDown() {}

  void testEntryTransaction();
  void testResourceTransaction();
  void testResourceTransaction_withBaseUri();
  void testMetaurlTransaction();
  void testChecksumTransaction();
  void testChunkChecksumTransaction();
  void testChunkChecksumTransactionV4();
  void testSignatureTransaction();
};

A2_TEST(MetalinkParserControllerTest, testEntryTransaction)
A2_TEST(MetalinkParserControllerTest, testResourceTransaction)
A2_TEST(MetalinkParserControllerTest, testResourceTransaction_withBaseUri)
A2_TEST(MetalinkParserControllerTest, testMetaurlTransaction)
A2_TEST(MetalinkParserControllerTest, testChecksumTransaction)
A2_TEST(MetalinkParserControllerTest, testChunkChecksumTransaction)
A2_TEST(MetalinkParserControllerTest, testChunkChecksumTransactionV4)
A2_TEST(MetalinkParserControllerTest, testSignatureTransaction)

void MetalinkParserControllerTest::testEntryTransaction()
{
  MetalinkParserController ctrl;

  ctrl.newEntryTransaction();
  ctrl.setFileNameOfEntry("aria2.tar.bz2");
  ctrl.setFileLengthOfEntry(1_m);
  ctrl.setVersionOfEntry("1.0");
  ctrl.setLanguageOfEntry("ja_JP");
  ctrl.setOSOfEntry("Linux");
  ctrl.commitEntryTransaction();
  ctrl.newEntryTransaction();
  ctrl.cancelEntryTransaction();
  {
    auto m = ctrl.getResult();
    REQUIRE_EQ((size_t)1, m->getEntries().size());
    auto& e = m->getEntries()[0];
    REQUIRE_EQ(std::string("aria2.tar.bz2"), e->file->getPath());
    REQUIRE_EQ((int64_t)1_m, e->file->getLength());
    REQUIRE_EQ((int64_t)0, e->file->getOffset());
    REQUIRE_EQ(std::string("1.0"), e->version);
    REQUIRE_EQ(std::string("ja_JP"), e->languages[0]);
    REQUIRE_EQ(std::string("Linux"), e->oses[0]);
  }
}

void MetalinkParserControllerTest::testResourceTransaction()
{
  MetalinkParserController ctrl;
  ctrl.newEntryTransaction();
  ctrl.newResourceTransaction();
  ctrl.setURLOfResource("http://mirror/aria2.tar.bz2");
  ctrl.setTypeOfResource("http");
  ctrl.setLocationOfResource("US");
  ctrl.setPriorityOfResource(100);
  ctrl.setMaxConnectionsOfResource(1);
  ctrl.commitEntryTransaction();
  ctrl.newEntryTransaction();
  ctrl.newResourceTransaction();
  ctrl.cancelResourceTransaction();
  ctrl.commitEntryTransaction();
  {
    auto m = ctrl.getResult();
    REQUIRE_EQ((size_t)2, m->getEntries().size());
    REQUIRE_EQ((size_t)1, m->getEntries()[0]->resources.size());
    REQUIRE_EQ((size_t)0, m->getEntries()[1]->resources.size());

    auto& res = m->getEntries()[0]->resources[0];
    REQUIRE_EQ(std::string("http://mirror/aria2.tar.bz2"), res->url);
    REQUIRE_EQ(MetalinkResource::TYPE_HTTP, res->type);
    REQUIRE_EQ(std::string("US"), res->location);
    REQUIRE_EQ(100, res->priority);
    REQUIRE_EQ(1, res->maxConnections);
  }
}

void MetalinkParserControllerTest::testResourceTransaction_withBaseUri()
{
  MetalinkParserController ctrl;
  ctrl.setBaseUri("http://base/dir/file");
  ctrl.newEntryTransaction();
  ctrl.newResourceTransaction();
  ctrl.setURLOfResource("aria2.tar.bz2");
  ctrl.commitResourceTransaction();
#ifdef ENABLE_BITTORRENT
  ctrl.newMetaurlTransaction();
  ctrl.setURLOfMetaurl("/meta/aria2.tar.bz2.torrent");
  ctrl.setMediatypeOfMetaurl("torrent");
  ctrl.commitMetaurlTransaction();
  ctrl.newMetaurlTransaction();
  ctrl.setURLOfMetaurl(
      "magnet:?xt=urn:btih:248d0a1cd08284299de78d5c1ed359bb46717d8c");
  ctrl.setMediatypeOfMetaurl("torrent");
  ctrl.commitMetaurlTransaction();
#endif // ENABLE_BITTORRENT
  ctrl.commitEntryTransaction();
  {
    auto m = ctrl.getResult();
    REQUIRE_EQ((size_t)1, m->getEntries()[0]->resources.size());
    auto& res = m->getEntries()[0]->resources[0];
    REQUIRE_EQ(std::string("http://base/dir/aria2.tar.bz2"),
                         res->url);
    REQUIRE_EQ(MetalinkResource::TYPE_HTTP, res->type);

#ifdef ENABLE_BITTORRENT
    REQUIRE_EQ((size_t)2, m->getEntries()[0]->metaurls.size());
    REQUIRE_EQ(std::string("http://base/meta/aria2.tar.bz2.torrent"),
                         m->getEntries()[0]->metaurls[0]->url);

    REQUIRE_EQ(
        std::string(
            "magnet:?xt=urn:btih:248d0a1cd08284299de78d5c1ed359bb46717d8c"),
        m->getEntries()[0]->metaurls[1]->url);
#endif // ENABLE_BITTORRENT
  }
}

void MetalinkParserControllerTest::testMetaurlTransaction()
{
  MetalinkParserController ctrl;
  ctrl.newEntryTransaction();
  ctrl.newMetaurlTransaction();
  ctrl.setURLOfMetaurl("http://example.org/chocolate.torrent");
  ctrl.setMediatypeOfMetaurl("torrent");
  ctrl.setPriorityOfMetaurl(999);
  ctrl.setNameOfMetaurl("mybirthdaycake");
  ctrl.commitEntryTransaction();
#ifdef ENABLE_BITTORRENT
  ctrl.newEntryTransaction();
  ctrl.newMetaurlTransaction();
  ctrl.cancelMetaurlTransaction();
  ctrl.commitEntryTransaction();
  {
    auto m = ctrl.getResult();
    REQUIRE_EQ((size_t)2, m->getEntries().size());
    REQUIRE_EQ((size_t)1, m->getEntries()[0]->metaurls.size());
    REQUIRE_EQ((size_t)0, m->getEntries()[1]->metaurls.size());

    auto& metaurl = m->getEntries()[0]->metaurls[0];
    REQUIRE_EQ(std::string("http://example.org/chocolate.torrent"),
                         metaurl->url);
    REQUIRE_EQ(std::string("torrent"), metaurl->mediatype);
    REQUIRE_EQ(std::string("mybirthdaycake"), metaurl->name);
    REQUIRE_EQ(999, metaurl->priority);
  }
#else  // !ENABLE_BITTORRENT
  {
    auto m = ctrl.getResult();
    REQUIRE_EQ((size_t)1, m->getEntries().size());
    REQUIRE_EQ((size_t)0, m->getEntries()[0]->metaurls.size());
  }
#endif // !ENABLE_BITTORRENT
}

void MetalinkParserControllerTest::testChecksumTransaction()
{
  MetalinkParserController ctrl;
  ctrl.newEntryTransaction();
  ctrl.newChecksumTransaction();
  ctrl.setTypeOfChecksum("md5");
  ctrl.setHashOfChecksum("acbd18db4cc2f85cedef654fccc4a4d8");
  ctrl.commitEntryTransaction();

  ctrl.newEntryTransaction();
  ctrl.newChecksumTransaction();
  ctrl.setTypeOfChecksum("md5");
  ctrl.setHashOfChecksum("badhash");
  ctrl.commitEntryTransaction();

  ctrl.newEntryTransaction();
  ctrl.newChecksumTransaction();
  ctrl.cancelChecksumTransaction();
  ctrl.commitEntryTransaction();
  {
    auto m = ctrl.getResult();
    auto& md = m->getEntries()[0]->checksum;
    REQUIRE_EQ(std::string("md5"), md->getHashType());
    REQUIRE_EQ(std::string("acbd18db4cc2f85cedef654fccc4a4d8"),
                         util::toHex(md->getDigest()));

    REQUIRE(!m->getEntries()[1]->checksum);

    REQUIRE(!m->getEntries()[2]->checksum);
  }
}

void MetalinkParserControllerTest::testChunkChecksumTransaction()
{
  MetalinkParserController ctrl;
  ctrl.newEntryTransaction();
  ctrl.newChunkChecksumTransaction();
  ctrl.setTypeOfChunkChecksum("md5");
  ctrl.setLengthOfChunkChecksum(256_k);
  ctrl.addHashOfChunkChecksum(4, "4cbd18db4cc2f85cedef654fccc4a4d8");
  ctrl.addHashOfChunkChecksum(1, "1cbd18db4cc2f85cedef654fccc4a4d8");
  ctrl.addHashOfChunkChecksum(3, "3cbd18db4cc2f85cedef654fccc4a4d8");
  ctrl.addHashOfChunkChecksum(2, "2cbd18db4cc2f85cedef654fccc4a4d8");
  ctrl.addHashOfChunkChecksum(5, "5cbd18db4cc2f85cedef654fccc4a4d8");
  ctrl.commitEntryTransaction();

  ctrl.newEntryTransaction();
  ctrl.newChunkChecksumTransaction();
  ctrl.setTypeOfChunkChecksum("md5");
  ctrl.setLengthOfChunkChecksum(256_k);
  ctrl.addHashOfChunkChecksum(1, "badhash");
  ctrl.commitEntryTransaction();

  ctrl.newEntryTransaction();
  ctrl.newChunkChecksumTransaction();
  ctrl.cancelChunkChecksumTransaction();
  ctrl.commitEntryTransaction();
  {
    auto m = ctrl.getResult();
    auto& md = m->getEntries()[0]->chunkChecksum;
    REQUIRE_EQ(std::string("md5"), md->getHashType());
    REQUIRE_EQ((int32_t)256_k, md->getPieceLength());
    REQUIRE_EQ((size_t)5, md->countPieceHash());
    REQUIRE_EQ(std::string("1cbd18db4cc2f85cedef654fccc4a4d8"),
                         md->getPieceHashes()[0]);
    REQUIRE_EQ(std::string("2cbd18db4cc2f85cedef654fccc4a4d8"),
                         md->getPieceHashes()[1]);
    REQUIRE_EQ(std::string("3cbd18db4cc2f85cedef654fccc4a4d8"),
                         md->getPieceHashes()[2]);
    REQUIRE_EQ(std::string("4cbd18db4cc2f85cedef654fccc4a4d8"),
                         md->getPieceHashes()[3]);
    REQUIRE_EQ(std::string("5cbd18db4cc2f85cedef654fccc4a4d8"),
                         md->getPieceHashes()[4]);

    REQUIRE(!m->getEntries()[1]->chunkChecksum);

    REQUIRE(!m->getEntries()[2]->chunkChecksum);
  }
}

void MetalinkParserControllerTest::testChunkChecksumTransactionV4()
{
  MetalinkParserController ctrl;
  ctrl.newEntryTransaction();
  ctrl.newChunkChecksumTransactionV4();
  ctrl.setTypeOfChunkChecksumV4("sha-1");
  ctrl.setLengthOfChunkChecksumV4(256_k);

  ctrl.addHashOfChunkChecksumV4("5bd9f7248df0f3a6a86ab6c95f48787d546efa14");
  ctrl.addHashOfChunkChecksumV4("9413ee70957a09d55704123687478e07f18c7b29");
  ctrl.addHashOfChunkChecksumV4("44213f9f4d59b557314fadcd233232eebcac8012");
  ctrl.commitEntryTransaction();

  ctrl.newEntryTransaction();
  ctrl.newChunkChecksumTransactionV4();
  ctrl.setTypeOfChunkChecksumV4("sha-1");
  ctrl.setLengthOfChunkChecksumV4(256_k);
  ctrl.addHashOfChunkChecksumV4("5bd9f7248df0f3a6a86ab6c95f48787d546efa14");
  ctrl.addHashOfChunkChecksumV4("badhash");
  ctrl.commitEntryTransaction();

  ctrl.newEntryTransaction();
  ctrl.newChunkChecksumTransactionV4();
  ctrl.cancelChunkChecksumTransactionV4();
  ctrl.commitEntryTransaction();
  {
    auto m = ctrl.getResult();
    auto& md = m->getEntries()[0]->chunkChecksum;
    REQUIRE_EQ(std::string("sha-1"), md->getHashType());
    REQUIRE_EQ((int32_t)256_k, md->getPieceLength());
    REQUIRE_EQ((size_t)3, md->countPieceHash());
    REQUIRE_EQ(
        std::string("5bd9f7248df0f3a6a86ab6c95f48787d546efa14"),
        util::toHex(md->getPieceHashes()[0]));
    REQUIRE_EQ(
        std::string("9413ee70957a09d55704123687478e07f18c7b29"),
        util::toHex(md->getPieceHashes()[1]));
    REQUIRE_EQ(
        std::string("44213f9f4d59b557314fadcd233232eebcac8012"),
        util::toHex(md->getPieceHashes()[2]));

    REQUIRE(!m->getEntries()[1]->chunkChecksum);

    REQUIRE(!m->getEntries()[2]->chunkChecksum);
  }
}

void MetalinkParserControllerTest::testSignatureTransaction()
{
  static std::string pgpSignature =
      "-----BEGIN PGP SIGNATURE-----\n"
      "Version: GnuPG v1.4.9 (GNU/Linux)\n"
      "\n"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\n"
      "ffffffffffffffffffffffff\n"
      "fffff\n"
      "-----END PGP SIGNATURE-----\n";

  MetalinkParserController ctrl;
  ctrl.newEntryTransaction();

  ctrl.newSignatureTransaction();
  ctrl.setTypeOfSignature("pgp");
  ctrl.setFileOfSignature("aria2.sig");
  ctrl.setBodyOfSignature(pgpSignature);
  // commitEntryTransaction also commits signature transaction.
  ctrl.commitEntryTransaction();

  // See when signature transaction is canceled:
  ctrl.newEntryTransaction();
  ctrl.newSignatureTransaction();
  ctrl.cancelSignatureTransaction();
  ctrl.commitEntryTransaction();

  auto m = ctrl.getResult();
  REQUIRE_EQ((size_t)2, m->getEntries().size());
  auto& sig = m->getEntries()[0]->getSignature();
  REQUIRE_EQ(std::string("pgp"), sig->getType());
  REQUIRE_EQ(std::string("aria2.sig"), sig->getFile());
  REQUIRE_EQ(pgpSignature, sig->getBody());

  REQUIRE(!m->getEntries()[1]->getSignature());
}

} // namespace aria2
