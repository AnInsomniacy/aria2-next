/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// Request settings shared by every HTTP, HTTPS and SFTP transfer.
#include "base/Error.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "http/Client.h"
#include "http/Cookies.h"
#include "http/Request.h"
#include "options/Options.h"

#include <regex>

namespace aria2::http {
namespace {

// Expands host globs such as *.example.com for this request; libcurl keeps
// native domain, address and CIDR matching for the remaining rules.
std::string noProxyFor(const std::string& rules, const std::string& host)
{
  std::vector<std::string> result;
  for (auto rule : text::split(text::replaceAll(rules, ";", ","), ',')) {
    if (rule != "*" && rule.find('*') != std::string::npos) {
      std::string pattern;
      for (const char c : rule) {
        if (c == '*') {
          pattern += ".*";
          continue;
        }
        if (std::string_view(".^$|()[]{}+?\\").find(c) !=
            std::string_view::npos) {
          pattern += '\\';
        }
        pattern += c;
      }
      rule = std::regex_match(host, std::regex(pattern, std::regex::icase))
                 ? host
                 : "";
    }
    if (!rule.empty()) {
      result.push_back(rule);
    }
  }
  return text::join(result, ",");
}

const char* proxyKind(const std::string& scheme)
{
  return scheme == "https" ? "https" : scheme == "http" ? "http" : "all";
}

} // namespace

void configureRequest(CURL* easy, CURLSH* share, std::string& loadedCookies,
                      const Options& options, const std::string& url,
                      const std::string& originalUrl,
                      const std::function<void(const std::string&)>& header)
{
  const auto set = [easy](CURLoption option, auto value) {
    if (curl_easy_setopt(easy, option, value) != CURLE_OK) {
      fail(ErrorCode::Unknown, "Unsupported transfer setting {}",
           static_cast<int>(option));
    }
  };
  const auto parts = uri::parse(url);
  const auto scheme = parts ? parts->scheme : std::string();
  const bool sameOrigin = uri::sameOrigin(url, originalUrl);

  set(CURLOPT_URL, url.c_str());
  set(CURLOPT_SHARE, share);
  set(CURLOPT_PROTOCOLS_STR, "http,https,sftp");
  set(CURLOPT_REDIR_PROTOCOLS_STR, "http,https,sftp");
  set(CURLOPT_FOLLOWLOCATION, 1L);
  set(CURLOPT_MAXREDIRS, 20L);
  set(CURLOPT_FAILONERROR, 1L);
  set(CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
  // libcurl's 16 KiB default costs a callback per few packets at high speed.
  set(CURLOPT_BUFFERSIZE, 512L * 1024);
  set(CURLOPT_TCP_KEEPALIVE, 1L);
  set(CURLOPT_HTTPAUTH, static_cast<long>(CURLAUTH_ANY));
  set(CURLOPT_CONNECTTIMEOUT,
      static_cast<long>(options.integer("connect-timeout")));
  // A hard idle limit; HttpTask detects stalls much earlier while other
  // connections make progress.
  set(CURLOPT_LOW_SPEED_LIMIT, static_cast<long>(std::max<int64_t>(
                                   1, options.integer("lowest-speed-limit"))));
  set(CURLOPT_LOW_SPEED_TIME, static_cast<long>(options.integer("timeout")));
  set(CURLOPT_FORBID_REUSE, options.flag("enable-http-keep-alive") ? 0L : 1L);
  set(CURLOPT_FILETIME, 1L);

  const auto agent = options.get("user-agent");
  set(CURLOPT_USERAGENT, agent.c_str());
  auto referer = options.get("referer");
  if (referer == "*") {
    referer = originalUrl;
  }
  if (!referer.empty()) {
    set(CURLOPT_REFERER, referer.c_str());
  }
  for (const auto& line : text::split(options.get("header"), '\n')) {
    // Credentials in custom headers stay with the origin they were given for.
    if (!sameOrigin && (text::istartsWith(line, "authorization:") ||
                        text::istartsWith(line, "cookie:"))) {
      continue;
    }
    header(line);
  }
  if (options.flag("http-no-cache")) {
    header("Cache-Control: no-cache");
    header("Pragma: no-cache");
  }

  // TLS
  const long verify = options.flag("check-certificate") ? 1L : 0L;
  set(CURLOPT_SSL_VERIFYPEER, verify);
  set(CURLOPT_SSL_VERIFYHOST, verify ? 2L : 0L);
  set(CURLOPT_PROXY_SSL_VERIFYPEER, verify);
  set(CURLOPT_PROXY_SSL_VERIFYHOST, verify ? 2L : 0L);
#ifdef _WIN32
  set(CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_REVOKE_BEST_EFFORT));
  set(CURLOPT_PROXY_SSL_OPTIONS,
      static_cast<long>(CURLSSLOPT_REVOKE_BEST_EFFORT));
#endif
  const auto tls = options.get("min-tls-version");
  set(CURLOPT_SSLVERSION,
      static_cast<long>(tls == "TLSv1.3"   ? CURL_SSLVERSION_TLSv1_3
                        : tls == "TLSv1.2" ? CURL_SSLVERSION_TLSv1_2
                                           : CURL_SSLVERSION_TLSv1_1));
  for (const auto& [name, option] :
       {std::pair{"ca-certificate", CURLOPT_CAINFO},
        std::pair{"certificate", CURLOPT_SSLCERT},
        std::pair{"private-key", CURLOPT_SSLKEY}}) {
    const auto value = options.get(name);
    if (!value.empty()) {
      set(option, value.c_str());
    }
  }

  // Network
  if (const auto interface = options.get("interface"); !interface.empty()) {
    set(CURLOPT_INTERFACE, interface.c_str());
  }
  if (options.flag("disable-ipv6")) {
    set(CURLOPT_IPRESOLVE, static_cast<long>(CURL_IPRESOLVE_V4));
  }
  if (const auto doh = options.get("dns-over-https"); !doh.empty()) {
    set(CURLOPT_DOH_URL, doh.c_str());
  }

  // Credentials
  if (scheme == "sftp") {
    if (!options.blank("sftp-user")) {
      set(CURLOPT_USERNAME, options.get("sftp-user").c_str());
      set(CURLOPT_PASSWORD, options.get("sftp-passwd").c_str());
    }
    if (!options.blank("private-key")) {
      set(CURLOPT_SSH_PRIVATE_KEYFILE, options.get("private-key").c_str());
    }
    if (!options.blank("ssh-host-key-sha256")) {
      set(CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256,
          options.get("ssh-host-key-sha256").c_str());
    }
  }
  else if (sameOrigin && !options.blank("http-user")) {
    set(CURLOPT_USERNAME, options.get("http-user").c_str());
    set(CURLOPT_PASSWORD, options.get("http-passwd").c_str());
  }
  if (!options.flag("no-netrc")) {
    set(CURLOPT_NETRC, static_cast<long>(CURL_NETRC_OPTIONAL));
    if (!options.blank("netrc-path")) {
      set(CURLOPT_NETRC_FILE, options.get("netrc-path").c_str());
    }
  }

  // Proxy
  const std::string kind = proxyKind(scheme);
  auto proxy = kind == "all" ? std::string() : options.get(kind + "-proxy");
  auto prefix = kind + "-proxy";
  if (proxy.empty()) {
    proxy = options.get("all-proxy");
    prefix = "all-proxy";
  }
  if (!proxy.empty()) {
    set(CURLOPT_PROXY, proxy.c_str());
    if (!options.blank(prefix + "-user")) {
      set(CURLOPT_PROXYUSERNAME, options.get(prefix + "-user").c_str());
      set(CURLOPT_PROXYPASSWORD, options.get(prefix + "-passwd").c_str());
    }
    if (!options.blank("no-proxy") && parts) {
      const auto bypass = noProxyFor(options.get("no-proxy"), parts->host);
      set(CURLOPT_NOPROXY, bypass.c_str());
    }
  }

  // Cookies: one jar shared by all transfers; browser stores load once per
  // revision.
  set(CURLOPT_COOKIEFILE, "");
  if (const auto cookies = options.get("load-cookies"); !cookies.empty()) {
    if (isSqliteCookieFile(cookies)) {
      const auto revision = cookies + '|' + fileRevision(cookies);
      if (loadedCookies != revision) {
        for (const auto& line : sqliteCookieLines(cookies)) {
          set(CURLOPT_COOKIELIST, line.c_str());
        }
        loadedCookies = revision;
      }
    }
    else {
      set(CURLOPT_COOKIEFILE, cookies.c_str());
    }
  }
  if (const auto jar = options.get("save-cookies"); !jar.empty()) {
    set(CURLOPT_COOKIEJAR, jar.c_str());
  }
}

void Client::configure(Transfer& transfer, const Options& options,
                       const std::string& url, const std::string& originalUrl)
{
  configureRequest(
      transfer.easy(), share_, loadedCookies_, options, url, originalUrl,
      [&transfer](const std::string& line) { transfer.header(line); });
}

std::optional<ContentRange> parseContentRange(std::string_view value)
{
  if (!text::istartsWith(value, "bytes")) {
    return std::nullopt;
  }
  value = text::trim(value.substr(5));
  const auto dash = value.find('-');
  const auto slash = value.find('/');
  if (dash == std::string_view::npos || slash == std::string_view::npos ||
      dash > slash) {
    return std::nullopt;
  }
  const auto first = text::toInt(value.substr(0, dash));
  const auto last = text::toInt(value.substr(dash + 1, slash - dash - 1));
  if (!first || !last || *first < 0 || *last < *first) {
    return std::nullopt;
  }
  const auto total = text::toInt(value.substr(slash + 1));
  return ContentRange{*first, *last, total.value_or(-1)};
}

} // namespace aria2::http
