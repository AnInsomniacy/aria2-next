/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/Checksum.h"

#include "base/Text.h"
#include "disk/DiskPool.h"
#include "disk/File.h"
#include "http/Metalink.h"

#include <openssl/evp.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace aria2::http {

namespace {
const EVP_MD* digestType(const std::string& type)
{
  const auto name = text::lower(type);
  return name == "sha-1"     ? EVP_sha1()
         : name == "sha-224" ? EVP_sha224()
         : name == "sha-256" ? EVP_sha256()
         : name == "sha-384" ? EVP_sha384()
         : name == "sha-512" ? EVP_sha512()
         : name == "md5"     ? EVP_md5()
                             : nullptr;
}

using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::string finish(EVP_MD_CTX* context)
{
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int size = 0;
  EVP_DigestFinal_ex(context, digest, &size);
  return text::toHex(std::string_view(reinterpret_cast<char*>(digest), size));
}
} // namespace

std::string fileDigest(const std::string& path, const std::string& type)
{
  const auto* algorithm = digestType(type);
  File file;
  if (!algorithm || file.openReadOnly(path)) {
    return {};
  }
  Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  EVP_DigestInit_ex(context.get(), algorithm, nullptr);
  std::vector<char> buffer(1 << 20);
  int64_t offset = 0;
  for (;;) {
    size_t read = 0;
    if (file.read(offset, buffer.data(), buffer.size(), read)) {
      return {};
    }
    if (read == 0) {
      break;
    }
    EVP_DigestUpdate(context.get(), buffer.data(), read);
    offset += static_cast<int64_t>(read);
  }
  return finish(context.get());
}

std::optional<std::vector<size_t>> badPieces(const std::string& path,
                                             const PieceHashes& pieces)
{
  const auto* algorithm = digestType(pieces.type);
  File file;
  if (!algorithm || pieces.length <= 0 || file.openReadOnly(path)) {
    return std::nullopt;
  }
  Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  std::vector<char> buffer(1 << 20);
  std::vector<size_t> bad;
  for (size_t piece = 0; piece < pieces.hashes.size(); ++piece) {
    EVP_DigestInit_ex(context.get(), algorithm, nullptr);
    auto offset = static_cast<int64_t>(piece) * pieces.length;
    const auto end = offset + pieces.length;
    while (offset < end) {
      size_t read = 0;
      const auto want =
          static_cast<size_t>(std::min<int64_t>(buffer.size(), end - offset));
      if (file.read(offset, buffer.data(), want, read)) {
        return std::nullopt;
      }
      if (read == 0) {
        break; // the last piece is shorter
      }
      EVP_DigestUpdate(context.get(), buffer.data(), read);
      offset += static_cast<int64_t>(read);
    }
    if (finish(context.get()) != pieces.hashes[piece]) {
      bad.push_back(piece);
    }
  }
  return bad;
}

void verifyChecksum(DiskPool& pool, const std::string& path,
                    const std::string& expected,
                    std::function<void(std::optional<std::string>)> done)
{
  const auto equals = expected.find('=');
  const auto type = expected.substr(0, equals);
  auto digest = equals == std::string::npos ? std::string()
                                            : expected.substr(equals + 1);
  if (digest.size() % 2 != 0 || digest.find_first_not_of("0123456789abcdefABCDEF") !=
                                    std::string::npos) {
    // Base64 digests, as some metalinks and headers carry them.
    digest = text::toHex(text::base64Decode(digest).value_or(""));
  }
  digest = text::lower(digest);
  auto strand = pool.strand();
  pool.run(
      strand, [path, type] { return fileDigest(path, type); },
      [digest, type, done = std::move(done)](std::string actual) {
        if (actual.empty()) {
          done("Cannot compute the " + type + " checksum");
        }
        else if (actual != digest) {
          done("Checksum mismatch: expected " + digest + ", got " + actual);
        }
        else {
          done(std::nullopt);
        }
      });
}

} // namespace aria2::http
