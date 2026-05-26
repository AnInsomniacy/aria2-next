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
#ifndef D_HTTP_RANGE_VALIDATOR_H
#define D_HTTP_RANGE_VALIDATOR_H

#include "Range.h"

#include <string>

namespace aria2 {

struct HttpRangeValidationResult {
  bool ok;
  bool retryable;
  std::string error;
};

HttpRangeValidationResult validateHttpRangeResponse(
    long status, const Range& expectedRange, const Range& responseRange,
    int64_t knownEntityLength, const std::string& contentEncoding);

bool isHttpContentEncodingIdentity(const std::string& contentEncoding);

} // namespace aria2

#endif // D_HTTP_RANGE_VALIDATOR_H
