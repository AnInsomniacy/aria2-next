/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "OutputName.h"

#include "Option.h"
#include "Request.h"
#include "base64.h"
#include "prefs.h"
#include "uri.h"
#include "support/ContentDisposition.h"
#include "support/Encoding.h"
#include "support/FilePath.h"
#include "support/Text.h"
#include <curl/curl.h>
#include <filesystem>
#include <limits>
#include <memory>
#include <regex>
#include <utility>

namespace aria2::output {
namespace {
std::string percentDecode(const std::string& value)
{
  if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return value;
  }
  int length = 0;
  const std::unique_ptr<char, decltype(&curl_free)> decoded(
      curl_easy_unescape(nullptr, value.c_str(), static_cast<int>(value.size()),
                         &length),
      curl_free);
  return decoded ? std::string(decoded.get(), static_cast<size_t>(length))
                 : value;
}

// Some HTTP endpoints emit a MIME encoded-word instead of filename*. Limit
// decoding to that explicit envelope; ordinary percent signs remain literal.
std::string headerText(const std::string& name)
{
  static const std::regex word(R"(^=\?([^?]+)\?([bBqQ])\?([^?]*)\?=$)");
  std::smatch parts;
  if (!std::regex_match(name, parts, word)) {
    return name;
  }
  auto charset = parts[1].str();
  util::lowercase(charset);
  if (charset != "utf-8" && charset != "iso-8859-1") {
    return name;
  }
  auto body = parts[3].str();
  std::string decoded;
  if (parts[2] == "B" || parts[2] == "b") {
    if (body.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz0123456789+/=") != std::string::npos) {
      return name;
    }
    decoded = base64::decode(body.begin(), body.end());
  }
  else {
    body = util::replace(util::replace(body, "%", "%25"), "=", "%");
    decoded = percentDecode(util::replace(body, "_", " "));
  }
  if (charset == "iso-8859-1") {
    decoded = util::iso8859p1ToUtf8(decoded.data(), decoded.size());
  }
  return !decoded.empty() && util::isUtf8(decoded) ? decoded : name;
}
} // namespace
std::string safeName(const std::string& name)
{
  if (name.empty() || name == "." || name == "..") {
    return {};
  }
  auto result = util::createSafePath(name);
#ifdef __MINGW32__
  while (!result.empty() && (result.back() == '.' || result.back() == ' ')) {
    result.pop_back();
  }
  auto stem = result.substr(0, result.find('.'));
  util::lowercase(stem);
  if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
      (stem.size() == 4 &&
       (stem.compare(0, 3, "com") == 0 || stem.compare(0, 3, "lpt") == 0) &&
       stem[3] >= '1' && stem[3] <= '9')) {
    result.insert(0, "_");
  }
#endif
  // A byte bound also fits filesystems whose component limit is in code units.
  if (result.size() > 240) {
    auto extension = std::filesystem::u8path(result).extension().u8string();
    if (extension.size() >= 240) {
      extension.clear();
    }
    result.resize(240 - extension.size());
    while (!result.empty() && !util::isUtf8(result)) {
      result.pop_back();
    }
    result += extension;
  }
  return result;
}

namespace {
std::string queryName(const std::string& uri)
{
  uri::UriStruct parsed;
  if (!uri::parse(parsed, uri)) {
    return {};
  }
  auto query = parsed.query;
  if (!query.empty() && query.front() == '?')
    query.erase(0, 1);
  for (size_t begin = 0; begin < query.size();) {
    const auto end = query.find('&', begin);
    const auto entry =
        query.substr(begin, end == std::string::npos ? end : end - begin);
    const auto equals = entry.find('=');
    if (equals != std::string::npos &&
        percentDecode(entry.substr(0, equals)) == "filename") {
      const auto name =
          percentDecode(util::replace(entry.substr(equals + 1), "+", " "));
      if (util::isUtf8(name) && !safeName(name).empty())
        return safeName(name);
    }
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return {};
}
} // namespace

std::string urlName(const std::string& uri)
{
  auto name = queryName(uri);
  if (!name.empty())
    return name;
  uri::UriStruct parsed;
  if (!uri::parse(parsed, uri))
    return {};
  name = parsed.file;
  auto candidate = percentDecode(name);
  if (util::isUtf8(candidate) && candidate != "." && candidate != "..") {
    name = std::move(candidate);
  }
  return safeName(name);
}

std::string suggestedName(const Option& option, const std::string& uri,
                          const std::string& disposition,
                          const std::string& originalUri)
{
  const auto hint = safeName(option.get(PREF_FILENAME_HINT));
  if (!hint.empty() && option.get(PREF_FILENAME_HINT_SOURCE) == "browser") {
    return hint;
  }
  auto name = util::getContentDispositionFilename(disposition, true);
  if (name.empty()) {
    name = util::getContentDispositionFilename(disposition, false);
  }
  name = safeName(headerText(name));
  if (!name.empty()) {
    return name;
  }
  if (!hint.empty()) {
    return hint;
  }
  name = queryName(originalUri);
  if (name.empty())
    name = urlName(uri);
  return name.empty() ? Request::DEFAULT_FILE : name;
}

std::string mediaName(const Option& option, const std::string& uri)
{
  auto name = option.get(PREF_OUT);
  if (name.empty()) {
    name = safeName(option.get(PREF_FILENAME_HINT));
    if (!name.empty() && option.get(PREF_FILENAME_HINT_SOURCE) == "title") {
      return name + "." + option.get(PREF_MEDIA_FORMAT);
    }
    if (name.empty()) {
      name = urlName(uri);
    }
    if (name.empty()) {
      name = "media";
    }
  }
  auto path = std::filesystem::u8path(name);
  path.replace_extension("." + option.get(PREF_MEDIA_FORMAT));
  return path.u8string();
}
} // namespace aria2::output
