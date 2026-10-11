/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef D_MEDIA_TRANSPORT_H
#define D_MEDIA_TRANSPORT_H
#include "MediaDownload.h"
#include "MediaRequestContext.h"
#include "MediaInput.h"
#include <curl/curl.h>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <stdexcept>

namespace aria2 {
class Option;
namespace media {
struct HttpError : Failure {
  HttpError(long status, CURLcode result, curl_off_t retryAfter);
  long status;
  bool retryable;
  curl_off_t retryAfter;
};
struct Resource {
  std::string path, url, mime;
  int64_t size = 0;
  int64_t utcStart = 0;
  std::map<std::string, std::string> headers;
};
class Request;
class Transport {
public:
  Transport(const Option* option, std::string source, std::string directory,
            std::shared_ptr<Control> control);
  ~Transport();
  Transport(const Transport&) = delete;
  Transport& operator=(const Transport&) = delete;
  Resource get(const std::string& url, int64_t begin = 0, int64_t end = -1,
               bool cached = true);
  std::string decrypt(const std::string& path, const std::string& keyUrl,
                      const unsigned char* iv, bool cacheKey,
                      const std::string& init = {});
  bool hasCustomKeys() const { return !input_.keys.empty(); }
  static std::string fingerprint(const std::string& value);
  static std::string digest(const std::string& path);
  void retain(const std::string& path);
  void invalidate();

private:
  const Option* option_;
  std::string source_, directory_;
  std::shared_ptr<Control> control_;
  CURLM* multi_ = nullptr;
  CURLSH* share_ = nullptr;
  // Cookie store revision already loaded into share_'s cookie jar.
  std::string loadedCookies_;
  std::map<std::string, std::string> retained_;
  std::vector<RequestContext> contexts_;
  InputPlan input_;
  std::set<std::string> suppliedManifests_;
  std::map<std::string, InputKey> verifiedKeys_;
  // Finished transfers of multi_, collected by the request that owns each.
  std::map<CURL*, CURLcode> completed_;
  // HLS media playlist entries in order, and each entry's resource key to its
  // playlist and position. Later VOD segments are fetched ahead in parallel
  // while the native client consumes them in order.
  struct Location {
    std::string url;
    int64_t begin = 0, end = -1;
  };
  std::map<std::string, std::vector<Location>> playlists_;
  std::map<std::string, std::pair<std::string, size_t>> positions_;
  std::map<std::string, std::unique_ptr<Request>> prefetched_;
  size_t prefetchLimit_ = 0;

  static std::string resourceKey(const std::string& url, int64_t begin,
                                 int64_t end);
  std::unique_ptr<Request> open(const std::string& url, int64_t begin,
                                int64_t end, const std::string& temporary);
  Resource describe(Request& transfer, const std::string& url, int64_t begin,
                    int64_t end, const std::string& temporary);
  Resource request(const std::string& url, int64_t begin, int64_t end,
                   const std::string& temporary);
  std::optional<Resource> collect(const std::string& key,
                                  const std::string& url, int64_t begin,
                                  int64_t end, const std::string& temporary);
  void learn(const Resource& resource);
  void prefetchAfter(const std::string& key);
};
} // namespace media
} // namespace aria2
#endif
