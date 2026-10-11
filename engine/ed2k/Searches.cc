/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Searches.h"

#include "base/Text.h"
#include "core/Engine.h"
#include "ed2k/Kad.h"
#include "ed2k/Link.h"
#include "ed2k/Network.h"
#include "ed2k/ServerSession.h"

#include <boost/json/array.hpp>

#include <algorithm>

namespace aria2::ed2k {

namespace json = boost::json;
using namespace std::chrono_literals;

namespace {

constexpr auto keepEnded = 1h;
constexpr size_t maxResults = 2000;

bool matches(const SearchQuery& query, const SearchResultEntry& entry)
{
  if (query.minSize > 0 && entry.size < query.minSize) {
    return false;
  }
  if (query.maxSize > 0 && entry.size > query.maxSize) {
    return false;
  }
  if (entry.sourceCount < query.minSourceCount ||
      entry.completeSourceCount < query.minCompleteSourceCount) {
    return false;
  }
  if (!query.extension.empty() &&
      !text::iequals(entry.extension, query.extension)) {
    return false;
  }
  return true;
}

} // namespace

Searches::Searches(Network& network) : network_(network) {}

Gid Searches::start(const SearchQuery& query, std::chrono::seconds timeout)
{
  Gid id;
  do {
    id = newGid();
  } while (searches_.contains(id) || network_.engine().find(id));
  auto& search = searches_[id];
  search.query = query;
  search.deadline = Clock::now() + timeout;
  send(id, search);
  return id;
}

void Searches::send(Gid id, Search& search)
{
  // The newest search takes the server over; it answers one at a time.
  if (!search.server && network_.server().search(search.query)) {
    search.server = true;
    serverSearch_ = id;
  }
  if (!search.kad) {
    search.kad = network_.kad().search(id, search.query.keyword);
  }
}

bool Searches::more(Gid id)
{
  const auto found = searches_.find(id);
  if (found == searches_.end() || serverSearch_ != id ||
      !found->second.moreResults) {
    return false;
  }
  found->second.moreResults = false;
  found->second.active = true;
  found->second.deadline = Clock::now() + 30s;
  return network_.server().moreResults();
}

bool Searches::remove(Gid id)
{
  if (serverSearch_ == id) {
    serverSearch_.reset();
  }
  network_.kad().stopSearch(id);
  return searches_.erase(id) > 0;
}

void Searches::merge(Search& search,
                     const std::vector<SearchResultEntry>& entries)
{
  for (const auto& entry : entries) {
    if (entry.hash.size() != 16 || !matches(search.query, entry)) {
      continue;
    }
    auto existing = std::find_if(search.results.begin(), search.results.end(),
                                 [&entry](const SearchResultEntry& other) {
                                   return other.hash == entry.hash;
                                 });
    if (existing == search.results.end()) {
      if (search.results.size() < maxResults) {
        search.results.push_back(entry);
      }
      continue;
    }
    existing->sourceCount = std::max(existing->sourceCount, entry.sourceCount);
    existing->completeSourceCount =
        std::max(existing->completeSourceCount, entry.completeSourceCount);
    if (existing->sourceNetwork != entry.sourceNetwork) {
      existing->sourceNetwork = "both";
    }
  }
}

void Searches::serverResults(const SearchResult& result)
{
  if (!serverSearch_) {
    return;
  }
  const auto found = searches_.find(*serverSearch_);
  if (found == searches_.end()) {
    return;
  }
  merge(found->second, result.entries);
  found->second.moreResults = result.moreResults;
}

void Searches::kadResults(Gid id, const std::vector<SearchResultEntry>& entries)
{
  if (const auto found = searches_.find(id); found != searches_.end()) {
    merge(found->second, entries);
  }
}

void Searches::tick(Clock::time_point now)
{
  for (auto it = searches_.begin(); it != searches_.end();) {
    auto& search = it->second;
    if (search.active && now >= search.deadline) {
      search.active = false;
      search.ended = now;
      network_.kad().stopSearch(it->first);
    }
    else if (search.active && (!search.server || !search.kad)) {
      send(it->first, search);
    }
    if (!search.active && now - search.ended > keepEnded) {
      if (serverSearch_ == it->first) {
        serverSearch_.reset();
      }
      it = searches_.erase(it);
      continue;
    }
    ++it;
  }
}

bool Searches::active() const
{
  return std::any_of(searches_.begin(), searches_.end(),
                     [](const auto& entry) { return entry.second.active; });
}

size_t Searches::resultCount() const
{
  size_t count = 0;
  for (const auto& [id, search] : searches_) {
    count += search.results.size();
  }
  return count;
}

bool Searches::moreResults() const
{
  return std::any_of(searches_.begin(), searches_.end(), [](const auto& entry) {
    return entry.second.moreResults;
  });
}

std::optional<json::object> Searches::results(Gid id) const
{
  const auto found = searches_.find(id);
  if (found == searches_.end()) {
    return std::nullopt;
  }
  const auto& search = found->second;
  json::object out;
  out["gid"] = gidText(id);
  out["status"] = search.active ? "active" : "complete";
  out["moreResults"] = search.moreResults;
  json::array results;
  for (const auto& entry : search.results) {
    json::object item;
    item["hash"] = text::toHex(entry.hash);
    item["name"] = entry.name;
    item["length"] = std::to_string(entry.size);
    item["sourceCount"] = std::to_string(entry.sourceCount);
    item["completeSourceCount"] = std::to_string(entry.completeSourceCount);
    item["fileType"] = entry.fileType;
    item["extension"] = entry.extension;
    item["mediaArtist"] = entry.mediaArtist;
    item["mediaAlbum"] = entry.mediaAlbum;
    item["mediaTitle"] = entry.mediaTitle;
    item["mediaLength"] = entry.mediaLength;
    item["mediaBitrate"] = std::to_string(entry.mediaBitrate);
    item["mediaCodec"] = entry.mediaCodec;
    item["sourceNetwork"] = entry.sourceNetwork;
    Link link;
    link.name = entry.name;
    link.size = entry.size;
    link.hash = entry.hash;
    item["ed2kLink"] =
        entry.ed2kLink.empty() ? toFileLink(link) : entry.ed2kLink;
    results.push_back(std::move(item));
  }
  out["results"] = std::move(results);
  return out;
}

} // namespace aria2::ed2k
