/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#include "LegacyInputAdapter.h"
#include "aria2/aria2.h"
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <map>
#include <mutex>
#include <set>
#include <string_view>

#include "DlAbortEx.h"
#include "Log.h"
#include "Option.h"
#include "error_code.h"
#include "fmt.h"
#include "prefs.h"
#include "support/Numbers.h"

namespace aria2 {

namespace {

enum class ArgumentKind { Required, OptionalBoolean };
enum class ValueAction { Keep, Map, Skip };

struct LegacyOptionSpec {
  const char* name;
  ArgumentKind argument;
  bool sensitive = false;
};

constexpr std::array<LegacyOptionSpec, 73> LEGACY_OPTIONS{{
    {"allow-piece-length-change", ArgumentKind::OptionalBoolean},
    {"always-resume", ArgumentKind::OptionalBoolean},
    {"async-dns", ArgumentKind::OptionalBoolean},
    {"async-dns-server", ArgumentKind::Required},
    {"auto-save-interval", ArgumentKind::Required},
    {"bt-detach-seed-only", ArgumentKind::OptionalBoolean},
    {"bt-enable-hook-after-hash-check", ArgumentKind::OptionalBoolean},
    {"bt-force-encryption", ArgumentKind::OptionalBoolean},
    {"bt-hash-check-seed", ArgumentKind::OptionalBoolean},
    {"bt-keep-alive-interval", ArgumentKind::Required},
    {"bt-load-saved-metadata", ArgumentKind::OptionalBoolean},
    {"bt-lpd-interface", ArgumentKind::Required},
    {"bt-metadata-only", ArgumentKind::OptionalBoolean},
    {"bt-min-crypto-level", ArgumentKind::Required},
    {"bt-prioritize-piece", ArgumentKind::Required},
    {"bt-remove-unselected-file", ArgumentKind::OptionalBoolean},
    {"bt-request-peer-speed-limit", ArgumentKind::Required},
    {"bt-request-timeout", ArgumentKind::Required},
    {"bt-require-crypto", ArgumentKind::OptionalBoolean},
    {"bt-save-metadata", ArgumentKind::OptionalBoolean},
    {"bt-stop-timeout", ArgumentKind::Required},
    {"bt-timeout", ArgumentKind::Required},
    {"bt-tracker-connect-timeout", ArgumentKind::Required},
    {"bt-tracker-interval", ArgumentKind::Required},
    {"bt-tracker-timeout", ArgumentKind::Required},
    {"conditional-get", ArgumentKind::OptionalBoolean},
    {"content-disposition-default-utf8", ArgumentKind::OptionalBoolean},
    {"dht-entry-point", ArgumentKind::Required},
    {"dht-entry-point-host", ArgumentKind::Required},
    {"dht-entry-point-host6", ArgumentKind::Required},
    {"dht-entry-point-port", ArgumentKind::Required},
    {"dht-entry-point-port6", ArgumentKind::Required},
    {"dht-entry-point6", ArgumentKind::Required},
    {"dht-file-path", ArgumentKind::Required},
    {"dht-file-path6", ArgumentKind::Required},
    {"dht-listen-addr", ArgumentKind::Required},
    {"dht-listen-addr6", ArgumentKind::Required},
    {"dht-listen-port", ArgumentKind::Required},
    {"dht-message-timeout", ArgumentKind::Required},
    {"enable-async-dns6", ArgumentKind::OptionalBoolean},
    {"enable-dht6", ArgumentKind::OptionalBoolean},
    {"enable-http-pipelining", ArgumentKind::OptionalBoolean},
    {"ftp-passwd", ArgumentKind::Required, true},
    {"ftp-pasv", ArgumentKind::OptionalBoolean},
    {"ftp-proxy", ArgumentKind::Required},
    {"ftp-proxy-passwd", ArgumentKind::Required, true},
    {"ftp-proxy-user", ArgumentKind::Required},
    {"ftp-reuse-connection", ArgumentKind::OptionalBoolean},
    {"ftp-type", ArgumentKind::Required},
    {"ftp-user", ArgumentKind::Required},
    {"http-auth-challenge", ArgumentKind::OptionalBoolean},
    {"max-connection-per-server", ArgumentKind::Required},
    {"max-http-pipelining", ArgumentKind::Required},
    {"min-split-size", ArgumentKind::Required},
    {"no-want-digest-header", ArgumentKind::OptionalBoolean},
    {"peer-agent", ArgumentKind::Required},
    {"peer-connection-timeout", ArgumentKind::Required},
    {"peer-id-prefix", ArgumentKind::Required},
    {"proxy-method", ArgumentKind::Required},
    {"remove-control-file", ArgumentKind::OptionalBoolean},
    {"reuse-uri", ArgumentKind::OptionalBoolean},
    {"rpc-passwd", ArgumentKind::Required, true},
    {"rpc-user", ArgumentKind::Required, true},
    {"select-least-used-host", ArgumentKind::OptionalBoolean},
    {"server-stat-if", ArgumentKind::Required},
    {"server-stat-of", ArgumentKind::Required},
    {"server-stat-timeout", ArgumentKind::Required},
    {"split", ArgumentKind::Required},
    {"ssh-host-key-md", ArgumentKind::Required},
    {"startup-idle-time", ArgumentKind::Required},
    {"stream-piece-selector", ArgumentKind::Required},
    {"uri-selector", ArgumentKind::Required},
    {"use-head", ArgumentKind::OptionalBoolean},
}};

const LegacyOptionSpec* findLegacyOption(std::string_view name)
{
  const auto result = std::find_if(
      LEGACY_OPTIONS.begin(), LEGACY_OPTIONS.end(),
      [name](const LegacyOptionSpec& spec) { return name == spec.name; });
  return result == LEGACY_OPTIONS.end() ? nullptr : &*result;
}

const char* sourceName(LegacyInputSource source)
{
  switch (source) {
  case LegacyInputSource::CommandLine:
    return "command line";
  case LegacyInputSource::Configuration:
    return "configuration";
  case LegacyInputSource::Session:
    return "task input";
  case LegacyInputSource::Rpc:
    return "RPC";
  case LegacyInputSource::Library:
    return "libaria2";
  }
  return "input";
}

void logAdaptation(LegacyInputSource source, const std::string& name,
                   const std::string& result)
{
  static std::mutex mutex;
  static std::set<std::string> emitted;
  const auto key = std::to_string(static_cast<int>(source)) + ':' + name;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!emitted.insert(key).second) {
      return;
    }
  }
  A2_LOG_WARN(fmt("Legacy aria2 input from %s: %s; %s", sourceName(source),
                  name.c_str(), result.c_str()));
}

