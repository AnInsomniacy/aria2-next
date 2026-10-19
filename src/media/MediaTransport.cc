/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"
#include "media/MediaTransport.h"
#include "media/MediaRequest.h"
#include "media/MuxInput.h"
#include "media/Control.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <curl/system.h>
#include <ios>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include "media/MediaFiles.h"

#include "base/Log.h"
#include "base/Text.h"
#include <curl/urlapi.h>
#include <optional>
#include <openssl/evp.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace aria2 {
namespace media {
namespace {
std::string fileDigest(const std::string& path)
{
  std::ifstream input(nativePath(path), std::ios::binary);
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                              EVP_MD_CTX_free);
  if (!input || !ctx || !EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr))
    throw std::runtime_error("Cannot verify media cache content");
  std::array<char, 65536> buffer{};
  while (input) {
    input.read(buffer.data(), buffer.size());
    if (!EVP_DigestUpdate(ctx.get(), buffer.data(),
                          static_cast<size_t>(input.gcount())))
      throw std::runtime_error("Cannot hash media cache content");
  }
  if (!input.eof())
    throw std::runtime_error("Cannot read media cache content");
  unsigned char digest[32];
  unsigned int size = 0;
  if (!EVP_DigestFinal_ex(ctx.get(), digest, &size))
    throw std::runtime_error("Cannot finish media cache verification");
  std::string hex;
  const char* digits = "0123456789abcdef";
  for (unsigned int i = 0; i < size; ++i) {
    hex += digits[digest[i] >> 4];
    hex += digits[digest[i] & 15];
  }
  return hex;
}
} // namespace

std::string Transport::digest(const std::string& path)
{
  return fileDigest(path);
}

void Transport::retain(const std::string& path)
{
  if (path.empty())
    return;
  auto name = utf8(nativePath(path).filename());
  if (name.size() < 129 || name[64] != '-')
    return;
  auto raw = utf8(nativePath(path).parent_path() / name.substr(0, 129));
  retained_[name.substr(0, 64)] = raw;
}

std::string Transport::fingerprint(const std::string& value)
{
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int size = 0;
  if (!EVP_Digest(value.data(), value.size(), digest, &size, EVP_sha256(),
                  nullptr))
    throw std::runtime_error("Cannot hash media resource identity");
  const char* digits = "0123456789abcdef";
  std::string result;
  for (unsigned i = 0; i < size; ++i) {
    result += digits[digest[i] >> 4];
    result += digits[digest[i] & 15];
  }
  return result;
}
Transport::Transport(const Options* option, std::string source,
                     std::string directory, std::shared_ptr<Control> control)
    : option_(option),
      source_(std::move(source)),
      directory_(std::move(directory)),
      control_(std::move(control)),
      contexts_(parseRequestContexts(option->get("media-request-contexts"))),
      input_(parseInputPlan(option->get("media-input"))),
      throttle_(std::make_unique<Throttle>())
{
  createDirectories(directory_);
  multi_ = curl_multi_init();
  share_ = curl_share_init();
  if (!multi_ || !share_) {
    if (multi_)
      curl_multi_cleanup(multi_);
    if (share_)
      curl_share_cleanup(share_);
    throw std::runtime_error("Cannot initialize media HTTP transport");
  }
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
  // Segment parallelism follows the per-file connection ceiling, bounded so a
  // large HTTP setting does not flood a segment origin.
  prefetchLimit_ = static_cast<size_t>(
      std::clamp<int64_t>(option->integer("max-connection-per-server"), 1, 8) -
      1);
}
Transport::~Transport()
{
  prefetched_.clear();
  curl_multi_cleanup(multi_);
  curl_share_cleanup(share_);
}

