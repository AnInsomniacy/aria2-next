/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Gid.h"
#include "core/Speed.h"
#include "ed2k/Search.h"

#include <boost/json/object.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aria2::ed2k {

class Network;

// Keyword searches on the connected server and in Kad. Results of both are
// merged by file hash; a search ends after its timeout and is discarded when
// its client removes it or an hour after it ended.
class Searches {
public:
  explicit Searches(Network& network);

  Gid start(const SearchQuery& query, std::chrono::seconds timeout);
  // Asks the server for more results of the last server search.
  bool more(Gid id);
  bool remove(Gid id);
  std::optional<boost::json::object> results(Gid id) const;
  bool active() const;
  size_t resultCount() const;
  bool moreResults() const;

  // Results as they arrive.
  void serverResults(const SearchResult& result);
  void kadResults(Gid id, const std::vector<SearchResultEntry>& entries);
  void tick(Clock::time_point now);

private:
  struct Search {
    SearchQuery query;
    std::vector<SearchResultEntry> results;
    Clock::time_point deadline;
    Clock::time_point ended{};
    bool moreResults = false;
    bool active = true;
    // Sent once the server is logged in or Kad has contacts.
    bool server = false;
    bool kad = false;
  };

  void send(Gid id, Search& search);

  void merge(Search& search, const std::vector<SearchResultEntry>& entries);

  Network& network_;
  std::map<Gid, Search> searches_;
  std::optional<Gid> serverSearch_; // the server answers one search at a time
};

} // namespace aria2::ed2k
