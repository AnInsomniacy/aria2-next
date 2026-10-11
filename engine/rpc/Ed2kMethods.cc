/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// ED2K methods: keyword searches on the server and in Kad, and the state of
// the eDonkey network presence.
#include "rpc/Params.h"

#include "base/Text.h"
#include "core/Engine.h"
#include "ed2k/Network.h"
#include "ed2k/Search.h"
#include "ed2k/Searches.h"
#include "rpc/Dispatcher.h"

namespace aria2::rpc {

namespace {

constexpr int64_t defaultSearchTimeout = 60;

std::string field(const json::object* options, std::string_view name)
{
  const auto* value = options ? options->if_contains(name) : nullptr;
  if (!value) {
    return {};
  }
  if (value->is_string()) {
    return std::string(value->as_string());
  }
  if (value->is_int64()) {
    return std::to_string(value->as_int64());
  }
  if (value->is_uint64()) {
    return std::to_string(value->as_uint64());
  }
  return {};
}

int64_t number(const json::object* options, std::string_view name,
               int64_t fallback)
{
  const auto value = field(options, name);
  if (value.empty()) {
    return fallback;
  }
  const auto parsed = text::toSize(value);
  if (!parsed || *parsed < 0) {
    invalid(fmt::format("{} must be a non-negative number", name));
  }
  return *parsed;
}

Gid searchGid(const json::array& params)
{
  const auto gid = parseGid(text(params, 0, "GID"));
  if (!gid) {
    invalid("Invalid GID");
  }
  return *gid;
}

} // namespace

void registerEd2kMethods(Dispatcher& d, Engine& e)
{
  d.add("aria2.ed2kSearch", [&e](json::array& p) -> json::value {
    ed2k::SearchQuery query;
    query.keyword = std::string(text::trim(text(p, 0, "keyword")));
    if (query.keyword.empty()) {
      invalid("The ED2K search keyword is empty");
    }
    const auto* value = at(p, 1);
    const auto* options = value ? value->if_object() : nullptr;
    query.fileType = field(options, "fileType");
    query.extension = field(options, "extension");
    query.minSize = number(options, "minSize", 0);
    query.maxSize = number(options, "maxSize", 0);
    query.minSourceCount =
        static_cast<uint32_t>(number(options, "minSourceCount", 0));
    query.minCompleteSourceCount =
        static_cast<uint32_t>(number(options, "minCompleteSourceCount", 0));
    const auto timeout = std::clamp<int64_t>(
        number(options, "timeout", defaultSearchTimeout), 5, 600);
    const auto gid =
        e.ed2k().searches().start(query, std::chrono::seconds(timeout));
    return json::string(gidText(gid));
  });
  d.add("aria2.getEd2kSearchResults", [&e](json::array& p) -> json::value {
    const auto gid = searchGid(p);
    auto results =
        e.ed2kStarted() ? e.ed2k().searches().results(gid) : std::nullopt;
    if (!results) {
      invalid(fmt::format("No ED2K search with GID {}", gidText(gid)));
    }
    return std::move(*results);
  });
  d.add("aria2.ed2kSearchMore", [&e](json::array& p) -> json::value {
    const auto gid = searchGid(p);
    return json::value(e.ed2kStarted() && e.ed2k().searches().more(gid));
  });
  d.add("aria2.removeEd2kSearch", [&e](json::array& p) -> json::value {
    const auto gid = searchGid(p);
    if (e.ed2kStarted()) {
      e.ed2k().searches().remove(gid);
    }
    return json::string("OK");
  });
  d.add("aria2.getEd2kStatus", [&e](json::array&) -> json::value {
    json::object out;
    e.ed2k().describe(out);
    return out;
  });
}

} // namespace aria2::rpc
