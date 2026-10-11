/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "media/MediaInput.h"

#include "media/MediaError.h"
#include "media/MediaRequestContext.h"

#include <boost/json/parse.hpp>
#include <openssl/crypto.h>

#include <set>

namespace aria2::media {

namespace json = boost::json;

namespace {

[[noreturn]] void invalid()
{
  throw Failure(FailureCode::UnsupportedSource, "Invalid media input plan");
}

std::string field(const json::object& value, const char* key, size_t maximum)
{
  const auto* text = value.if_contains(key);
  if (!text || !text->is_string() || text->as_string().size() > maximum ||
      std::string_view(text->as_string()).find('\0') !=
          std::string_view::npos) {
    invalid();
  }
  return std::string(text->as_string());
}

const json::object& object(const json::value& value)
{
  if (!value.is_object()) {
    invalid();
  }
  return value.as_object();
}

const json::array& array(const json::object& value, const char* key,
                         size_t maximum)
{
  const auto* found = value.if_contains(key);
  if (!found || !found->is_array() || found->as_array().size() > maximum) {
    invalid();
  }
  return found->as_array();
}

} // namespace

std::array<unsigned char, 16> decodeMediaKey(const std::string& hex)
{
  std::array<unsigned char, 16> value{};
  size_t length = 0;
  if (hex.size() != 32 ||
      !OPENSSL_hexstr2buf_ex(value.data(), value.size(), &length, hex.c_str(),
                             '\0') ||
      length != 16) {
    invalid();
  }
  return value;
}

InputPlan parseInputPlan(const std::string& payload)
{
  InputPlan result;
  if (payload.empty()) {
    return result;
  }
  if (payload.size() > 4 * 1024 * 1024) {
    invalid();
  }
  boost::system::error_code error;
  const auto document = json::parse(payload, error);
  if (error) {
    invalid();
  }
  const auto& root = object(document);
  if (root.size() != 3) {
    invalid();
  }
  for (const auto& entry : array(root, "manifests", 32)) {
    const auto& item = object(entry);
    auto url = field(item, "url", 16384);
    auto content = field(item, "content", 2 * 1024 * 1024);
    if (item.size() != 2 || !isMediaHttpUrl(url) || content.empty() ||
        !result.manifests.emplace(std::move(url), std::move(content)).second) {
      invalid();
    }
  }
  std::set<std::string> ids;
  for (const auto& entry : array(root, "tracks", 32)) {
    const auto& item = object(entry);
    InputTrack track{field(item, "id", 128), field(item, "type", 16), {}};
    const auto& urls = array(item, "urls", 10000);
    const auto* offset = item.if_contains("offsetMs");
    if ((item.size() != 3 && !(item.size() == 4 && offset)) ||
        track.id.empty() || !ids.insert(track.id).second || urls.empty() ||
        (track.type != "video" && track.type != "audio" &&
         track.type != "subtitle" && track.type != "muxed")) {
      invalid();
    }
    if (offset) {
      if (!offset->is_int64() || offset->as_int64() < 0 ||
          offset->as_int64() > 31536000000LL) {
        invalid();
      }
      track.offsetMs = offset->as_int64();
    }
    for (const auto& resource : urls) {
      if (!resource.is_string() || resource.as_string().size() > 16384 ||
          !isMediaHttpUrl(std::string(resource.as_string()))) {
        invalid();
      }
      track.urls.emplace_back(resource.as_string());
    }
    result.tracks.push_back(std::move(track));
  }
  for (const auto& entry : array(root, "keys", 64)) {
    const auto& item = object(entry);
    InputKey key{field(item, "url", 16384), field(item, "key", 32),
                 field(item, "iv", 32)};
    if (item.size() != 3 || (!key.url.empty() && !isMediaHttpUrl(key.url))) {
      invalid();
    }
    decodeMediaKey(key.key);
    if (!key.iv.empty()) {
      decodeMediaKey(key.iv);
    }
    result.keys.push_back(std::move(key));
  }
  return result;
}

} // namespace aria2::media
