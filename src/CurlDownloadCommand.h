/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 AnInsomniacy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#ifndef D_CURL_DOWNLOAD_COMMAND_H
#define D_CURL_DOWNLOAD_COMMAND_H

#include "AbstractCommand.h"

#include <curl/curl.h>

#include <string>
#include <vector>

#include "Range.h"
#include "error_code.h"

namespace aria2 {

class CurlSession;
class PeerStat;

class CurlDownloadCommand : public AbstractCommand {
public:
  CurlDownloadCommand(cuid_t cuid, const std::shared_ptr<Request>& req,
                      const std::shared_ptr<FileEntry>& fileEntry,
                      RequestGroup* requestGroup, DownloadEngine* e);
  ~CurlDownloadCommand() CXX11_OVERRIDE;

  static long platformSslTrustOptions();
  static bool shouldDisableCurlProxy(const Option* option);
  static bool isRetryableHttpCurlError(CURLcode result);
  static bool supportsHttp2();
  static bool isRetryableHttpStatus(long status);
  static bool shouldFallbackMetadataHeadErrorToRangeProbe(
      bool metadataProbe, bool metadataRangeProbe, bool explicitHead,
      bool httpTransfer, CURLcode result);
  static bool shouldFallbackMetadataHeadStatusToRangeProbe(
      bool metadataProbe, bool metadataRangeProbe, bool explicitHead,
      bool httpTransfer, long status);
  static bool shouldFallbackMetadataRangeProbeToFullDownload(
      bool metadataProbe, bool metadataRangeProbe, long status);

private:
  bool execute() CXX11_OVERRIDE;

  bool executeInternal() CXX11_OVERRIDE;
  bool noCheck() const CXX11_OVERRIDE { return true; }

  void initialize();
  void applyRequestOptions();
  void applyCredentialOptions();
  void applyCookieAndNetrcOptions(bool hasExplicitCookie);
  void applyFtpFamilyOptions();
  void applyMetadataProbeOptions();
  void finish(CURLcode result);
  bool finishMetadataProbe(long status);
  void completeCurrentSegment();
  void prepareKnownLengthStorage(int64_t length);
  std::string determineFilename() const;
  bool isRangedHttpTransfer() const;
  bool isHttpTransfer() const;
  void retryHttpTransfer(CURLcode result);
  void retryMetadataProbeWithRange(const std::string& reason);
  void retryMetadataProbeWithRange(CURLcode result);
  void retryMetadataProbeAsFullDownload(const std::string& reason);
  void retryHttpStatus(long status);
  int httpRetryAfterDelaySeconds();
  void setRequestWakeAfter(int seconds);
  void validateRangeResponseBeforeBody();
  bool ensureWritableSegment();
  size_t writeBodyToStorage(const unsigned char* data, size_t length);
  bool inspectBodyBeforeWrite(const unsigned char* data, size_t length,
                              const unsigned char*& writeData,
                              size_t& writeLength);
  std::string getErrorPageTargetPath() const;

  static size_t writeCallback(char* ptr, size_t size, size_t nmemb,
                              void* userdata);
  static size_t headerCallback(char* ptr, size_t size, size_t nmemb,
                               void* userdata);

  size_t writeData(const unsigned char* data, size_t length);
  size_t writeHeader(const char* data, size_t length);

  CURL* easy_;
  CurlSession* session_;
  std::shared_ptr<PeerStat> peerStat_;
  bool initialized_;
  bool finished_;
  bool metadataProbe_;
  bool metadataRangeProbe_;
  bool rangeRequested_;
  bool rangeResponseValidated_;
  int64_t expectedLength_;
  int64_t responseLength_;
  Range expectedRange_;
  Range responseRange_;
  error_code::Value rangeProtocolErrorCode_;
  std::string contentDisposition_;
  std::string contentEncoding_;
  std::string contentType_;
  int retryAfterSeconds_;
  std::string rangeProtocolError_;
  std::string httpBodyError_;
  char errorBuffer_[CURL_ERROR_SIZE];
  std::string userPassword_;
  std::string requestCookie_;
  std::vector<std::string> requestHeaders_;
  std::vector<unsigned char> pendingBody_;
  bool bodyInspectionComplete_;
  curl_slist* headerList_;
  std::string proxyUri_;
};

} // namespace aria2

#endif // D_CURL_DOWNLOAD_COMMAND_H