std::string normalizeLegacyPort(const std::string& name,
                                const std::string& value)
{
  try {
    auto ports = util::parseIntSegments(value);
    ports.normalize();
    if (!ports.hasNext()) {
      throw DL_ABORT_EX("Empty port range");
    }
    int first = -1;
    while (ports.hasNext()) {
      const auto port = ports.next();
      if (port < 1024 || port > UINT16_MAX) {
        throw DL_ABORT_EX("Port is out of range");
      }
      if (first < 0) {
        first = port;
      }
    }
    return std::to_string(first);
  }
  catch (Exception&) {
    throw DL_ABORT_EX2(
        fmt("Invalid legacy option value: %s=%s", name.c_str(), value.c_str()),
        error_code::OPTION_ERROR);
  }
}

ValueAction adaptLegacyValue(const std::string& name, const std::string& value,
                             std::string& normalized)
{
  if ((name == "log-level" || name == "console-log-level") &&
      value == "notice") {
    normalized = V_INFO;
    return ValueAction::Map;
  }
  if (name == "bt-encryption" && value == "enabled") {
    normalized = V_PREFERRED;
    return ValueAction::Map;
  }
  if (name == "listen-port" && value.find_first_of("-,") != std::string::npos) {
    normalized = normalizeLegacyPort(name, value);
    return ValueAction::Map;
  }
  if (name == "metalink-preferred-protocol" && value == "ftp") {
    return ValueAction::Skip;
  }
  return ValueAction::Keep;
}

bool parseBoolean(const std::string& name, const std::string& value)
{
  if (value == A2_V_TRUE) {
    return true;
  }
  if (value == A2_V_FALSE) {
    return false;
  }
  throw DL_ABORT_EX2(
      fmt("Invalid legacy option value: %s=%s", name.c_str(), value.c_str()),
      error_code::OPTION_ERROR);
}

std::string joinValues(const std::vector<std::string>& values)
{
  std::string result;
  std::set<std::string> seen;
  for (const auto& value : values) {
    if (value.empty() || !seen.insert(value).second) {
      continue;
    }
    if (!result.empty()) {
      result += ',';
    }
    result += value;
  }
  return result;
}

