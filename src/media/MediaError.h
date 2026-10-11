/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <stdexcept>
#include <string>

namespace aria2::media {

enum class FailureCode {
  None,
  UnsupportedSource,
  AuthenticationRequired,
  ProtectedMedia,
  UnsupportedSelection,
  ProbeFailed,
};

inline const char* failureCodeName(FailureCode code)
{
  switch (code) {
  case FailureCode::None:
    return "";
  case FailureCode::UnsupportedSource:
    return "unsupported_source";
  case FailureCode::AuthenticationRequired:
    return "authentication_required";
  case FailureCode::ProtectedMedia:
    return "protected_media";
  case FailureCode::UnsupportedSelection:
    return "unsupported_selection";
  case FailureCode::ProbeFailed:
    return "probe_failed";
  }
  return "probe_failed";
}

struct Failure : std::runtime_error {
  Failure(FailureCode code, const std::string& message)
      : std::runtime_error(message), code(code)
  {
  }
  FailureCode code;
};

inline FailureCode failureCode(const std::exception& error)
{
  const auto* failure = dynamic_cast<const Failure*>(&error);
  return failure ? failure->code : FailureCode::ProbeFailed;
}

} // namespace aria2::media
