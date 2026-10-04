/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// Keep the Windows socket ABI consistent before native library headers.
#include "common.h" // IWYU pragma: keep

#include "CurlOptions.h"

#include <curl/urlapi.h>
#include <curl/curl.h>
#include <memory>
#include <string>
#include <regex>
#include "uri.h"

#include "Option.h"
#include "prefs.h"
#include "support/Text.h"

namespace aria2::http {
std::string noProxyFor(const std::string& rules, const std::string& url)
{
  uri::UriStruct parsed;
  if (!uri::parse(parsed, url))
    return {};
  std::string result;
  for (size_t begin = 0; begin < rules.size();) {
    const auto end = rules.find_first_of(",;\r\n", begin);
    auto rule =
        rules.substr(begin, end == std::string::npos ? end : end - begin);
    const auto first = rule.find_first_not_of(" \t");
    if (first != std::string::npos)
      rule = rule.substr(first, rule.find_last_not_of(" \t") - first + 1);
    else
      rule.clear();
    if (rule != "*" && rule.find('*') != std::string::npos) {
      std::string expression;
      for (const char c : rule) {
        if (c == '*')
          expression += ".*";
        else {
          if (std::string(".^$|()[]{}+?\\").find(c) != std::string::npos)
            expression += '\\';
          expression += c;
        }
      }
      rule = rule.size() <= 253 &&
                     std::regex_match(parsed.host,
                                      std::regex(expression, std::regex::icase))
                 ? parsed.host
                 : "";
    }
    if (!rule.empty()) {
      if (!result.empty())
        result += ',';
      result += rule;
    }
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  return result;
}

namespace {
long platformSslOptions() noexcept
{
#ifdef _WIN32
  return CURLSSLOPT_REVOKE_BEST_EFFORT;
#else
  return 0L;
#endif
}
} // namespace

bool sameOrigin(const std::string& first, const std::string& second)
{
  const auto left = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(
      curl_url(), curl_url_cleanup);
  const auto right = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(
      curl_url(), curl_url_cleanup);
  if (!left || !right ||
      curl_url_set(left.get(), CURLUPART_URL, first.c_str(), 0) != CURLUE_OK ||
      curl_url_set(right.get(), CURLUPART_URL, second.c_str(), 0) !=
          CURLUE_OK) {
    return false;
  }
  for (const auto part : {CURLUPART_SCHEME, CURLUPART_HOST, CURLUPART_PORT}) {
    char* a = nullptr;
    char* b = nullptr;
    const auto aResult = curl_url_get(left.get(), part, &a, CURLU_DEFAULT_PORT);
    const auto bResult =
        curl_url_get(right.get(), part, &b, CURLU_DEFAULT_PORT);
    const auto aValue =
        std::unique_ptr<char, decltype(&curl_free)>(a, curl_free);
    const auto bValue =
        std::unique_ptr<char, decltype(&curl_free)>(b, curl_free);
    if (aResult != CURLUE_OK || bResult != CURLUE_OK || !util::strieq(a, b)) {
      return false;
    }
  }
  return true;
}

CURLcode configureTls(CURL* handle, const Option* option)
{
  CURLcode result = CURLE_OK;
  auto set = [&](CURLoption key, auto value) {
    if (result == CURLE_OK)
      result = curl_easy_setopt(handle, key, value);
  };
  const long verify = option->getAsBool(PREF_CHECK_CERTIFICATE) ? 1L : 0L;
  set(CURLOPT_SSL_VERIFYPEER, verify);
  set(CURLOPT_SSL_VERIFYHOST, verify ? 2L : 0L);
  set(CURLOPT_PROXY_SSL_VERIFYPEER, verify);
  set(CURLOPT_PROXY_SSL_VERIFYHOST, verify ? 2L : 0L);
  set(CURLOPT_SSL_OPTIONS, platformSslOptions());
  set(CURLOPT_PROXY_SSL_OPTIONS, platformSslOptions());
  const auto& minimum = option->get(PREF_MIN_TLS_VERSION);
  const long version = minimum == A2_V_TLS13   ? CURL_SSLVERSION_TLSv1_3
                       : minimum == A2_V_TLS12 ? CURL_SSLVERSION_TLSv1_2
                                               : CURL_SSLVERSION_TLSv1_1;
  set(CURLOPT_SSLVERSION, version);
  if (!option->blank(PREF_CA_CERTIFICATE))
    set(CURLOPT_CAINFO, option->get(PREF_CA_CERTIFICATE).c_str());
  if (!option->blank(PREF_CERTIFICATE))
    set(CURLOPT_SSLCERT, option->get(PREF_CERTIFICATE).c_str());
  if (!option->blank(PREF_PRIVATE_KEY))
    set(CURLOPT_SSLKEY, option->get(PREF_PRIVATE_KEY).c_str());
  return result;
}

} // namespace aria2::http