std::string endpoint(const std::map<std::string, std::string>& legacy,
                     const char* hostName, const char* portName, bool ipv6)
{
  const auto host = legacy.find(hostName);
  const auto port = legacy.find(portName);
  if (host == legacy.end() || port == legacy.end() || host->second.empty() ||
      port->second.empty()) {
    return {};
  }
  return ipv6 ? '[' + host->second + "]:" + port->second
              : host->second + ':' + port->second;
}

std::string currentValue(const Option* option, const char* name)
{
  if (!option) {
    return {};
  }
  const auto pref = option::k2p(name);
  return pref && pref->i != 0 ? option->get(pref) : std::string();
}

} // namespace

bool isLegacyInputOption(const std::string& name)
{
  return findLegacyOption(name) != nullptr;
}

KeyVals normalizeLegacyInput(const KeyVals& options, LegacyInputSource source)
{
  KeyVals result;
  std::map<std::string, std::string> legacy;
  std::set<std::string> canonical;
  for (const auto& option : options) {
    if (isLegacyInputOption(option.first)) {
      legacy[option.first] = option.second;
      continue;
    }
    std::string normalized;
    const auto action =
        adaptLegacyValue(option.first, option.second, normalized);
    if (action == ValueAction::Map) {
      result.emplace_back(option.first, normalized);
      logAdaptation(source, option.first + '=' + option.second,
                    "mapped to " + option.first + '=' + normalized);
    }
    else if (action == ValueAction::Skip) {
      logAdaptation(source, option.first + '=' + option.second,
                    "accepted and skipped; FTP is not a maintained protocol");
      continue;
    }
    else {
      result.push_back(option);
    }
    canonical.insert(option.first);
  }
  if (legacy.empty()) {
    return result;
  }

  std::set<std::string> mapped;
  auto add = [&](const std::string& oldName, const std::string& target,
                 const std::string& value, bool approximate = false) {
    if (canonical.count(target) != 0 || mapped.count(target) != 0) {
      logAdaptation(source, oldName,
                    "skipped because " + target + " is already set");
      return;
    }
    result.emplace_back(target, value);
    mapped.insert(target);
    logAdaptation(
        source, oldName,
        std::string(approximate ? "approximately mapped to " : "mapped to ") +
            target + '=' + value);
  };
  auto ignore = [&](const std::string& name, const std::string& reason) {
    logAdaptation(source, name, "accepted and skipped; " + reason);
  };
  auto addGroup = [&](std::initializer_list<const char*> names,
                      const std::string& target, const std::string& value,
                      bool approximate = false) {
    const bool conflict =
        canonical.count(target) != 0 || mapped.count(target) != 0;
    if (!conflict) {
      result.emplace_back(target, value);
      mapped.insert(target);
    }
    for (const auto* name : names) {
      if (legacy.count(name) == 0) {
        continue;
      }
      logAdaptation(source, name,
                    conflict
                        ? "skipped because " + target + " is already set"
                        : std::string(approximate ? "approximately mapped to "
                                                  : "mapped to ") +
                              target + '=' + value);
    }
  };

  for (const auto* name : {"async-dns", "enable-async-dns6"}) {
    const auto item = legacy.find(name);
    if (item != legacy.end()) {
      parseBoolean(item->first, item->second);
      ignore(item->first, "the system resolver is always asynchronous");
    }
  }
  if (legacy.count("async-dns-server") != 0) {
    ignore("async-dns-server",
           "DNS servers are managed by the operating system");
  }

  if (const auto item = legacy.find("auto-save-interval");
      item != legacy.end()) {
    add(item->first, "state-save-interval", item->second);
  }
  if (const auto item = legacy.find("bt-detach-seed-only");
      item != legacy.end()) {
    add(item->first, "detach-share-only",
        parseBoolean(item->first, item->second) ? A2_V_TRUE : A2_V_FALSE);
  }

  if (canonical.count("bt-encryption") == 0) {
    bool present = false;
    bool required = false;
    for (const auto* name : {"bt-force-encryption", "bt-require-crypto"}) {
      const auto item = legacy.find(name);
      if (item != legacy.end()) {
        present = true;
        required = required || parseBoolean(item->first, item->second);
      }
    }
    const auto level = legacy.find("bt-min-crypto-level");
    if (level != legacy.end()) {
      present = true;
      if (level->second == "arc4") {
        required = true;
      }
      else if (level->second != "plain") {
        throw DL_ABORT_EX2(fmt("Invalid legacy option value: %s=%s",
                               level->first.c_str(), level->second.c_str()),
                           error_code::OPTION_ERROR);
      }
    }
    if (present) {
      addGroup(
          {"bt-force-encryption", "bt-require-crypto", "bt-min-crypto-level"},
          "bt-encryption", required ? V_REQUIRED : V_PREFERRED, true);
    }
  }
  else {
    for (const auto* name :
         {"bt-force-encryption", "bt-require-crypto", "bt-min-crypto-level"}) {
      if (legacy.count(name) != 0) {
        logAdaptation(source, name,
                      "skipped because bt-encryption is already set");
      }
    }
  }

  if (const auto item = legacy.find("bt-lpd-interface"); item != legacy.end()) {
    add(item->first, "bt-interface", item->second, true);
  }
  std::vector<std::string> interfaces;
  for (const auto* name : {"dht-listen-addr", "dht-listen-addr6"}) {
    const auto item = legacy.find(name);
    if (item != legacy.end()) {
      interfaces.push_back(item->second);
    }
  }
  if (!interfaces.empty()) {
    addGroup({"dht-listen-addr", "dht-listen-addr6"}, "bt-interface",
             joinValues(interfaces), true);
  }
  if (const auto item = legacy.find("bt-metadata-only"); item != legacy.end()) {
    add(item->first, "pause-metadata",
        parseBoolean(item->first, item->second) ? A2_V_TRUE : A2_V_FALSE, true);
  }
  if (const auto item = legacy.find("bt-prioritize-piece");
      item != legacy.end()) {
    add(item->first, "bt-first-last-piece-first", A2_V_TRUE, true);
  }
  if (const auto item = legacy.find("bt-tracker-connect-timeout");
      item != legacy.end()) {
    add(item->first, "bt-tracker-completion-timeout", item->second, true);
  }
  if (const auto item = legacy.find("bt-tracker-timeout");
      item != legacy.end()) {
    add(item->first, "bt-tracker-receive-timeout", item->second, true);
  }
  if (const auto item = legacy.find("dht-listen-port"); item != legacy.end()) {
    add(item->first, "listen-port",
        normalizeLegacyPort(item->first, item->second), true);
  }
  if (const auto item = legacy.find("enable-dht6"); item != legacy.end()) {
    if (parseBoolean(item->first, item->second)) {
      add(item->first, "enable-dht", A2_V_TRUE, true);
    }
    else {
      ignore(item->first, "libtorrent uses one DHT switch for both families");
    }
  }

  std::vector<std::string> bootstrap;
  for (const auto* name : {"dht-entry-point", "dht-entry-point6"}) {
    const auto item = legacy.find(name);
    if (item != legacy.end()) {
      bootstrap.push_back(item->second);
    }
  }
  bootstrap.push_back(
      endpoint(legacy, "dht-entry-point-host", "dht-entry-point-port", false));
  bootstrap.push_back(
      endpoint(legacy, "dht-entry-point-host6", "dht-entry-point-port6", true));
  const auto bootstrapValue = joinValues(bootstrap);
  if (!bootstrapValue.empty()) {
    addGroup({"dht-entry-point", "dht-entry-point6", "dht-entry-point-host",
              "dht-entry-point-port", "dht-entry-point-host6",
              "dht-entry-point-port6"},
             "bt-dht-bootstrap-nodes", bootstrapValue);
  }
  else {
    for (const auto* name :
         {"dht-entry-point", "dht-entry-point6", "dht-entry-point-host",
          "dht-entry-point-port", "dht-entry-point-host6",
          "dht-entry-point-port6"}) {
      if (legacy.count(name) != 0) {
        ignore(name, "the DHT endpoint is incomplete");
      }
    }
  }

  const auto split = legacy.find("split");
  const auto perServer = legacy.find("max-connection-per-server");
  if (split != legacy.end() || perServer != legacy.end()) {
    std::string value =
        split != legacy.end() ? split->second : perServer->second;
    int64_t connectionValue = 0;
    if (split != legacy.end() && perServer != legacy.end()) {
      int64_t splitValue = 0;
      int64_t perServerValue = 0;
      if (util::parseLLIntNoThrow(splitValue, split->second) &&
          util::parseLLIntNoThrow(perServerValue, perServer->second)) {
        connectionValue = std::min(splitValue, perServerValue);
      }
    }
    else {
      util::parseLLIntNoThrow(connectionValue, value);
    }
    if (connectionValue > 0) {
      value = std::to_string(std::min<int64_t>(connectionValue, 256));
    }
    addGroup({"split", "max-connection-per-server"}, "stream-max-connections",
             value, true);
  }

  if (const auto item = legacy.find("always-resume"); item != legacy.end()) {
    parseBoolean(item->first, item->second);
    ignore(item->first, "native recovery and bounded full-response restarts own this policy");
  }

  static constexpr std::array<const char*, 35> RETIRED{{
      "allow-piece-length-change",
      "bt-enable-hook-after-hash-check",
      "bt-hash-check-seed",
      "bt-keep-alive-interval",
      "bt-load-saved-metadata",
      "bt-remove-unselected-file",
      "bt-request-peer-speed-limit",
      "bt-request-timeout",
      "bt-save-metadata",
      "bt-stop-timeout",
      "bt-timeout",
      "bt-tracker-interval",
      "conditional-get",
      "content-disposition-default-utf8",
      "dht-file-path",
      "dht-file-path6",
      "dht-message-timeout",
      "enable-http-pipelining",
      "http-auth-challenge",
      "max-http-pipelining",
      "min-split-size",
      "no-want-digest-header",
      "peer-agent",
      "peer-connection-timeout",
      "peer-id-prefix",
      "proxy-method",
      "remove-control-file",
      "reuse-uri",
      "select-least-used-host",
      "server-stat-if",
      "server-stat-of",
      "server-stat-timeout",
      "startup-idle-time",
      "stream-piece-selector",
      "uri-selector",
  }};
  for (const auto* name : RETIRED) {
    if (legacy.count(name) != 0) {
      ignore(name, "the maintained native engine owns or retired this policy");
    }
  }
  for (const auto* name :
       {"ftp-passwd", "ftp-pasv", "ftp-proxy", "ftp-proxy-passwd",
        "ftp-proxy-user", "ftp-reuse-connection", "ftp-type", "ftp-user"}) {
    if (legacy.count(name) != 0) {
      ignore(name, "FTP is not a maintained protocol");
    }
  }
  for (const auto* name : {"rpc-user", "rpc-passwd"}) {
    if (legacy.count(name) != 0) {
      ignore(name, "RPC Basic authentication was replaced by rpc-secret");
    }
  }
  if (legacy.count("ssh-host-key-md") != 0) {
    ignore("ssh-host-key-md", "legacy hashes cannot be converted to SHA-256");
  }
  if (legacy.count("use-head") != 0) {
    ignore("use-head", "libcurl performs native request discovery");
  }

  for (auto& item : legacy) {
    const auto* spec = findLegacyOption(item.first);
    if (spec && spec->sensitive) {
      std::fill(item.second.begin(), item.second.end(), '*');
    }
  }

  return result;
}

