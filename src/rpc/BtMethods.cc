/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// BitTorrent methods: aria2.getPeers plus the aria2-next extensions for
// trackers, web seeds, peers, rechecks, announces, the peer blocklist,
// metainfo inspection and the session.
#include "rpc/Params.h"

#include "base/Text.h"
#include "bt/Blocklist.h"
#include "bt/BtTask.h"
#include "bt/Metainfo.h"
#include "bt/Session.h"
#include "core/Engine.h"
#include "rpc/Dispatcher.h"

namespace aria2::rpc {

namespace {

std::shared_ptr<bt::BtTask> btTask(Engine& engine, const json::array& params)
{
  auto found = std::dynamic_pointer_cast<bt::BtTask>(task(engine, params));
  if (!found) {
    invalid(fmt::format("GID {} is not a BitTorrent download", text(params, 0, "GID")));
  }
  return found;
}

const json::array& list(const json::array& params, size_t index, std::string_view name)
{
  const auto* value = at(params, index);
  if (!value || !value->is_array()) {
    invalid(fmt::format("{} must be an array", name));
  }
  return value->as_array();
}

} // namespace

void registerBtMethods(Dispatcher& d, Engine& e)
{
  d.add("aria2.getPeers", [&e](json::array& p) -> json::value {
    const auto found = std::dynamic_pointer_cast<bt::BtTask>(task(e, p));
    if (!found || found->state() != TaskState::Active) {
      return json::array();
    }
    try {
      return found->peers();
    }
    catch (const Error&) {
      return json::array(); // between start and attach
    }
  });
  d.add("aria2.getBtTrackers", [&e](json::array& p) -> json::value {
    return btTask(e, p)->trackers();
  });
  d.add("aria2.forceBtRecheck", [&e](json::array& p) -> json::value {
    auto found = btTask(e, p);
    found->recheck();
    return json::string(gidOf(*found));
  });
  d.add("aria2.forceBtAnnounce", [&e](json::array& p) -> json::value {
    auto found = btTask(e, p);
    found->announce();
    return json::string(gidOf(*found));
  });
  d.add("aria2.replaceBtTrackers", [&e](json::array& p) -> json::value {
    auto found = btTask(e, p);
    std::vector<std::pair<std::string, int>> trackers;
    for (const auto& item : list(p, 1, "trackers")) {
      const auto* entry = item.if_object();
      const auto* url = entry ? entry->if_contains("url") : nullptr;
      const auto* tier = entry ? entry->if_contains("tier") : nullptr;
      if (!url || !url->is_string() || !tier || !tier->is_int64()) {
        invalid("Each tracker needs a string url and an integer tier");
      }
      trackers.emplace_back(std::string(url->as_string()), static_cast<int>(tier->as_int64()));
    }
    found->replaceTrackers(trackers);
    return json::string(gidOf(*found));
  });
  d.add("aria2.replaceBtWebSeeds", [&e](json::array& p) -> json::value {
    auto found = btTask(e, p);
    found->replaceWebSeeds(strings(p, 1));
    return json::string(gidOf(*found));
  });
  d.add("aria2.addBtPeers", [&e](json::array& p) -> json::value {
    const auto [added, failed] = btTask(e, p)->addPeers(strings(p, 1));
    return json::object{{"added", added}, {"failed", failed}};
  });
  // Each rule is an address, a CIDR block or a range; one bad rule rejects
  // the call and keeps the current blocklist.
  d.add("aria2.setBtPeerBlocklist", [&e](json::array& p) -> json::value {
    std::string content;
    for (const auto& rule : strings(p, 0)) {
      if (rule.find('\n') != std::string::npos || bt::parseBlocklist(rule).rules != 1) {
        invalid(fmt::format("Invalid blocklist rule: {}", rule));
      }
      content += rule + '\n';
    }
    auto blocklist = bt::parseBlocklist(content);
    const auto rules = blocklist.rules;
    const auto revision = e.bt().replaceBlocklist(std::move(blocklist));
    return json::object{{"ruleCount", rules}, {"revision", revision}};
  });
  d.add("aria2.getBtSessionStatus", [&e](json::array&) -> json::value {
    return e.bt().status();
  });
  d.add("aria2.inspectTorrent", [](json::array& p) -> json::value {
    const auto data = text::base64Decode(text(p, 0, "torrent"));
    if (!data) {
      invalid("torrent must be base64");
    }
    return bt::describe(bt::parseTorrent(*data));
  });
}

} // namespace aria2::rpc