Resource Transport::get(const std::string& url, int64_t begin, int64_t end,
                        bool cached)
{
  if (!(url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0))
    throw std::runtime_error(
        "Media manifests may reference HTTP(S) resources only");
  if (begin < 0 || (end >= 0 && end < begin))
    throw std::runtime_error("Invalid media byte range");
  const auto manifest = input_.manifests.find(url);
  if (manifest != input_.manifests.end() &&
      (cached || !suppliedManifests_.count(url))) {
    if (begin != 0 || end != -1)
      throw std::runtime_error("A captured manifest cannot be byte-ranged");
    const auto path =
        utf8(nativePath(directory_) / fingerprint(url + manifest->second));
    std::ofstream output(nativePath(path), std::ios::binary | std::ios::trunc);
    output.write(manifest->second.data(), manifest->second.size());
    output.close();
    if (!output)
      throw std::runtime_error("Cannot store captured manifest");
    suppliedManifests_.insert(url);
    Resource resource{path, url, "",
                      static_cast<int64_t>(manifest->second.size())};
    learn(resource);
    return resource;
  }
  const auto key = resourceKey(url, begin, end);
  auto path = utf8(nativePath(directory_) / key);
  auto found = retained_.find(key);
  if (cached && found != retained_.end() &&
      std::filesystem::is_regular_file(nativePath(found->second))) {
    auto name = utf8(nativePath(found->second).filename());
    if (fileDigest(found->second) == name.substr(65, 64))
      return {found->second, url, "",
              static_cast<int64_t>(
                  std::filesystem::file_size(nativePath(found->second)))};
  }
  auto temporary = path + ".partial";
  const auto tries = option_->integer("max-tries");
  for (int attempt = 0;; ++attempt) {
    if (control_->cancel)
      throw std::runtime_error("Media transfer interrupted");
    try {
      control_->connections = static_cast<int>(1 + prefetched_.size());
      auto fetched = collect(key, url, begin, end, temporary);
      auto resource =
          fetched ? std::move(*fetched) : request(url, begin, end, temporary);
      const auto digest = fileDigest(temporary);
      const auto committed = path + "-" + digest;
      if (std::filesystem::exists(nativePath(committed)) &&
          fileDigest(committed) == digest)
        std::filesystem::remove(nativePath(temporary));
      else {
        std::filesystem::remove(nativePath(committed));
        std::filesystem::rename(nativePath(temporary), nativePath(committed));
      }
      resource.path = committed;
      learn(resource);
      if (cached)
        prefetchAfter(key);
      control_->connections = static_cast<int>(prefetched_.size());
      return resource;
    }
    catch (const HttpError& error) {
      if (!error.retryable || (tries > 0 && attempt + 1 >= tries) ||
          control_->cancel)
        throw;
      const auto delay = std::max<curl_off_t>(
          std::max<int64_t>(1, option_->integer("retry-wait")),
          error.retryAfter);
      std::unique_lock<std::mutex> lock(control_->mutex);
      control_->wake.wait_for(lock, std::chrono::seconds(delay),
                              [&] { return control_->cancel.load(); });
    }
  }
}

std::string Transport::resourceKey(const std::string& url, int64_t begin,
                                   int64_t end)
{
  return fingerprint(url + "\n" + std::to_string(begin) + "\n" +
                     std::to_string(end));
}

void Transport::invalidate()
{
  retained_.clear();
  prefetched_.clear();
  playlists_.clear();
  positions_.clear();
}

void Transport::learn(const Resource& resource)
{
  constexpr int64_t maximumPlaylist = 16 * 1024 * 1024;
  if (resource.size <= 0 || resource.size > maximumPlaylist)
    return;
  std::ifstream input(nativePath(resource.path), std::ios::binary);
  std::string line;
  if (!std::getline(input, line) || line.rfind("#EXTM3U", 0) != 0)
    return;
  std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> base(curl_url(),
                                                           curl_url_cleanup);
  if (!base || curl_url_set(base.get(), CURLUPART_URL, resource.url.c_str(),
                            0) != CURLUE_OK)
    return;
  std::vector<Location> entries;
  std::optional<std::pair<int64_t, int64_t>> range;
  int64_t nextOffset = 0;
  bool media = false;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.rfind("#EXTINF:", 0) == 0) {
      media = true;
      continue;
    }
    if (line.rfind("#EXT-X-BYTERANGE:", 0) == 0) {
      // length[@offset]; without an offset the range follows the previous one.
      // Prefetch is an optimization; the native client validates playlists.
      const auto value = line.substr(17);
      const auto at = value.find('@');
      const auto length = text::toInt(value.substr(0, at));
      const auto offset = at == std::string::npos
                              ? std::optional<int64_t>(nextOffset)
                              : text::toInt(value.substr(at + 1));
      if (!length || !offset || *length <= 0 || *offset < 0)
        return;
      range = std::make_pair(*offset, *offset + *length - 1);
      nextOffset = *offset + *length;
      continue;
    }
    if (line.empty() || line.front() == '#')
      continue;
    std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> target(
        curl_url_dup(base.get()), curl_url_cleanup);
    char* resolved = nullptr;
    if (target &&
        curl_url_set(target.get(), CURLUPART_URL, line.c_str(), 0) ==
            CURLUE_OK &&
        curl_url_get(target.get(), CURLUPART_URL, &resolved, 0) == CURLUE_OK) {
      std::unique_ptr<char, decltype(&curl_free)> owned(resolved, curl_free);
      entries.push_back(range ? Location{resolved, range->first, range->second}
                              : Location{resolved});
    }
    range.reset();
  }
  if (!media)
    return;
  for (auto position = positions_.begin(); position != positions_.end();) {
    if (position->second.first == resource.url)
      position = positions_.erase(position);
    else
      ++position;
  }
  for (size_t i = 0; i < entries.size(); ++i)
    positions_[resourceKey(entries[i].url, entries[i].begin, entries[i].end)] =
        {resource.url, i};
  playlists_[resource.url] = std::move(entries);
}

void Transport::prefetchAfter(const std::string& key)
{
  const auto position = positions_.find(key);
  if (position == positions_.end() || prefetchLimit_ == 0)
    return;
  const auto& entries = playlists_[position->second.first];
  for (auto index = position->second.second + 1;
       index < entries.size() && prefetched_.size() < prefetchLimit_; ++index) {
    const auto& entry = entries[index];
    const auto next = resourceKey(entry.url, entry.begin, entry.end);
    if (prefetched_.count(next) || retained_.count(next))
      continue;
    try {
      auto transfer = open(entry.url, entry.begin, entry.end,
                           utf8(nativePath(directory_) / next) + ".partial");
      transfer->start();
      prefetched_.emplace(next, std::move(transfer));
    }
    catch (const std::exception& error) {
      log::debug("media: prefetch skipped: {}", error.what());
      return;
    }
  }
}