std::vector<std::string> normalizeLegacyCommandLine(int argc, char* argv[],
                                                    LegacyInputSource source)
{
  KeyVals legacy;
  std::vector<std::string> passthrough;
  passthrough.reserve(static_cast<size_t>(argc));
  passthrough.emplace_back(argc > 0 ? argv[0] : "aria2-next");
  bool adapted = false;
  bool endOfOptions = false;

  for (int index = 1; index < argc; ++index) {
    std::string argument(argv[index]);
    if (endOfOptions || argument == "--") {
      endOfOptions = true;
      passthrough.push_back(std::move(argument));
      continue;
    }

    std::string name;
    std::string value;
    const LegacyOptionSpec* spec = nullptr;
    char* sensitiveValue = nullptr;
    if (argument.rfind("--", 0) == 0 && argument.size() > 2) {
      const auto equals = argument.find('=');
      name = argument.substr(2, equals == std::string::npos ? std::string::npos
                                                            : equals - 2);
      spec = findLegacyOption(name);
      if (!spec) {
        if (name != "log-level" && name != "console-log-level" &&
            name != "bt-encryption" && name != "listen-port" &&
            name != "metalink-preferred-protocol") {
          passthrough.push_back(std::move(argument));
          continue;
        }
        if (equals != std::string::npos) {
          value = argument.substr(equals + 1);
        }
        else if (index + 1 < argc) {
          value = argv[++index];
        }
        std::string normalized;
        if (adaptLegacyValue(name, value, normalized) == ValueAction::Keep) {
          passthrough.push_back(std::move(argument));
          if (equals == std::string::npos) {
            passthrough.push_back(value);
          }
          continue;
        }
        legacy.emplace_back(name, std::move(value));
        adapted = true;
        continue;
      }
      if (equals != std::string::npos) {
        value = argument.substr(equals + 1);
        sensitiveValue = argv[index] + equals + 1;
      }
      else if (spec->argument == ArgumentKind::OptionalBoolean) {
        value = A2_V_TRUE;
      }
      else if (index + 1 < argc) {
        value = argv[++index];
        sensitiveValue = argv[index];
      }
      else {
        throw DL_ABORT_EX2("Missing argument for legacy option --" + name,
                           error_code::OPTION_ERROR);
      }
    }
    else if (argument.size() >= 2 && argument[0] == '-' &&
             (argument[1] == 's' || argument[1] == 'x' || argument[1] == 'k' ||
              argument[1] == 'p')) {
      const char shortName = argument[1];
      name = shortName == 's'   ? "split"
             : shortName == 'x' ? "max-connection-per-server"
             : shortName == 'k' ? "min-split-size"
                                : "ftp-pasv";
      spec = findLegacyOption(name);
      if (argument.size() > 2) {
        value = argument.substr(argument[2] == '=' ? 3 : 2);
      }
      else if (spec->argument == ArgumentKind::OptionalBoolean) {
        value = A2_V_TRUE;
      }
      else if (index + 1 < argc) {
        value = argv[++index];
      }
      else {
        throw DL_ABORT_EX2("Missing argument for legacy option -" +
                               std::string(1, shortName),
                           error_code::OPTION_ERROR);
      }
    }
    else {
      passthrough.push_back(std::move(argument));
      continue;
    }

    if (spec->sensitive && sensitiveValue) {
      std::fill(sensitiveValue, sensitiveValue + std::strlen(sensitiveValue),
                '*');
    }
    legacy.emplace_back(std::move(name), std::move(value));
    adapted = true;
  }

  if (!adapted) {
    return {};
  }
  const auto normalized = normalizeLegacyInput(legacy, source);
  std::vector<std::string> result;
  result.reserve(passthrough.size() + normalized.size());
  result.push_back(passthrough.front());
  for (const auto& option : normalized) {
    result.push_back("--" + option.first + '=' + option.second);
  }
  result.insert(result.end(), std::next(passthrough.begin()),
                passthrough.end());
  return result;
}

