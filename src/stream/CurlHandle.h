/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef ARIA2_STREAM_CURL_HANDLE_H
#define ARIA2_STREAM_CURL_HANDLE_H
#include "common.h"
#include "RangePlanner.h"
#include "SpeedCalc.h"
#include "TimerA2.h"
#include "error_code.h"
#include <curl/curl.h>
#include <array>
#include <string>
#include <optional>
#include <vector>

namespace aria2 {
class CurlDownload;
struct CurlDownloadImpl;
enum class CurlResponseFailure {
  None,
  EtagChanged,
  ValidatorUnavailable,
  ModifiedChanged,
  LengthChanged,
  InvalidRange,
  RangeUnsupported,
  EncodedRange,
  PreconditionFailed
};
// Owns one native request and its response/write progress on the engine thread.
// The session detaches value from the multi handle before reset or destruction.
struct CurlHandle {
  CurlHandle() = default;
  ~CurlHandle();
  CurlHandle(const CurlHandle&) = delete;
  CurlHandle& operator=(const CurlHandle&) = delete;
  void reset() noexcept;

  static std::string failureMessage(const CurlHandle& handle, CURLcode result,
                                    long responseCode);
  static void rememberEndpoint(CurlHandle& handle);
  static std::string gid(const CurlDownload* download);
  static void fail(CurlDownload* download, error_code::Value code,
                   const std::string& message) noexcept;
  static void validateResponse(CurlHandle& handle,
                               const std::string& contentRange);
  static size_t writeData(char*, size_t, size_t, void*) noexcept;
  static size_t receiveHeader(char*, size_t, size_t, void*) noexcept;
  static int updateProgress(void*, curl_off_t, curl_off_t, curl_off_t,
                            curl_off_t) noexcept;
  static int debugCallback(CURL*, curl_infotype, char*, size_t, void*) noexcept;

  CurlDownload* download = nullptr;
  CURL* value = nullptr;
  curl_slist* headers = nullptr;
  RangeLease lease;
  int64_t writeOffset = 0;
  int64_t appliedLimit = -1;
  int64_t bufferOffset = 0;
  size_t bufferLimit = 0;
  SpeedCalc payloadSpeed;
  Timer bodySampleStart = Timer::zero();
  Timer lastPayload = Timer::zero();
  uint64_t connectionEpoch = 0;
  uint64_t endpointGeneration = 0;
  bool resolvingEndpoint = false;
  bool redirectedEndpoint = false;
  long addressFamily = CURL_IPRESOLVE_WHATEVER;
  int64_t responseRangeEnd = -1;
  int64_t responseTotalLength = -1;
  int64_t responseContentLength = -1;
  int64_t unsatisfiedTotalLength = -1;
  long responseCode = 0;
  bool ranged = false;
  bool rangeAccepted = false;
  bool fullResponseAccepted = false;
  bool encoded = false;
  bool headersComplete = false;
  bool primary = false;
  bool pausedForDisk = false;
  std::optional<CURLcode> completed;
  bool manualRedirect = false;
  unsigned redirects = 0;
  CurlResponseFailure responseFailure = CurlResponseFailure::None;
  std::string responseEtag;
  std::string responseLastModified;
  std::string responseDate;
  std::string rangeValidator;
  std::string range;
  std::vector<unsigned char> writeBuffer;
  std::array<char, CURL_ERROR_SIZE> errorBuffer{};
};

namespace stream {
bool containsSensitiveCurlText(const std::string& value);
void rememberIdentity(CurlDownloadImpl& impl, const std::string& etag,
                      const std::string& modified, const std::string& date);
const char* responseFailureName(CurlResponseFailure failure);
const char* responseFailureMessage(CurlResponseFailure failure);

// A successful writer call precedes committing its half-open range. Buffered
// progress contributes to live statistics, but is excluded from checkpoints.
void flushWriteBuffer(CurlDownloadImpl& impl, CurlHandle& handle);
int64_t bufferedLength(const CurlDownloadImpl& impl);
} // namespace stream
} // namespace aria2
#endif
