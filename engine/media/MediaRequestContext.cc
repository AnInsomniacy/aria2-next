/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "media/MediaRequestContext.h"

#include "base/Text.h"
#include "base/Uri.h"
#include "media/MediaError.h"

#include <boost/json/parse.hpp>
#include <curl/curl.h>

#include <algorithm>
#include <memory>
#include <string_view>

namespace aria2::media {

namespace json = boost::json;

namespace {

[[noreturn]] void invalid()
{
  throw Failure(FailureCode::UnsupportedSource,
                "Invalid media-request-contexts");
}

std::string stringField(const json::value& value, const char* name,
                        size_t maximum)
{
  const auto* object = value.if_object();
  const auto* field = object ? object->if_contains(name) : nullptr;
  if (!field || !field->is_string() || field->as_string().size() > maximum) {
    invalid();
  }
  return std::string(field->as_string());
}

bool validHeader(const std::string& name, const std::string& value)
{
  constexpr std::string_view token =
      "!#$%&'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyz";
  if (name.empty() || name.find_first_not_of(token) != std::string::npos ||
      std::any_of(value.begin(), value.end(), [](unsigned char c) {
        return (c < 32 && c != '\t') || c == 127;
      })) {
    return false;
  }
  for (const auto* owned :
       {"host", "connection", "content-length", "transfer-encoding", "range",
        "accept-encoding", "keep-alive", "te", "trailer", "upgrade"}) {
    if (name == owned) {
      return false;
    }
  }
  return !name.starts_with("proxy-") && !name.starts_with("if-");
}

} // namespace

bool isMediaHttpUrl(const std::string& value)
{
  std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> url(curl_url(),
                                                          curl_url_cleanup);
  if (!url || value.find('\0') != std::string::npos ||
      value.find_first_of("\r\n") != std::string::npos ||
      curl_url_set(url.get(), CURLUPART_URL, value.c_str(),
                   CURLU_DISALLOW_USER) != CURLUE_OK) {
    return false;
  }
  char* scheme = nullptr;
  if (curl_url_get(url.get(), CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK) {
    return false;
  }
  const bool valid =
      std::string_view(scheme) == "http" || std::string_view(scheme) == "https";
  curl_free(scheme);
  return valid;
}

std::vector<RequestContext> parseRequestContexts(const std::string& payload)
{
  if (payload.empty()) {
    return {};
  }
  if (payload.size() > 256 * 1024) {
    invalid();
  }
  boost::system::error_code error;
  const auto document = json::parse(payload, error);
  if (error || !document.is_array() || document.as_array().size() > 8) {
    invalid();
  }
  std::vector<RequestContext> contexts;
  for (const auto& entry : document.as_array()) {
    RequestContext context;
    context.url = stringField(entry, "url", 16384);
    const auto* headers = entry.as_object().if_contains("headers");
    if (!isMediaHttpUrl(context.url) || !headers || !headers->is_array() ||
        entry.as_object().size() != 2 || headers->as_array().size() > 32 ||
        std::any_of(contexts.begin(), contexts.end(),
                    [&](const auto& previous) {
                      return uri::sameOrigin(previous.url, context.url);
                    })) {
      invalid();
    }
    size_t bytes = 0;
    for (const auto& field : headers->as_array()) {
      auto name = text::lower(stringField(field, "name", 128));
      auto value = stringField(field, "value", 8192);
      bytes += name.size() + value.size();
      if (field.as_object().size() != 2 || bytes > 16384 ||
          !validHeader(name, value) ||
          !context.headers.emplace(std::move(name), std::move(value)).second) {
        invalid();
      }
    }
    contexts.push_back(std::move(context));
  }
  return contexts;
}

} // namespace aria2::media