bool projectLegacyOption(const Option* option, const std::string& name,
                         std::string& value)
{
  if (!option || !isLegacyInputOption(name)) {
    return false;
  }
  if (name == "auto-save-interval") {
    value = currentValue(option, "state-save-interval");
  }
  else if (name == "split" || name == "max-connection-per-server") {
    value = currentValue(option, "stream-max-connections");
  }
  else if (name == "bt-detach-seed-only") {
    value = currentValue(option, "detach-share-only");
  }
  else if (name == "bt-force-encryption" || name == "bt-require-crypto") {
    value = currentValue(option, "bt-encryption") == V_REQUIRED ? A2_V_TRUE
                                                                : A2_V_FALSE;
  }
  else if (name == "bt-min-crypto-level") {
    value =
        currentValue(option, "bt-encryption") == V_REQUIRED ? "arc4" : "plain";
  }
  else if (name == "bt-lpd-interface" || name == "dht-listen-addr" ||
           name == "dht-listen-addr6") {
    value = currentValue(option, "bt-interface");
  }
  else if (name == "bt-metadata-only") {
    value = currentValue(option, "pause-metadata");
  }
  else if (name == "bt-prioritize-piece") {
    value = currentValue(option, "bt-first-last-piece-first") == A2_V_TRUE
                ? "head,tail"
                : std::string();
  }
  else if (name == "bt-tracker-connect-timeout") {
    value = currentValue(option, "bt-tracker-completion-timeout");
  }
  else if (name == "bt-tracker-timeout") {
    value = currentValue(option, "bt-tracker-receive-timeout");
  }
  else if (name == "dht-entry-point" || name == "dht-entry-point6") {
    value = currentValue(option, "bt-dht-bootstrap-nodes");
  }
  else if (name == "dht-listen-port") {
    value = currentValue(option, "listen-port");
  }
  else if (name == "enable-dht6") {
    value = currentValue(option, "enable-dht") == A2_V_TRUE &&
                    currentValue(option, "disable-ipv6") != A2_V_TRUE
                ? A2_V_TRUE
                : A2_V_FALSE;
  }
  else {
    return false;
  }
  return true;
}

KeyVals projectLegacyOptions(const Option* option)
{
  KeyVals result;
  for (const auto& spec : LEGACY_OPTIONS) {
    std::string value;
    if (projectLegacyOption(option, spec.name, value)) {
      result.emplace_back(spec.name, std::move(value));
    }
  }
  return result;
}

} // namespace aria2
