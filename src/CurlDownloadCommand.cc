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
#include "CurlDownloadCommand.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "CurlRequestContext.h"
#include "CurlSession.h"
#include "DiskAdaptor.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "DownloadFailureException.h"
#include "DlAbortEx.h"
#include "DlRetryEx.h"
#include "FileEntry.h"
#include "HttpErrorPageDetector.h"
#include "HttpHeader.h"
#include "HttpRangeValidator.h"
#include "LogFactory.h"
#include "Logger.h"
#include "Command.h"
#include "PeerStat.h"
#include "PieceStorage.h"
#include "DefaultProgressInfoFile.h"
#include "NullProgressInfoFile.h"
#include "Request.h"
#include "RequestGroup.h"
#include "Segment.h"
#include "SegmentMan.h"
#include "TimeA2.h"
#include "fmt.h"
#include "Option.h"
#include "error_code.h"
#include "message.h"
#include "prefs.h"
#include "uri.h"
#include "util.h"
#include "wallclock.h"

namespace aria2 {

CurlDownloadCommand::CurlDownloadCommand(
    cuid_t cuid, const std::shared_ptr<Request>& req,
    const std::shared_ptr<FileEntry>& fileEntry, RequestGroup* requestGroup,
    DownloadEngine* e)
    : AbstractCommand(cuid, req, fileEntry, requestGroup, e, nullptr, nullptr),
      easy_(nullptr),
      session_(nullptr),
      initialized_(false),
      finished_(false),
      metadataProbe_(false),
      metadataRangeProbe_(false),
      rangeRequested_(false),
      rangeResponseValidated_(false),
      expectedLength_(0),
      responseLength_(0),
      rangeProtocolErrorCode_(error_code::HTTP_PROTOCOL_ERROR),
      retryAfterSeconds_(0),
      bodyInspectionComplete_(false),
      headerList_(nullptr),
      resolveList_(nullptr)
{
  std::memset(errorBuffer_, 0, sizeof(errorBuffer_));
  peerStat_ = req->initPeerStat();
  peerStat_->downloadStart();
  if (getSegmentMan()) {
    getSegmentMan()->registerPeerStat(peerStat_);
  }
}

CurlDownloadCommand::~CurlDownloadCommand()
{
  if (session_ && easy_) {
    session_->remove(easy_);
  }
  if (headerList_) {
    curl_slist_free_all(headerList_);
  }
  if (resolveList_) {
    curl_slist_free_all(resolveList_);
  }
  if (easy_) {
    curl_easy_cleanup(easy_);
  }
  peerStat_->downloadStop();
  if (getSegmentMan()) {
    getSegmentMan()->updateFastestPeerStat(peerStat_);
  }
}

long CurlDownloadCommand::platformSslTrustOptions()
{
#if (defined(_WIN32) || defined(__APPLE__)) && defined(CURLSSLOPT_NATIVE_CA)
  return CURLSSLOPT_NATIVE_CA;
#else
  return 0L;
#endif
}

bool CurlDownloadCommand::shouldDisableCurlProxy(const Option* option)
{
  return option->get(PREF_PROXY_MODE) != V_AUTO;
}

bool CurlDownloadCommand::usesSystemDnsResolver(const Option* option)
{
  return option->get(PREF_DNS_RESOLVER) != V_ASYNC;
}

std::string CurlDownloadCommand::makeCurlResolveEntry(
    const std::string& host, uint16_t port,
    const std::vector<std::string>& addresses)
{
  std::string entry = host;
  entry += ":";
  entry += util::uitos(port);
  entry += ":";
  for (size_t i = 0; i < addresses.size(); ++i) {
    if (i != 0) {
      entry += ",";
    }
    const auto& address = addresses[i];
    if (address.find(':') != std::string::npos) {
      entry += "[";
      entry += address;
      entry += "]";
    }
    else {
      entry += address;
    }
  }
  return entry;
}

bool CurlDownloadCommand::isRetryableHttpCurlError(CURLcode result)
{
  switch (result) {
  case CURLE_COULDNT_CONNECT:
  case CURLE_OPERATION_TIMEDOUT:
  case CURLE_RECV_ERROR:
  case CURLE_SEND_ERROR:
  case CURLE_GOT_NOTHING:
  case CURLE_PARTIAL_FILE:
  case CURLE_WRITE_ERROR:
    return true;
  default:
    return false;
  }
}

bool CurlDownloadCommand::supportsHttp2()
{
#if defined(CURL_VERSION_HTTP2) && defined(CURL_HTTP_VERSION_2TLS)
  auto info = curl_version_info(CURLVERSION_NOW);
  return info && (info->features & CURL_VERSION_HTTP2);
#else
  return false;
#endif
}

bool CurlDownloadCommand::isHttpRedirectStatus(long status)
{
  return status == 300 || status == 301 || status == 302 || status == 303 ||
         status == 307 || status == 308;
}

bool CurlDownloadCommand::isRetryableHttpStatus(long status)
{
  return status == 429 || status == 503;
}

bool CurlDownloadCommand::shouldFallbackMetadataHeadErrorToRangeProbe(
    bool metadataProbe, bool metadataRangeProbe, bool explicitHead,
    bool httpTransfer, CURLcode result)
{
  return metadataProbe && !metadataRangeProbe && !explicitHead && httpTransfer &&
         isRetryableHttpCurlError(result);
}

bool CurlDownloadCommand::shouldFallbackMetadataHeadStatusToRangeProbe(
    bool metadataProbe, bool metadataRangeProbe, bool explicitHead,
    bool httpTransfer, long status)
{
  return metadataProbe && !metadataRangeProbe && !explicitHead && httpTransfer &&
         (status == 405 || status == 501);
}

bool CurlDownloadCommand::shouldFallbackMetadataRangeProbeToFullDownload(
    bool metadataProbe, bool metadataRangeProbe, long status)
{
  return metadataProbe && metadataRangeProbe && status == 200;
}

bool CurlDownloadCommand::execute()
{
  return AbstractCommand::execute();
}

bool CurlDownloadCommand::executeInternal()
{
  if (!initialized_) {
    initialize();
  }

  session_->perform();

  CURLcode result = CURLE_OK;
  if (session_->takeDoneResult(easy_, result)) {
    finish(result);
    return true;
  }

  addCommandSelf();
  getDownloadEngine()->scheduleRuntimeWake(std::chrono::milliseconds(10));
  return false;
}

void CurlDownloadCommand::initialize()
{
  easy_ = curl_easy_init();
  if (!easy_) {
    throw DOWNLOAD_FAILURE_EXCEPTION("Failed to initialize libcurl easy.");
  }

  session_ = &getDownloadEngine()->getCurlSession();

  expectedLength_ = getFileEntry()->getLength();
  if (getSegmentMan()) {
    getSegmentMan()->registerPeerStat(peerStat_);
  }

  curl_easy_setopt(easy_, CURLOPT_URL, getRequest()->getCurrentUri().c_str());
  curl_easy_setopt(easy_, CURLOPT_HEADERFUNCTION,
                   &CurlDownloadCommand::headerCallback);
  curl_easy_setopt(easy_, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(easy_, CURLOPT_WRITEFUNCTION,
                   &CurlDownloadCommand::writeCallback);
  curl_easy_setopt(easy_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(easy_, CURLOPT_ERRORBUFFER, errorBuffer_);
  curl_easy_setopt(easy_, CURLOPT_FOLLOWLOCATION,
                   usesSystemDnsResolver(getOption().get()) ? 0L : 1L);
  curl_easy_setopt(easy_, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(easy_, CURLOPT_FAILONERROR, 0L);
  applyRequestOptions();
  applyDnsResolverOptions();

  metadataProbe_ = !getPieceStorage();
  metadataRangeProbe_ =
      metadataProbe_ && isHttpTransfer() &&
      getRequest()->getMethod() != Request::METHOD_HEAD &&
      getRequestGroup()->shouldUseHttpMetadataRangeProbe();
  if (metadataProbe_) {
    applyMetadataProbeOptions();
    session_->add(easy_, this);
    session_->perform();
    initialized_ = true;
    return;
  }

  const auto& segments = getSegments();
  if (!segments.empty() && expectedLength_ > 0 &&
      getRequestGroup()->shouldUseHttpRange()) {
    const auto& segment = segments.front();
    const auto begin = getFileEntry()->gtoloff(segment->getPositionToWrite());
    const auto end =
        getFileEntry()->gtoloff(segment->getPosition() + segment->getLength()) -
        1;
    expectedRange_ = Range(begin, end, expectedLength_);
    rangeRequested_ = true;
    if (isRangedHttpTransfer()) {
      curl_easy_setopt(easy_, CURLOPT_ACCEPT_ENCODING, "identity");
    }
    curl_easy_setopt(easy_, CURLOPT_RANGE,
                     fmt("%lld-%lld", static_cast<long long>(begin),
                         static_cast<long long>(end))
                         .c_str());
  }

  session_->add(easy_, this);
  session_->perform();
  initialized_ = true;
}

namespace {
void appendHeaders(curl_slist*& headers,
                   const std::vector<std::string>& headerValues)
{
  for (const auto& header : headerValues) {
    headers = curl_slist_append(headers, header.c_str());
  }
}

std::vector<std::string> splitCumulativeOption(const std::string& value)
{
  std::vector<std::string> entries;
  util::split(value.begin(), value.end(), std::back_inserter(entries), '\n',
              false, false);
  return entries;
}

bool isFtpFamily(const std::string& protocol)
{
  return protocol == "ftp" || protocol == "ftps" || protocol == "sftp" ||
         protocol == "scp";
}

std::string makeUserPassword(const std::string& user,
                             const std::string& password)
{
  std::string userPassword = user;
  userPassword += ":";
  userPassword += password;
  return userPassword;
}
} // namespace

void CurlDownloadCommand::applyRequestOptions()
{
  auto option = getOption();
  const auto& protocol = getRequest()->getProtocol();
  auto headerValues = splitCumulativeOption(option->get(PREF_HEADER));
  if (option->getAsBool(PREF_HTTP_NO_CACHE)) {
    headerValues.push_back("Cache-Control: no-cache");
    headerValues.push_back("Pragma: no-cache");
  }

  CurlRequestContextInput contextInput;
  contextInput.userAgent = option->get(PREF_USER_AGENT);
  if (!option->blank(PREF_REFERER)) {
    contextInput.referer = option->get(PREF_REFERER);
  }
  contextInput.httpAcceptGzip = option->getAsBool(PREF_HTTP_ACCEPT_GZIP);
  contextInput.headers = std::move(headerValues);
  auto requestContext = buildCurlRequestContext(contextInput);

  curl_easy_setopt(easy_, CURLOPT_PROTOCOLS_STR,
                   "http,https,ftp,ftps,sftp,scp");
  curl_easy_setopt(easy_, CURLOPT_REDIR_PROTOCOLS_STR,
                   "http,https,ftp,ftps,sftp,scp");
  curl_easy_setopt(easy_, CURLOPT_USERAGENT, requestContext.userAgent.c_str());

  if (requestContext.hasReferer) {
    curl_easy_setopt(easy_, CURLOPT_REFERER, requestContext.referer.c_str());
  }

  if (getRequest()->getMethod() == Request::METHOD_HEAD) {
    curl_easy_setopt(easy_, CURLOPT_NOBODY, 1L);
  }

  if (requestContext.hasAcceptEncoding) {
    curl_easy_setopt(easy_, CURLOPT_ACCEPT_ENCODING,
                     requestContext.acceptEncoding.c_str());
  }

  if ((protocol == "http" || protocol == "https") && supportsHttp2()) {
#if defined(CURL_HTTP_VERSION_2TLS)
    curl_easy_setopt(easy_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
#endif
  }

  curl_easy_setopt(easy_, CURLOPT_CONNECTTIMEOUT,
                   static_cast<long>(option->getAsInt(PREF_CONNECT_TIMEOUT)));

  const auto lowestSpeedLimit = option->getAsInt(PREF_LOWEST_SPEED_LIMIT);
  curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_LIMIT,
                   static_cast<long>(std::max(lowestSpeedLimit, 1)));
  curl_easy_setopt(easy_, CURLOPT_LOW_SPEED_TIME,
                   static_cast<long>(option->getAsInt(PREF_TIMEOUT)));

  applyCredentialOptions();
  if (requestContext.hasCookie) {
    requestCookie_ = requestContext.cookie;
    curl_easy_setopt(easy_, CURLOPT_COOKIE, requestCookie_.c_str());
  }
  applyCookieAndNetrcOptions(requestContext.hasCookie);
  if (isFtpFamily(protocol)) {
    applyFtpFamilyOptions();
  }

  proxyUri_ = resolveProxyUri(getRequest(), option.get());
  if (shouldDisableCurlProxy(option.get())) {
    curl_easy_setopt(easy_, CURLOPT_PROXY, "");
  }
  if (!proxyUri_.empty()) {
    curl_easy_setopt(easy_, CURLOPT_PROXY, proxyUri_.c_str());
    if (option->get(PREF_PROXY_METHOD) == V_TUNNEL) {
      curl_easy_setopt(easy_, CURLOPT_HTTPPROXYTUNNEL, 1L);
    }
  }

  if (protocol == "https" || protocol == "ftps") {
    const auto verify = option->getAsBool(PREF_CHECK_CERTIFICATE) ? 1L : 0L;
    curl_easy_setopt(easy_, CURLOPT_SSL_VERIFYPEER, verify);
    curl_easy_setopt(easy_, CURLOPT_SSL_VERIFYHOST, verify ? 2L : 0L);
    const auto sslOptions = platformSslTrustOptions();
    if (sslOptions != 0L) {
      curl_easy_setopt(easy_, CURLOPT_SSL_OPTIONS, sslOptions);
    }
    if (!option->blank(PREF_CA_CERTIFICATE)) {
      curl_easy_setopt(easy_, CURLOPT_CAINFO,
                       option->get(PREF_CA_CERTIFICATE).c_str());
    }
    if (!option->blank(PREF_CERTIFICATE)) {
      curl_easy_setopt(easy_, CURLOPT_SSLCERT,
                       option->get(PREF_CERTIFICATE).c_str());
    }
    if (!option->blank(PREF_PRIVATE_KEY)) {
      curl_easy_setopt(easy_, CURLOPT_SSLKEY,
                       option->get(PREF_PRIVATE_KEY).c_str());
    }
  }

  if (protocol == "ftps") {
    curl_easy_setopt(easy_, CURLOPT_USE_SSL, CURLUSESSL_ALL);
  }

  requestHeaders_ = std::move(requestContext.headers);
  appendHeaders(headerList_, requestHeaders_);
  if (headerList_) {
#ifdef CURLHEADER_SEPARATE
    curl_easy_setopt(easy_, CURLOPT_HEADEROPT, CURLHEADER_SEPARATE);
#endif
    curl_easy_setopt(easy_, CURLOPT_HTTPHEADER, headerList_);
  }
}

void CurlDownloadCommand::applyCredentialOptions()
{
  auto option = getOption();
  const auto& protocol = getRequest()->getProtocol();

  if (protocol == "http" || protocol == "https") {
    if (!option->blank(PREF_HTTP_USER)) {
      userPassword_ =
          makeUserPassword(option->get(PREF_HTTP_USER),
                           option->get(PREF_HTTP_PASSWD));
      curl_easy_setopt(easy_, CURLOPT_USERPWD, userPassword_.c_str());
      curl_easy_setopt(easy_, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    }
    return;
  }

  if (!isFtpFamily(protocol)) {
    return;
  }

  if (!getRequest()->getUsername().empty()) {
    curl_easy_setopt(easy_, CURLOPT_USERNAME,
                     getRequest()->getUsername().c_str());
    curl_easy_setopt(easy_, CURLOPT_PASSWORD,
                     getRequest()->getPassword().c_str());
  }
  else if (!option->blank(PREF_FTP_USER)) {
    curl_easy_setopt(easy_, CURLOPT_USERNAME,
                     option->get(PREF_FTP_USER).c_str());
    curl_easy_setopt(easy_, CURLOPT_PASSWORD,
                     option->get(PREF_FTP_PASSWD).c_str());
  }
}

void CurlDownloadCommand::applyCookieAndNetrcOptions(bool hasExplicitCookie)
{
  auto option = getOption();

  if (!hasExplicitCookie) {
    curl_easy_setopt(easy_, CURLOPT_COOKIEFILE, "");
  }
  if (!hasExplicitCookie && !option->blank(PREF_LOAD_COOKIES)) {
    curl_easy_setopt(easy_, CURLOPT_COOKIEFILE,
                     option->get(PREF_LOAD_COOKIES).c_str());
  }
  if (!option->blank(PREF_SAVE_COOKIES)) {
    curl_easy_setopt(easy_, CURLOPT_COOKIEJAR,
                     option->get(PREF_SAVE_COOKIES).c_str());
  }

  if (!option->getAsBool(PREF_NO_NETRC)) {
    curl_easy_setopt(easy_, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
    if (!option->blank(PREF_NETRC_PATH)) {
      curl_easy_setopt(easy_, CURLOPT_NETRC_FILE,
                       option->get(PREF_NETRC_PATH).c_str());
    }
  }
  else {
    curl_easy_setopt(easy_, CURLOPT_NETRC, CURL_NETRC_IGNORED);
  }
}

void CurlDownloadCommand::applyFtpFamilyOptions()
{
  auto option = getOption();

  if (option->get(PREF_FTP_TYPE) == V_ASCII) {
    curl_easy_setopt(easy_, CURLOPT_TRANSFERTEXT, 1L);
  }
  if (!option->getAsBool(PREF_FTP_PASV)) {
    curl_easy_setopt(easy_, CURLOPT_FTPPORT, "-");
  }
  if ((getRequest()->getProtocol() == "sftp" ||
       getRequest()->getProtocol() == "scp") &&
      !option->blank(PREF_PRIVATE_KEY)) {
    curl_easy_setopt(easy_, CURLOPT_SSH_PRIVATE_KEYFILE,
                     option->get(PREF_PRIVATE_KEY).c_str());
  }
}

void CurlDownloadCommand::applyDnsResolverOptions()
{
  auto option = getOption();
  if (option->getAsBool(PREF_DISABLE_IPV6)) {
    curl_easy_setopt(easy_, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
  }

  if (!usesSystemDnsResolver(option.get())) {
    return;
  }

  auto host = getRequest()->getHost();
  auto uriHost = getRequest()->getURIHost();
  auto port = getRequest()->getPort();
  if (!proxyUri_.empty()) {
    auto proxyRequest = createProxyRequest();
    if (!proxyRequest) {
      return;
    }
    host = proxyRequest->getHost();
    uriHost = proxyRequest->getURIHost();
    port = proxyRequest->getPort();
  }
  if (util::isNumericHost(host)) {
    return;
  }
  std::vector<std::string> addresses;
  resolveHostname(addresses, host, port);
  if (addresses.empty()) {
    throw DOWNLOAD_FAILURE_EXCEPTION(
        fmt("Failed to resolve hostname %s.", host.c_str()));
  }

  auto entry = makeCurlResolveEntry(uriHost, port, addresses);
  resolveList_ = curl_slist_append(resolveList_, entry.c_str());
  curl_easy_setopt(easy_, CURLOPT_RESOLVE, resolveList_);
}

void CurlDownloadCommand::applyMetadataProbeOptions()
{
  curl_easy_setopt(easy_, CURLOPT_ACCEPT_ENCODING, "identity");
  if (metadataRangeProbe_) {
    curl_easy_setopt(easy_, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(easy_, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(easy_, CURLOPT_RANGE, "0-0");
    return;
  }
  curl_easy_setopt(easy_, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(easy_, CURLOPT_HTTPGET, 0L);
  curl_easy_setopt(easy_, CURLOPT_RANGE, nullptr);
}

void CurlDownloadCommand::finish(CURLcode result)
{
  finished_ = true;
  long status = 0;
  curl_easy_getinfo(easy_, CURLINFO_RESPONSE_CODE, &status);
  curl_off_t contentLength = -1;
  if (curl_easy_getinfo(easy_, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                        &contentLength) == CURLE_OK &&
      contentLength >= 0) {
    responseLength_ = contentLength;
  }

  if (result != CURLE_OK) {
    if (!httpBodyError_.empty()) {
      throw DL_ABORT_EX2(httpBodyError_, error_code::HTTP_PROTOCOL_ERROR);
    }
    if (!rangeProtocolError_.empty()) {
      if (rangeProtocolErrorCode_ == error_code::CANNOT_RESUME) {
        getRequestGroup()->disableHttpRangeForDownload();
        throw DL_RETRY_EX2(rangeProtocolError_, rangeProtocolErrorCode_);
      }
      throw DL_RETRY_EX2(rangeProtocolError_, rangeProtocolErrorCode_);
    }
    if (shouldFallbackMetadataHeadErrorToRangeProbe(
            metadataProbe_, metadataRangeProbe_,
            getRequest()->getMethod() == Request::METHOD_HEAD, isHttpTransfer(),
            result)) {
      retryMetadataProbeWithRange(result);
    }
    if (isHttpTransfer() && isRetryableHttpCurlError(result)) {
      retryHttpTransfer(result);
    }
    throw DOWNLOAD_FAILURE_EXCEPTION(
        fmt("libcurl transfer failed: %s",
            errorBuffer_[0] ? errorBuffer_ : curl_easy_strerror(result)));
  }
  if (followManualRedirect(status)) {
    return;
  }
  if (status >= 400) {
    if (shouldFallbackMetadataHeadStatusToRangeProbe(
            metadataProbe_, metadataRangeProbe_,
            getRequest()->getMethod() == Request::METHOD_HEAD, isHttpTransfer(),
            status)) {
      retryMetadataProbeWithRange(
          fmt("HTTP metadata HEAD probe returned status %ld", status));
    }
    if (isRetryableHttpStatus(status)) {
      retryHttpStatus(status);
    }
    throw DOWNLOAD_FAILURE_EXCEPTION(
        fmt("HTTP transfer failed with status %ld.", status));
  }

  if (metadataProbe_ && finishMetadataProbe(status)) {
    return;
  }

  if (isRangedHttpTransfer()) {
    validateRangeResponseBeforeBody();
  }

  completeCurrentSegment();
  if (isRangedHttpTransfer()) {
    getRequestGroup()->noteHttpSegmentSuccess(getRequest());
  }
  if (getRequestGroup()->downloadFinished()) {
    getRequestGroup()->queueChecksumValidationIfNeeded(getDownloadEngine());
    getDownloadEngine()->setNoWait(true);
    getDownloadEngine()->setRefreshInterval(std::chrono::milliseconds(0));
    return;
  }

  auto numNext = getRequestGroup()->countNextCommandForCompletedStream();
  if (numNext <= 0) {
    getFileEntry()->poolRequest(getRequest());
    return;
  }

  getDownloadEngine()->addCommand(
      make_unique<CurlDownloadCommand>(getCuid(), getRequest(), getFileEntry(),
                                       getRequestGroup(), getDownloadEngine()));
  --numNext;
  if (numNext > 0) {
    std::vector<std::unique_ptr<Command>> commands;
    getRequestGroup()->createNextCommand(commands, getDownloadEngine(),
                                         numNext);
    getDownloadEngine()->addCommand(std::move(commands));
  }
}

bool CurlDownloadCommand::followManualRedirect(long status)
{
  if (!usesSystemDnsResolver(getOption().get()) || !isHttpTransfer() ||
      !isHttpRedirectStatus(status)) {
    return false;
  }
  if (location_.empty()) {
    throw DOWNLOAD_FAILURE_EXCEPTION(
        fmt("HTTP redirect status %ld did not include Location.", status));
  }
  if (getRequest()->getRedirectCount() >= Request::MAX_REDIRECT) {
    throw DOWNLOAD_FAILURE_EXCEPTION("Too many redirects.");
  }
  if (!getRequest()->redirectUri(location_)) {
    throw DOWNLOAD_FAILURE_EXCEPTION(
        fmt("Invalid redirect URI: %s", location_.c_str()));
  }
  A2_LOG_NOTICE(fmt(MSG_REDIRECT, getCuid(),
                    getRequest()->getCurrentUri().c_str()));
  getDownloadEngine()->addCommand(
      make_unique<CurlDownloadCommand>(getCuid(), getRequest(), getFileEntry(),
                                       getRequestGroup(), getDownloadEngine()));
  return true;
}

bool CurlDownloadCommand::finishMetadataProbe(long status)
{
  if (status == 304) {
    prepareKnownLengthStorage(responseLength_);
    getPieceStorage()->markAllPiecesDone();
    getDownloadContext()->setChecksumVerified(true);
    getDownloadEngine()->setNoWait(true);
    getDownloadEngine()->setRefreshInterval(std::chrono::milliseconds(0));
    return true;
  }

  if (getRequest()->getMethod() == Request::METHOD_HEAD) {
    prepareKnownLengthStorage(responseLength_);
    getRequestGroup()->queueChecksumValidationIfNeeded(getDownloadEngine());
    getDownloadEngine()->setNoWait(true);
    getDownloadEngine()->setRefreshInterval(std::chrono::milliseconds(0));
    return true;
  }

  if (metadataRangeProbe_) {
    if (shouldFallbackMetadataRangeProbeToFullDownload(
            metadataProbe_, metadataRangeProbe_, status)) {
      retryMetadataProbeAsFullDownload(
          "HTTP metadata range probe was ignored by the server");
    }
    auto result =
        validateHttpMetadataRangeProbe(status, responseRange_, contentEncoding_);
    if (!result.ok) {
      throw DL_ABORT_EX2(result.error, error_code::HTTP_PROTOCOL_ERROR);
    }
    responseLength_ = result.entityLength;
  }
  else if (isHttpTransfer()) {
    auto result =
        validateHttpMetadataHead(status, responseLength_, contentEncoding_);
    if (!result.ok && result.needsRangeProbe) {
      getRequestGroup()->requireHttpMetadataRangeProbe();
      throw DL_RETRY_EX2(result.error, error_code::HTTP_PROTOCOL_ERROR);
    }
    if (!result.ok) {
      throw DL_ABORT_EX2(result.error, error_code::HTTP_PROTOCOL_ERROR);
    }
    responseLength_ = result.entityLength;
  }

  if (responseLength_ <= 0) {
    getDownloadContext()->markTotalLengthIsUnknown();
    prepareKnownLengthStorage(0);
    getSegmentMan()->getSegment(getCuid(), 1);
    return false;
  }

  prepareKnownLengthStorage(responseLength_);
  if (getRequestGroup()->downloadFinished()) {
    getRequestGroup()->queueChecksumValidationIfNeeded(getDownloadEngine());
    getDownloadEngine()->setNoWait(true);
    getDownloadEngine()->setRefreshInterval(std::chrono::milliseconds(0));
    return true;
  }
  getFileEntry()->poolRequest(getRequest());
  std::vector<std::unique_ptr<Command>> commands;
  getRequestGroup()->createNextCommandForCompletedStream(commands,
                                                         getDownloadEngine());
  getDownloadEngine()->setNoWait(true);
  getDownloadEngine()->addCommand(std::move(commands));
  return true;
}

void CurlDownloadCommand::prepareKnownLengthStorage(int64_t length)
{
  if (length < 0) {
    throw DL_ABORT_EX2("Content-Length must be positive integer",
                       error_code::HTTP_PROTOCOL_ERROR);
  }
  if (length > std::numeric_limits<a2_off_t>::max()) {
    throw DOWNLOAD_FAILURE_EXCEPTION(fmt(EX_TOO_LARGE_FILE, length));
  }

  getFileEntry()->setLength(length);
  if (getFileEntry()->getPath().empty()) {
    auto suffixPath = util::createSafePath(determineFilename());
    getFileEntry()->setPath(
        util::applyDir(getOption()->get(PREF_DIR), suffixPath));
    getFileEntry()->setSuffixPath(suffixPath);
  }
  getRequestGroup()->adjustFilename(
      std::make_shared<NullProgressInfoFile>());
  getRequestGroup()->initPieceStorage();
  auto progressInfoFile = std::make_shared<DefaultProgressInfoFile>(
      getDownloadContext(), getPieceStorage(), getOption().get());
  getRequestGroup()->loadAndOpenFile(progressInfoFile);
}

std::string CurlDownloadCommand::determineFilename() const
{
  auto contentDisposition = util::getContentDispositionFilename(
      contentDisposition_,
      getOption()->getAsBool(PREF_CONTENT_DISPOSITION_DEFAULT_UTF8));
  if (!contentDisposition.empty()) {
    A2_LOG_INFO(fmt(MSG_CONTENT_DISPOSITION_DETECTED, getCuid(),
                    contentDisposition.c_str()));
    return contentDisposition;
  }

  auto file = getRequest()->getFile();
  file = util::percentDecode(file.begin(), file.end());
  if (file.empty()) {
    return Request::DEFAULT_FILE;
  }
  return file;
}

bool CurlDownloadCommand::isRangedHttpTransfer() const
{
  if (!rangeRequested_) {
    return false;
  }
  return isHttpTransfer();
}

bool CurlDownloadCommand::isHttpTransfer() const
{
  const auto& protocol = getRequest()->getProtocol();
  return protocol == "http" || protocol == "https";
}

void CurlDownloadCommand::retryHttpTransfer(CURLcode result)
{
  if (isRangedHttpTransfer()) {
    getRequestGroup()->noteHttpSegmentFailure(getRequest());
  }
  throw DL_RETRY_EX2(
      fmt("libcurl transfer failed: %s",
          errorBuffer_[0] ? errorBuffer_ : curl_easy_strerror(result)),
      result == CURLE_OPERATION_TIMEDOUT ? error_code::TIME_OUT
                                         : error_code::NETWORK_PROBLEM);
}

void CurlDownloadCommand::retryMetadataProbeWithRange(CURLcode result)
{
  retryMetadataProbeWithRange(
      fmt("HTTP metadata HEAD probe failed: %s",
          errorBuffer_[0] ? errorBuffer_ : curl_easy_strerror(result)));
}

void CurlDownloadCommand::retryMetadataProbeWithRange(const std::string& reason)
{
  getRequestGroup()->requireHttpMetadataRangeProbe();
  throw DL_RETRY_EX2(
      fmt("%s. Retrying with Range: bytes=0-0.", reason.c_str()),
      error_code::NETWORK_PROBLEM);
}

void CurlDownloadCommand::retryMetadataProbeAsFullDownload(
    const std::string& reason)
{
  getDownloadContext()->markTotalLengthIsUnknown();
  prepareKnownLengthStorage(0);
  throw DL_RETRY_EX2(
      fmt("%s. Restarting as an unknown-length full download.",
          reason.c_str()),
      error_code::CANNOT_RESUME);
}

void CurlDownloadCommand::retryHttpStatus(long status)
{
  if (isRangedHttpTransfer() && status == 429) {
    getRequestGroup()->noteHttpRateLimited(getRequest());
  }
  else if (isRangedHttpTransfer()) {
    getRequestGroup()->noteHttpSegmentFailure(getRequest());
  }

  const auto delay = httpRetryAfterDelaySeconds();
  setRequestWakeAfter(delay);
  throw DL_RETRY_EX2(fmt("HTTP transfer rate limited with status %ld. "
                         "Retrying after %d second(s).",
                         status, delay),
                     status == 503 ? error_code::HTTP_SERVICE_UNAVAILABLE
                                   : error_code::NETWORK_PROBLEM);
}

int CurlDownloadCommand::httpRetryAfterDelaySeconds()
{
  curl_off_t retryAfter = 0;
#ifdef CURLINFO_RETRY_AFTER
  if (curl_easy_getinfo(easy_, CURLINFO_RETRY_AFTER, &retryAfter) != CURLE_OK ||
      retryAfter <= 0) {
    retryAfter = retryAfterSeconds_;
  }
#else
  retryAfter = retryAfterSeconds_;
#endif
  if (retryAfter <= 0) {
    retryAfter = std::max(1, getOption()->getAsInt(PREF_RETRY_WAIT));
  }
  return static_cast<int>(std::min<curl_off_t>(retryAfter, 300));
}

void CurlDownloadCommand::setRequestWakeAfter(int seconds)
{
  auto wakeTime = global::wallclock();
  wakeTime.advance(std::chrono::seconds(std::max(1, seconds)));
  getRequest()->setWakeTime(wakeTime);
  getRequest()->setResetTryCountAfterWake(true);
}

void CurlDownloadCommand::validateRangeResponseBeforeBody()
{
  if (rangeResponseValidated_) {
    return;
  }

  long status = 0;
  curl_easy_getinfo(easy_, CURLINFO_RESPONSE_CODE, &status);
  if (isRetryableHttpStatus(status)) {
    retryHttpStatus(status);
  }
  auto result = validateHttpRangeResponse(
      status, expectedRange_, responseRange_, expectedLength_, contentEncoding_);
  if (!result.ok) {
    rangeProtocolError_ = result.error;
    rangeProtocolErrorCode_ = result.retryable ? error_code::HTTP_PROTOCOL_ERROR
                                               : error_code::CANNOT_RESUME;
    getRequestGroup()->noteHttpSegmentFailure(getRequest());
    if (result.rangeUnsupported) {
      getRequestGroup()->disableHttpRangeForDownload();
      throw DL_RETRY_EX2(result.error, rangeProtocolErrorCode_);
    }
    if (result.retryable) {
      throw DL_RETRY_EX2(result.error, rangeProtocolErrorCode_);
    }
    throw DL_ABORT_EX2(result.error, rangeProtocolErrorCode_);
  }
  rangeResponseValidated_ = true;
}

bool CurlDownloadCommand::ensureWritableSegment()
{
  auto& segments = getSegments();
  while (!segments.empty()) {
    const auto& segment = segments.front();
    if (segment->getLength() == 0 ||
        segment->getWrittenLength() < segment->getLength()) {
      return true;
    }
    completeCurrentSegment();
    segments.erase(segments.begin());
  }

  if (isRangedHttpTransfer()) {
    return false;
  }

  auto segment = getSegmentMan()->getSegment(getCuid(), 1);
  if (!segment) {
    return false;
  }
  segments.push_back(segment);
  return true;
}

void CurlDownloadCommand::completeCurrentSegment()
{
  const auto& segments = getSegments();
  if (segments.empty()) {
    return;
  }
  const auto& segment = segments.front();
  getPieceStorage()->flushWrDiskCacheEntry(false);
  getSegmentMan()->completeSegment(getCuid(), segment);
}

size_t CurlDownloadCommand::writeCallback(char* ptr, size_t size, size_t nmemb,
                                          void* userdata)
{
  auto self = static_cast<CurlDownloadCommand*>(userdata);
  return self->writeData(reinterpret_cast<const unsigned char*>(ptr),
                         size * nmemb);
}

size_t CurlDownloadCommand::headerCallback(char* ptr, size_t size, size_t nmemb,
                                           void* userdata)
{
  auto self = static_cast<CurlDownloadCommand*>(userdata);
  return self->writeHeader(ptr, size * nmemb);
}

size_t CurlDownloadCommand::writeData(const unsigned char* data, size_t length)
{
  if (length == 0) {
    return 0;
  }

  if (isHttpTransfer() && usesSystemDnsResolver(getOption().get())) {
    long status = 0;
    if (curl_easy_getinfo(easy_, CURLINFO_RESPONSE_CODE, &status) == CURLE_OK &&
        isHttpRedirectStatus(status)) {
      return length;
    }
  }

  if (metadataProbe_) {
    if (metadataRangeProbe_ && responseRange_.entityLength <= 0) {
      long status = 0;
      curl_easy_getinfo(easy_, CURLINFO_RESPONSE_CODE, &status);
      if (shouldFallbackMetadataRangeProbeToFullDownload(
              metadataProbe_, metadataRangeProbe_, status)) {
        return length;
      }
      httpBodyError_ =
          "HTTP metadata range probe did not include Content-Range.";
      return 0;
    }
    return length;
  }

  if (isRangedHttpTransfer()) {
    try {
      validateRangeResponseBeforeBody();
    }
    catch (const DlAbortEx& ex) {
      rangeProtocolError_ = ex.what();
      rangeProtocolErrorCode_ = ex.getErrorCode();
      return 0;
    }
    catch (const DlRetryEx& ex) {
      rangeProtocolError_ = ex.what();
      rangeProtocolErrorCode_ = ex.getErrorCode();
      return 0;
    }
  }

  const unsigned char* bodyData = data;
  size_t bodyLength = length;
  if (!inspectBodyBeforeWrite(data, length, bodyData, bodyLength)) {
    return httpBodyError_.empty() ? length : 0;
  }

  return writeBodyToStorage(bodyData, bodyLength);
}

bool CurlDownloadCommand::inspectBodyBeforeWrite(const unsigned char* data,
                                                 size_t length,
                                                 const unsigned char*& writeData,
                                                 size_t& writeLength)
{
  writeData = data;
  writeLength = length;

  if (bodyInspectionComplete_ || metadataProbe_ || isRangedHttpTransfer() ||
      !isHttpTransfer()) {
    return true;
  }

  constexpr size_t MAX_INSPECT = 32_k;
  const auto room = MAX_INSPECT > pendingBody_.size()
                        ? MAX_INSPECT - pendingBody_.size()
                        : 0;
  const auto capture = std::min(length, room);
  pendingBody_.insert(pendingBody_.end(), data, data + capture);

  const std::string prefix(reinterpret_cast<const char*>(pendingBody_.data()),
                           pendingBody_.size());
  auto decision =
      detectHttpErrorPage(getErrorPageTargetPath(), contentType_, prefix);
  if (decision.reject) {
    httpBodyError_ = decision.reason;
    return false;
  }

  bodyInspectionComplete_ = true;
  writeData = pendingBody_.data();
  writeLength = pendingBody_.size();
  if (capture < length) {
    pendingBody_.insert(pendingBody_.end(), data + capture, data + length);
    writeData = pendingBody_.data();
    writeLength = pendingBody_.size();
  }
  return true;
}

size_t CurlDownloadCommand::writeBodyToStorage(const unsigned char* data,
                                               size_t length)
{
  auto remaining = length;
  auto cursor = data;
  while (remaining > 0) {
    if (!ensureWritableSegment()) {
      return length - remaining;
    }
    const auto& segments = getSegments();
    const auto& segment = segments.front();
    const auto capacity =
        segment->getLength() > 0
            ? static_cast<size_t>(segment->getLength() -
                                  segment->getWrittenLength())
            : remaining;
    if (capacity == 0) {
      rangeProtocolError_ =
          "HTTP response body exceeded the requested segment range.";
      rangeProtocolErrorCode_ = error_code::HTTP_PROTOCOL_ERROR;
      getRequestGroup()->noteHttpSegmentFailure(getRequest());
      return 0;
    }
    const auto writeLength = std::min(remaining, capacity);
    getPieceStorage()->getDiskAdaptor()->writeData(
        cursor, writeLength, segment->getPositionToWrite());
    segment->updateWrittenLength(writeLength);
    peerStat_->updateDownload(writeLength);
    getDownloadContext()->updateDownload(writeLength);
    cursor += writeLength;
    remaining -= writeLength;
  }

  return length - remaining;
}

std::string CurlDownloadCommand::getErrorPageTargetPath() const
{
  if (!getFileEntry()->getPath().empty()) {
    return getFileEntry()->getPath();
  }
  if (!getFileEntry()->getSuffixPath().empty()) {
    return getFileEntry()->getSuffixPath();
  }
  auto file = getRequest()->getFile();
  if (!file.empty()) {
    return util::percentDecode(file.begin(), file.end());
  }
  return determineFilename();
}

size_t CurlDownloadCommand::writeHeader(const char* data, size_t length)
{
  constexpr char CONTENT_LENGTH[] = "content-length:";
  constexpr char CONTENT_DISPOSITION[] = "content-disposition:";
  constexpr char CONTENT_RANGE[] = "content-range:";
  constexpr char CONTENT_ENCODING[] = "content-encoding:";
  constexpr char CONTENT_TYPE[] = "content-type:";
  constexpr char LOCATION[] = "location:";
  constexpr char LAST_MODIFIED[] = "last-modified:";
  constexpr char RETRY_AFTER[] = "retry-after:";

  std::string line(data, length);
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.pop_back();
  }

  auto lower = line;
  util::lowercase(lower);
  if (util::startsWith(lower, "http/")) {
    responseLength_ = 0;
    responseRange_ = Range();
    contentDisposition_.clear();
    contentEncoding_.clear();
    contentType_.clear();
    location_.clear();
    retryAfterSeconds_ = 0;
  }
  else if (util::startsWith(lower, CONTENT_LENGTH)) {
    auto value = util::strip(line.substr(sizeof(CONTENT_LENGTH) - 1));
    int64_t parsed = 0;
    if (!util::parseLLIntNoThrow(parsed, value) || parsed < 0) {
      return 0;
    }
    responseLength_ = parsed;
  }
  else if (util::startsWith(lower, CONTENT_DISPOSITION)) {
    contentDisposition_ =
        util::strip(line.substr(sizeof(CONTENT_DISPOSITION) - 1));
  }
  else if (util::startsWith(lower, CONTENT_RANGE)) {
    HttpHeader header;
    header.put(HttpHeader::CONTENT_RANGE,
               util::strip(line.substr(sizeof(CONTENT_RANGE) - 1)));
    try {
      responseRange_ = header.getRange();
    }
    catch (const Exception& ex) {
      rangeProtocolError_ = ex.what();
      rangeProtocolErrorCode_ = error_code::HTTP_PROTOCOL_ERROR;
      return 0;
    }
  }
  else if (util::startsWith(lower, CONTENT_ENCODING)) {
    contentEncoding_ =
        util::strip(line.substr(sizeof(CONTENT_ENCODING) - 1));
  }
  else if (util::startsWith(lower, CONTENT_TYPE)) {
    contentType_ = util::strip(line.substr(sizeof(CONTENT_TYPE) - 1));
  }
  else if (util::startsWith(lower, LOCATION)) {
    location_ = util::strip(line.substr(sizeof(LOCATION) - 1));
  }
  else if (util::startsWith(lower, LAST_MODIFIED)) {
    if (getOption()->getAsBool(PREF_REMOTE_TIME)) {
      auto value = util::strip(line.substr(sizeof(LAST_MODIFIED) - 1));
      getRequestGroup()->updateLastModifiedTime(Time::parseHTTPDate(value));
    }
  }
  else if (util::startsWith(lower, RETRY_AFTER)) {
    auto value = util::strip(line.substr(sizeof(RETRY_AFTER) - 1));
    int64_t seconds = 0;
    if (util::parseLLIntNoThrow(seconds, value) && seconds > 0) {
      retryAfterSeconds_ = static_cast<int>(std::min<int64_t>(seconds, 300));
    }
  }

  return length;
}

} // namespace aria2
