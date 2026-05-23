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

namespace aria2 {

class CurlSession;
class PeerStat;

class CurlDownloadCommand : public AbstractCommand {
public:
  CurlDownloadCommand(cuid_t cuid, const std::shared_ptr<Request>& req,
                      const std::shared_ptr<FileEntry>& fileEntry,
                      RequestGroup* requestGroup, DownloadEngine* e);
  ~CurlDownloadCommand() CXX11_OVERRIDE;

private:
  bool execute() CXX11_OVERRIDE;

  bool executeInternal() CXX11_OVERRIDE;
  bool noCheck() const CXX11_OVERRIDE { return true; }

  void initialize();
  void applyRequestOptions();
  void applyFtpFamilyOptions();
  void applyMetadataProbeOptions();
  void finish(CURLcode result);
  bool finishMetadataProbe(long status);
  void completeCurrentSegment();
  void prepareKnownLengthStorage(int64_t length);
  std::string determineFilename() const;

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
  int64_t expectedLength_;
  int64_t responseLength_;
  std::string contentDisposition_;
  char errorBuffer_[CURL_ERROR_SIZE];
  std::vector<std::string> requestHeaders_;
  curl_slist* headerList_;
  std::string proxyUri_;
};

} // namespace aria2

#endif // D_CURL_DOWNLOAD_COMMAND_H
