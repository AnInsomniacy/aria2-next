/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef D_MEDIA_REQUEST_H
#define D_MEDIA_REQUEST_H

#include "BufferedFile.h"
#include "MediaDownload.h"
#include <curl/curl.h>
#include <map>
#include <memory>
#include <string>

namespace aria2::media {

// One HTTP hop with its own easy handle, header storage and cache file. The
// task's multi handle retains connections and may run several requests at
// once; `completed` receives every finished transfer of that multi handle, so
// a request waits for its own completion without consuming another's result.
class Request {
public:
  Request(CURLM* multi, Control* control, const std::string& path,
          std::map<CURL*, CURLcode>& completed);
  ~Request();
  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;

  template <typename T> void set(CURLoption option, T value)
  {
    const auto result = curl_easy_setopt(easy_.get(), option, value);
    if (result != CURLE_OK)
      throw std::runtime_error(curl_easy_strerror(result));
  }

  void header(const std::string& line);
  CURL* handle() const { return easy_.get(); }
  size_t bytes() const { return bytes_; }

  // Starts the transfer without waiting; perform() later collects it.
  void start();
  // Drives the multi handle until this transfer ends. Throws HttpError for
  // transport and HTTP failures.
  void perform();

private:
  static size_t write(char* data, size_t size, size_t count,
                      void* opaque) noexcept;
  static int progress(void* opaque, curl_off_t, curl_off_t, curl_off_t,
                      curl_off_t) noexcept;
  void detach() noexcept;

  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy_{curl_easy_init(),
                                                            curl_easy_cleanup};
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers_{
      nullptr, curl_slist_free_all};
  CURLM* multi_;
  Control* control_;
  std::map<CURL*, CURLcode>& completed_;
  BufferedFile output_;
  size_t bytes_ = 0;
  bool started_ = false;
  bool metadata_ = false;
  bool metadataTooLarge_ = false;
};

} // namespace aria2::media
#endif // D_MEDIA_REQUEST_H
