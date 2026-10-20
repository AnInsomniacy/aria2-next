/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
// The aria2 JSON-RPC interface: task control, status and options.
#include "rpc/Params.h"

#include "base/ContentDisposition.h"
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "core/Engine.h"
#include "core/Version.h"
#include "http/Probe.h"
#include "rpc/Dispatcher.h"

#include <random>

namespace aria2::rpc {

std::string text(const json::array& params, size_t index, std::string_view name)
{
  const auto* value = at(params, index);
  if (!value || !value->is_string()) {
    invalid(fmt::format("{} must be a string", name));
  }
  return std::string(value->as_string());
}

std::optional<int64_t> integer(const json::array& params, size_t index)
{
  const auto* value = at(params, index);
  if (!value) {
    return std::nullopt;
  }
  if (value->is_int64()) {
    return value->as_int64();
  }
  if (value->is_uint64()) {
    return static_cast<int64_t>(value->as_uint64());
  }
  if (value->is_string()) {
    if (const auto number = text::toInt(value->as_string())) {
      return number;
    }
    if (value->as_string().starts_with("-")) {
      if (const auto number =
              text::toInt(std::string_view(value->as_string()).substr(1))) {
        return -*number;
      }
    }
  }
  invalid("expected an integer");
}

std::vector<std::string> strings(const json::array& params, size_t index)
{
  std::vector<std::string> result;
  const auto* value = at(params, index);
  if (!value) {
    return result;
  }
  if (!value->is_array()) {
    invalid("expected an array of strings");
  }
  for (const auto& item : value->as_array()) {
    if (!item.is_string()) {
      invalid("expected an array of strings");
    }
    result.emplace_back(item.as_string());
  }
  return result;
}

std::vector<std::string> keys(const json::array& params, size_t index)
{
  return strings(params, index);
}

OptionPairs optionPairs(const json::array& params, size_t index)
{
  OptionPairs pairs;
  const auto* value = at(params, index);
  if (!value) {
    return pairs;
  }
  if (!value->is_object()) {
    invalid("options must be an object");
  }
  for (const auto& [name, item] : value->as_object()) {
    const std::string key(name);
    if (item.is_string()) {
      pairs.emplace_back(key, std::string(item.as_string()));
    }
    else if (item.is_bool()) {
      pairs.emplace_back(key, item.as_bool() ? "true" : "false");
    }
    else if (item.is_int64() || item.is_uint64() || item.is_double()) {
      pairs.emplace_back(key, json::serialize(item));
    }
    else if (item.is_array()) {
      for (const auto& element : item.as_array()) {
        if (element.is_string()) {
          pairs.emplace_back(key, std::string(element.as_string()));
        }
      }
    }
  }
  return pairs;
}

std::shared_ptr<Task> task(Engine& engine, const json::array& params,
                           size_t index)
{
  const auto gid = text(params, index, "GID");
  auto found = engine.find(std::string_view(gid));
  if (!found) {
    invalid(fmt::format("GID {} is not found", gid));
  }
  return found;
}

std::string gidOf(const Task& task) { return gidText(task.gid()); }

json::object status(const Task& task, const std::vector<std::string>& keys)
{
  json::object out;
  task.status(out, StatusKeys(keys));
  return out;
}

namespace {

std::optional<size_t> position(const json::array& params, size_t index)
{
  if (const auto value = integer(params, index); value) {
    if (*value < 0) {
      invalid("position must not be negative");
    }
    return static_cast<size_t>(*value);
  }
  return std::nullopt;
}

// aria2's paging: a negative offset counts from the end, newest first.
template <typename Range>
json::array page(const Range& tasks, int64_t offset, int64_t count,
                 const std::vector<std::string>& wanted)
{
  json::array out;
  const auto size = static_cast<int64_t>(tasks.size());
  if (count <= 0 || size == 0) {
    return out;
  }
  if (offset >= 0) {
    for (auto i = offset; i < size && i < offset + count; ++i) {
      out.push_back(status(*tasks[static_cast<size_t>(i)], wanted));
    }
  }
  else {
    for (auto i = size + offset; i >= 0 && i > size + offset - count; --i) {
      out.push_back(status(*tasks[static_cast<size_t>(i)], wanted));
    }
  }
  return out;
}

json::object optionObject(const std::map<std::string, std::string>& values)
{
  json::object out;
  for (const auto& [name, value] : values) {
    out[name] = value;
  }
  return out;
}

std::map<std::string, std::string> effectiveOptions(const Options& options,
                                                    unsigned wantedScope)
{
  std::map<std::string, std::string> values;
  for (const auto& spec : optionCatalog()) {
    if ((spec.scope & scope::Secret) ||
        (wantedScope && !(spec.scope & wantedScope))) {
      continue;
    }
    auto value = options.get(spec.name);
    if (!value.empty()) {
      values[std::string(spec.name)] = std::move(value);
    }
  }
  return values;
}

} // namespace

void registerMethods(Dispatcher& dispatcher, Engine& engine)
{
  auto& d = dispatcher;
  auto& e = engine;

  d.addAsync("aria2.probe", [&e](json::array& p, Dispatcher::Reply reply) {
    auto uri = text(p, 0, "uri");
    if (uri::isThunder(uri)) {
      uri = uri::unwrapThunder(uri).value_or(std::string());
    }
    const auto parts = uri::parse(uri);
    if (!parts || (parts->scheme != "http" && parts->scheme != "https")) {
      invalid("aria2.probe takes an HTTP or HTTPS URI");
    }
    const auto options = taskOptions(e, optionPairs(p, 1));
    http::probe(e.http(), *options, uri, [reply](http::ProbeResult probe) {
      json::object result{{"status", std::to_string(probe.status)},
                          {"finalUrl", probe.finalUrl},
                          {"contentType", probe.contentType},
                          {"resumable", probe.resumable ? "true" : "false"}};
      if (!probe.filename.empty()) {
        result["filename"] = probe.filename;
      }
      if (probe.size >= 0) {
        result["totalLength"] = std::to_string(probe.size);
      }
      if (!probe.error.empty()) {
        result["error"] = probe.error;
      }
      reply(std::move(result));
    });
  });

  d.add("aria2.addUri", [&e](json::array& p) -> json::value {
    auto options = taskOptions(e, optionPairs(p, 1));
    auto tasks = createTasks(e, strings(p, 0), std::move(options));
    const auto at = position(p, 2);
    for (size_t i = 0; i < tasks.size(); ++i) {
      e.add(tasks[i], at ? std::optional(*at + i) : std::nullopt);
    }
    return json::string(gidOf(*tasks.front()));
  });

  d.add("aria2.addTorrent", [&e](json::array& p) -> json::value {
    const auto torrent = text::base64Decode(text(p, 0, "torrent"));
    if (!torrent) {
      invalid("torrent must be base64");
    }
    auto task = createTorrentTask(e, *torrent, strings(p, 1),
                                  taskOptions(e, optionPairs(p, 2)));
    e.add(task, position(p, 3));
    return json::string(gidOf(*task));
  });

  d.add("aria2.addMetalink", [&e](json::array& p) -> json::value {
    const auto metalink = text::base64Decode(text(p, 0, "metalink"));
    if (!metalink) {
      invalid("metalink must be base64");
    }
    auto tasks =
        createMetalinkTasks(e, *metalink, taskOptions(e, optionPairs(p, 1)));
    const auto at = position(p, 2);
    json::array gids;
    for (size_t i = 0; i < tasks.size(); ++i) {
      e.add(tasks[i], at ? std::optional(*at + i) : std::nullopt);
      gids.push_back(json::string(gidOf(*tasks[i])));
    }
    return gids;
  });

  for (const auto& [name, force] : {std::pair{"aria2.remove", false},
                                    std::pair{"aria2.forceRemove", true}}) {
    d.add(name, [&e, force = force](json::array& p) -> json::value {
      auto target = task(e, p);
      if (target->state() == TaskState::Complete ||
          target->state() == TaskState::Error ||
          target->state() == TaskState::Removed) {
        invalid(fmt::format("Active download not found for GID {}",
                            gidOf(*target)));
      }
      e.remove(target, force);
      return json::string(gidOf(*target));
    });
  }

  for (const auto& [name, force] :
       {std::pair{"aria2.pause", false}, std::pair{"aria2.forcePause", true}}) {
    d.add(name, [&e, force = force](json::array& p) -> json::value {
      auto target = task(e, p);
      if (target->state() != TaskState::Active &&
          target->state() != TaskState::Waiting) {
        invalid(fmt::format("GID {} cannot be paused now", gidOf(*target)));
      }
      e.pause(target, force);
      return json::string(gidOf(*target));
    });
  }
  d.add("aria2.pauseAll", [&e](json::array&) -> json::value {
    e.pauseAll(false);
    return "OK";
  });
  d.add("aria2.forcePauseAll", [&e](json::array&) -> json::value {
    e.pauseAll(true);
    return "OK";
  });
  d.add("aria2.unpause", [&e](json::array& p) -> json::value {
    auto target = task(e, p);
    if (target->state() != TaskState::Paused) {
      invalid(fmt::format("GID {} cannot be unpaused now", gidOf(*target)));
    }
    e.unpause(target);
    return json::string(gidOf(*target));
  });
  d.add("aria2.unpauseAll", [&e](json::array&) -> json::value {
    e.unpauseAll();
    return "OK";
  });

  d.add("aria2.tellStatus", [&e](json::array& p) -> json::value {
    return status(*task(e, p), keys(p, 1));
  });
  d.add("aria2.getUris", [&e](json::array& p) -> json::value {
    json::array out;
    const auto files = task(e, p)->files();
    if (!files.empty()) {
      for (const auto& uri : files.front().uris) {
        out.push_back(json::object{{"uri", uri.uri},
                                   {"status", uri.used ? "used" : "waiting"}});
      }
    }
    return out;
  });
  d.add("aria2.getFiles", [&e](json::array& p) -> json::value {
    json::object out;
    task(e, p)->status(out, StatusKeys({"files"}));
    return out["files"];
  });
  d.add("aria2.getServers", [&e](json::array& p) -> json::value {
    json::array out;
    for (const auto& file : task(e, p)->files()) {
      json::array servers;
      for (const auto& uri : file.uris) {
        if (uri.used) {
          servers.push_back(json::object{
              {"uri", uri.uri},
              {"currentUri", uri.current.empty() ? uri.uri : uri.current},
              {"downloadSpeed", "0"}});
        }
      }
      out.push_back(json::object{{"index", std::to_string(file.index)},
                                 {"servers", std::move(servers)}});
    }
    return out;
  });
  d.add("aria2.tellActive", [&e](json::array& p) -> json::value {
    json::array out;
    for (const auto& task : e.active()) {
      out.push_back(status(*task, keys(p, 0)));
    }
    return out;
  });
  d.add("aria2.tellWaiting", [&e](json::array& p) -> json::value {
    return page(e.waiting(), integer(p, 0).value_or(0),
                integer(p, 1).value_or(0), keys(p, 2));
  });
  d.add("aria2.tellStopped", [&e](json::array& p) -> json::value {
    return page(e.stopped(), integer(p, 0).value_or(0),
                integer(p, 1).value_or(0), keys(p, 2));
  });

  d.add("aria2.changePosition", [&e](json::array& p) -> json::value {
    const auto how = text(p, 2, "how");
    const auto mode = how == "POS_SET"   ? Position::Set
                      : how == "POS_CUR" ? Position::Current
                      : how == "POS_END"
                          ? Position::End
                          : (invalid("how must be POS_SET, POS_CUR or POS_END"),
                             Position::Set);
    return static_cast<int64_t>(
        e.move(task(e, p), integer(p, 1).value_or(0), mode));
  });
  d.add("aria2.changeUri", [&e](json::array& p) -> json::value {
    auto target = task(e, p);
    const auto index = integer(p, 1).value_or(0);
    const auto removed = strings(p, 2);
    const auto added = strings(p, 3);
    const auto at = integer(p, 4);
    if (!target->changeUris(static_cast<int>(index), removed, added,
                            static_cast<int>(at.value_or(-1)))) {
      invalid("The file index is invalid or the task has no URIs");
    }
    return json::array{static_cast<int64_t>(removed.size()),
                       static_cast<int64_t>(added.size())};
  });

  // Moves an unfinished download's files to a new directory, as BitTorrent
  // clients' "Set location"; answers once the files are there.
  d.addAsync("aria2.moveDownload",
             [&e](json::array& p, Dispatcher::Reply reply) {
               auto target = task(e, p);
               const auto dir = text(p, 1, "dir");
               if (dir.empty()) {
                 invalid("dir must not be empty");
               }
               e.relocate(target, dir, [reply](std::string error) {
                 if (error.empty()) {
                   reply("OK");
                 }
                 else {
                   reply.fail(std::move(error));
                 }
               });
             });
  d.add("aria2.getOption", [&e](json::array& p) -> json::value {
    return optionObject(effectiveOptions(task(e, p)->options(), scope::Task));
  });
  d.add("aria2.changeOption", [&e](json::array& p) -> json::value {
    auto target = task(e, p);
    for (const auto& [name, value] : optionPairs(p, 1)) {
      const auto* spec = findOption(name);
      if (!spec) {
        invalid("Unknown option " + name);
      }
      if (!(spec->scope & scope::Task)) {
        continue;
      }
      target->options().set(name, value);
      if (target->state() == TaskState::Active && (spec->scope & scope::Live)) {
        target->optionChanged(name);
      }
    }
    return "OK";
  });
  d.add("aria2.getGlobalOption", [&e](json::array&) -> json::value {
    return optionObject(effectiveOptions(e.global(), 0));
  });
  d.add("aria2.changeGlobalOption", [&e](json::array& p) -> json::value {
    std::vector<std::string> changed;
    for (const auto& [name, value] : optionPairs(p, 0)) {
      const auto* spec = findOption(name);
      if (!spec) {
        e.global().set(name, value); // maps or reports retired names
        continue;
      }
      if (!(spec->scope & (scope::Global | scope::Task)) &&
          name != "save-session") {
        continue;
      }
      e.global().set(name, value);
      changed.push_back(name);
    }
    e.globalOptionsChanged(changed);
    return "OK";
  });

  d.add("aria2.getGlobalStat", [&e](json::array&) -> json::value {
    const auto stat = e.stat();
    return json::object{
        {"downloadSpeed", std::to_string(stat.downloadSpeed)},
        {"uploadSpeed", std::to_string(stat.uploadSpeed)},
        {"numActive", std::to_string(stat.numActive)},
        {"numWaiting", std::to_string(stat.numWaiting)},
        {"numStopped", std::to_string(stat.numStopped)},
        {"numStoppedTotal", std::to_string(stat.numStoppedTotal)}};
  });
  d.add("aria2.purgeDownloadResult", [&e](json::array&) -> json::value {
    e.purgeResults();
    return "OK";
  });
  d.add("aria2.removeDownloadResult", [&e](json::array& p) -> json::value {
    auto target = task(e, p);
    if (!e.removeResult(target->gid())) {
      invalid(fmt::format("Could not remove download result of GID {}",
                          gidOf(*target)));
    }
    return "OK";
  });

  d.add("aria2.getVersion", [](json::array&) -> json::value {
    json::array features;
    for (const auto* feature : enabledFeatures()) {
      features.push_back(json::string(feature));
    }
    return json::object{
        {"version", engineVersion()},
        {"product", "aria2-next"},
        {"rpcVersion", "1.1.0"},
        {"enabledFeatures", std::move(features)},
        {"mediaFeatures", json::array{"request-contexts", "stable-track-ids",
                                      "structured-errors", "captured-inputs"}},
        // Rayburst 4.0 hands browser downloads only to engines listing these.
        {"downloadFeatures", json::array{"filename-hints", "filename-resolution"}}};
  });
  d.add("aria2.getSessionInfo", [](json::array&) -> json::value {
    static const auto session = [] {
      std::random_device random;
      return fmt::format("{:08x}{:08x}{:08x}{:08x}", random(), random(),
                         random(), random());
    }();
    return json::object{{"sessionId", session}};
  });
  d.add("aria2.shutdown", [&e](json::array&) -> json::value {
    e.shutdown(false);
    return "OK";
  });
  d.add("aria2.forceShutdown", [&e](json::array&) -> json::value {
    e.shutdown(true);
    return "OK";
  });
  d.add("aria2.saveSession", [&e](json::array&) -> json::value {
    if (e.global().blank("save-session")) {
      invalid("Filename is not given.");
    }
    if (!e.saveSession()) {
      invalid("Failed to save the session.");
    }
    return "OK";
  });
  d.add("aria2.resolveFilename", [](json::array& p) -> json::value {
    // The name the engine would choose for a URL and Content-Disposition
    // bytes, so browsers and the engine agree before a download starts.
    const auto url = text(p, 0, "url");
    std::string disposition;
    if (const auto* bytes = at(p, 1); bytes && bytes->is_array()) {
      for (const auto& byte : bytes->as_array()) {
        if (!byte.is_int64() || byte.as_int64() < 0 || byte.as_int64() > 255) {
          invalid("Content-Disposition must be an array of bytes");
        }
        disposition.push_back(static_cast<char>(byte.as_int64()));
      }
    }
    auto name = safeFileName(contentDispositionFileName(disposition));
    if (name.empty()) {
      name = uri::fileName(url);
    }
    return json::string(name.empty() ? "index.html" : name);
  });

  registerBtMethods(d, e);
  registerMediaMethods(d, e);
  registerEd2kMethods(d, e);

  d.add("system.listMethods", [&d](json::array&) -> json::value {
    json::array out;
    for (const auto& name : d.methods()) {
      out.push_back(json::string(name));
    }
    out.push_back("system.multicall");
    return out;
  });
  d.add("system.listNotifications", [](json::array&) -> json::value {
    json::array out;
    for (const auto& name : Dispatcher::notifications()) {
      out.push_back(json::string(name));
    }
    return out;
  });
}

} // namespace aria2::rpc