namespace {
std::string decryptFile(const std::string& path,
                        const std::array<unsigned char, 16>& bytes,
                        const unsigned char* iv,
                        const std::shared_ptr<Control>& control)
{
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx || !EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr,
                                  bytes.data(), iv))
    throw std::runtime_error("Cannot initialize HLS AES-128 decryption");
  std::ifstream input(nativePath(path), std::ios::binary);
  auto clear = path + ".clear";
  std::ofstream output(nativePath(clear + ".partial"),
                       std::ios::binary | std::ios::trunc);
  std::array<unsigned char, 65536> in{};
  std::array<unsigned char, 65552> out{};
  while (input && !control->cancel) {
    input.read(reinterpret_cast<char*>(in.data()), in.size());
    int size = 0;
    if (!EVP_DecryptUpdate(ctx.get(), out.data(), &size, in.data(),
                           static_cast<int>(input.gcount())))
      throw std::runtime_error("HLS AES-128 decryption failed");
    output.write(reinterpret_cast<char*>(out.data()), size);
  }
  int size = 0;
  if (control->cancel || !input.eof())
    throw std::runtime_error("HLS decryption interrupted or unreadable input");
  if (!EVP_DecryptFinal_ex(ctx.get(), out.data(), &size))
    throw std::runtime_error("Invalid HLS AES-128 padding");
  output.write(reinterpret_cast<char*>(out.data()), size);
  output.close();
  if (!output)
    throw std::runtime_error("Cannot write decrypted media fragment");
  std::filesystem::remove(nativePath(clear));
  std::filesystem::rename(nativePath(clear + ".partial"), nativePath(clear));
  return clear;
}
} // namespace

std::string Transport::decrypt(const std::string& path,
                               const std::string& keyUrl,
                               const unsigned char* iv, bool cacheKey,
                               const std::string& init)
{
  auto verified = verifiedKeys_.find(keyUrl);
  std::vector<InputKey> candidates;
  if (cacheKey && verified != verifiedKeys_.end())
    candidates.push_back(verified->second);
  for (const auto& key : input_.keys)
    if ((key.url.empty() || key.url == keyUrl) &&
        std::none_of(
            candidates.begin(), candidates.end(), [&](const auto& candidate) {
              return candidate.key == key.key && candidate.iv == key.iv;
            }))
      candidates.push_back(key);
  auto readable = [&](const std::string& file, bool packets) {
    muxing::Input probe;
    probe.control = control_.get();
    if (!init.empty())
      probe.files.push_back(init);
    probe.files.push_back(file);
    probe.open();
    if (!probe.context->nb_streams ||
        probe.context->probe_score < AVPROBE_SCORE_EXTENSION)
      throw std::runtime_error("Media does not have a recognizable container");
    if (packets)
      muxing::check(av_read_frame(probe.context, probe.packet));
  };
  if (keyUrl.empty()) {
    if (candidates.empty())
      return path;
    try {
      readable(path, false);
      return path;
    }
    catch (...) {
      if (control_->cancel)
        throw;
    }
  }
  if (!candidates.empty()) {
    for (const auto& key : candidates) {
      auto bytes = decodeMediaKey(key.key);
      const auto overrideIv = key.iv.empty() ? std::array<unsigned char, 16>{}
                                             : decodeMediaKey(key.iv);
      try {
        auto clear = decryptFile(
            path, bytes, key.iv.empty() ? iv : overrideIv.data(), control_);
        OPENSSL_cleanse(bytes.data(), bytes.size());
        if (candidates.size() > 1)
          readable(clear, true);
        if (cacheKey)
          verifiedKeys_[keyUrl] = key;
        return clear;
      }
      catch (...) {
        OPENSSL_cleanse(bytes.data(), bytes.size());
        std::error_code ignored;
        std::filesystem::remove(nativePath(path + ".clear"), ignored);
        std::filesystem::remove(nativePath(path + ".clear.partial"), ignored);
        if (control_->cancel || candidates.size() == 1)
          throw;
      }
    }
    throw Failure(FailureCode::UnsupportedSource,
                  "No supplied AES key produced readable media");
  }
  auto key = get(keyUrl, 0, -1, cacheKey);
  if (key.size != 16)
    throw std::runtime_error("HLS AES-128 key must contain 16 bytes");
  std::array<unsigned char, 16> bytes{};
  std::ifstream file(nativePath(key.path), std::ios::binary);
  file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  if (file.gcount() != 16)
    throw std::runtime_error("Cannot read HLS AES-128 key");
  file.close();
  if (cacheKey)
    retain(key.path);
  else
    std::filesystem::remove(nativePath(key.path));
  struct Erase {
    std::array<unsigned char, 16>& bytes;
    ~Erase() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
  } erase{bytes};
  return decryptFile(path, bytes, iv, control_);
}
} // namespace media
} // namespace aria2
