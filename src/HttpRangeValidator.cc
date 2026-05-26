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
#include "HttpRangeValidator.h"

#include "fmt.h"
#include "util.h"

namespace aria2 {

bool isHttpContentEncodingIdentity(const std::string& contentEncoding)
{
  if (contentEncoding.empty()) {
    return true;
  }
  auto normalized = util::strip(contentEncoding);
  util::lowercase(normalized);
  return normalized.empty() || normalized == "identity";
}

HttpRangeValidationResult validateHttpRangeResponse(
    long status, const Range& expectedRange, const Range& responseRange,
    int64_t knownEntityLength, const std::string& contentEncoding)
{
  if (status != 206) {
    return {false, status != 200, status == 200,
            fmt("HTTP range request expected status 206, got %ld.", status)};
  }
  if (!isHttpContentEncodingIdentity(contentEncoding)) {
    return {false, true, false,
            fmt("HTTP range response used unsupported Content-Encoding: %s.",
                contentEncoding.c_str())};
  }
  if (responseRange.entityLength <= 0 ||
      responseRange.endByte < responseRange.startByte) {
    return {false, true, false,
            "HTTP range response did not include a valid Content-Range."};
  }
  if (responseRange.startByte != expectedRange.startByte ||
      responseRange.endByte != expectedRange.endByte) {
    return {false, true, false,
            fmt("HTTP range response mismatch. Request: %lld-%lld/%lld, "
                "Response: %lld-%lld/%lld.",
                static_cast<long long>(expectedRange.startByte),
                static_cast<long long>(expectedRange.endByte),
                static_cast<long long>(expectedRange.entityLength),
                static_cast<long long>(responseRange.startByte),
                static_cast<long long>(responseRange.endByte),
                static_cast<long long>(responseRange.entityLength))};
  }
  if (knownEntityLength > 0 && responseRange.entityLength != knownEntityLength) {
    return {false, true, false,
            fmt("HTTP range entity length mismatch. Expected %lld, got %lld.",
                static_cast<long long>(knownEntityLength),
                static_cast<long long>(responseRange.entityLength))};
  }
  return {true, false, false, ""};
}

} // namespace aria2
