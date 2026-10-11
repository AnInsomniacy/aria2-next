/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Uri.h"

#include "base/Path.h"
#include "base/Text.h"

#include <curl/curl.h>

#include <memory>

namespace aria2::uri {
namespace {

using Url = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;

Url load(std::string_view text)
{
  Url url(curl_url(), curl_url_cleanup);
  if (url && curl_url_set(url.get(), CURLUPART_URL, std::string(text).c_str(),
                          CURLU_NON_SUPPORT_SCHEME | CURLU_URLENCODE) !=
                 CURLUE_OK) {
    url.reset();
  }
  return url;
}

std::string get(CURLU* url, CURLUPart part, unsigned flags = 0)
{
  char* value = nullptr;
  if (curl_url_get(url, part, &value, flags) != CURLUE_OK) {
    return {};
  }
  std::string result(value);
  curl_free(value);
  return result;
}

} // namespace

std::optional<Parts> parse(std::string_view text)
{
  const auto url = load(text);
  if (!url) {
    return std::nullopt;
  }
  return Parts{text::lower(get(url.get(), CURLUPART_SCHEME)),
               get(url.get(), CURLUPART_USER),
               get(url.get(), CURLUPART_HOST),
               get(url.get(), CURLUPART_PORT, CURLU_DEFAULT_PORT),
               get(url.get(), CURLUPART_PATH),
               get(url.get(), CURLUPART_QUERY)};
}

std::optional<std::string> resolve(std::string_view base,
                                   std::string_view reference)
{
  const auto url = load(base);
  if (!url || curl_url_set(url.get(), CURLUPART_URL,
                           std::string(reference).c_str(),
                           CURLU_NON_SUPPORT_SCHEME | CURLU_URLENCODE) !=
                  CURLUE_OK) {
    return std::nullopt;
  }
  return get(url.get(), CURLUPART_URL);
}

bool sameOrigin(std::string_view a, std::string_view b)
{
  const auto left = parse(a);
  const auto right = parse(b);
  return left && right && left->scheme == right->scheme &&
         text::iequals(left->host, right->host) && left->port == right->port;
}

std::string redact(std::string_view text)
{
  const auto url = load(text);
  if (!url) {
    return std::string(text);
  }
  curl_url_set(url.get(), CURLUPART_USER, nullptr, 0);
  curl_url_set(url.get(), CURLUPART_PASSWORD, nullptr, 0);
  curl_url_set(url.get(), CURLUPART_QUERY, nullptr, 0);
  return get(url.get(), CURLUPART_URL);
}

std::string fileName(std::string_view text)
{
  const auto parts = parse(text);
  if (!parts) {
    return {};
  }
  for (const auto& entry : text::split(parts->query, '&')) {
    const auto equals = entry.find('=');
    if (equals != std::string::npos &&
        text::percentDecode(entry.substr(0, equals)) == "filename") {
      auto name = safeFileName(text::percentDecode(
          text::replaceAll(entry.substr(equals + 1), "+", " ")));
      if (!name.empty()) {
        return name;
      }
    }
  }
  const auto slash = parts->path.rfind('/');
  const auto segment = parts->path.substr(slash + 1);
  auto decoded = text::percentDecode(segment);
  return safeFileName(text::isUtf8(decoded) ? decoded : segment);
}

} // namespace aria2::uri
