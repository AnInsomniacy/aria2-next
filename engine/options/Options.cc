/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"

#include "base/Error.h"
#include "base/Log.h"
#include "base/Text.h"

#include <array>
#include <cassert>
#include <charconv>
#include <mutex>
#include <set>

namespace aria2 {
namespace {

// aria2 and earlier aria2-next options that no longer have a counterpart.
// They are accepted so existing configurations, scripts and frontends keep
// working; each one is reported once.
constexpr std::array retired{
    "allow-piece-length-change", "always-resume", "async-dns",
    "async-dns-server", "bt-announce-all-tiers", "bt-announce-all-trackers",
    "bt-checking-memory", "bt-connection-speed", "bt-detach-seed-only",
    "bt-disk-io", "bt-disk-queue-size", "bt-disk-read-cache",
    "bt-disk-write-cache", "bt-enable-hook-after-hash-check",
    "bt-hash-check-seed", "bt-hashing-threads", "bt-io-threads",
    "bt-keep-alive-interval", "bt-load-saved-metadata", "bt-lpd-interface",
    "bt-max-concurrent-http-announces", "bt-max-in-request-queue",
    "bt-max-open-files", "bt-max-out-request-queue", "bt-metadata-only",
    "bt-min-crypto-level", "bt-mixed-mode", "bt-peer-turnover",
    "bt-peer-turnover-cutoff", "bt-peer-turnover-interval",
    "bt-piece-extent-affinity", "bt-prioritize-piece",
    "bt-remove-unselected-file", "bt-request-peer-speed-limit",
    "bt-request-timeout", "bt-resume-save-interval", "bt-save-metadata",
    "bt-seed-choking-algorithm", "bt-seeding-outgoing-connections",
    "bt-send-buffer-low-watermark", "bt-send-buffer-watermark",
    "bt-send-buffer-watermark-factor", "bt-stop-timeout",
    "bt-stop-tracker-timeout", "bt-timeout", "bt-tracker-completion-timeout",
    "bt-tracker-connect-timeout", "bt-tracker-interval",
    "bt-tracker-receive-timeout", "bt-tracker-timeout",
    "bt-upload-slot-algorithm", "bt-upload-suggestions", "conditional-get",
    "content-disposition-default-utf8", "deferred-input", "dht-entry-point",
    "dht-entry-point-host", "dht-entry-point-host6", "dht-entry-point-port",
    "dht-entry-point-port6", "dht-entry-point6", "dht-file-path",
    "dht-file-path6", "dht-listen-addr", "dht-listen-addr6", "dht-listen-port",
    "dht-message-timeout", "disk-cache", "download-result", "dscp",
    "ed2k-min-split-size", "ed2k-piece-selector", "enable-async-dns6",
    "enable-dht6", "enable-http-pipelining", "enable-mmap", "event-poll",
    "ftp-passwd", "ftp-pasv", "ftp-proxy", "ftp-proxy-passwd",
    "ftp-proxy-user", "ftp-reuse-connection", "ftp-type", "ftp-user",
    "http-auth-challenge", "human-readable", "max-http-pipelining",
    "max-mmap-limit", "metalink-base-uri", "metalink-enable-unique-protocol",
    "metalink-language", "metalink-location", "metalink-os",
    "metalink-preferred-protocol", "metalink-version", "multiple-interface",
    "no-file-allocation-limit", "no-want-digest-header",
    "optimize-concurrent-downloads", "parameterized-uri", "peer-agent",
    "peer-connection-timeout", "peer-id-prefix", "piece-length",
    "proxy-method", "realtime-chunk-checksum", "reuse-uri", "rlimit-nofile",
    "rpc-certificate", "rpc-passwd", "rpc-private-key", "rpc-secure",
    "rpc-user", "select-least-used-host", "server-stat-if", "server-stat-of",
    "server-stat-timeout", "socket-recv-buffer-size", "ssh-host-key-md",
    "startup-idle-time", "stderr", "stream-max-range-size",
    "stream-piece-selector", "truncate-console-readout", "uri-selector",
    "use-head"};

constexpr std::array retiredFlags{
    "allow-piece-length-change", "always-resume", "async-dns",
    "bt-announce-all-tiers", "bt-announce-all-trackers",
    "bt-detach-seed-only", "bt-enable-hook-after-hash-check",
    "bt-force-encryption", "bt-hash-check-seed", "bt-load-saved-metadata",
    "bt-metadata-only", "bt-piece-extent-affinity",
    "bt-remove-unselected-file", "bt-require-crypto", "bt-save-metadata",
    "bt-seeding-outgoing-connections", "bt-upload-suggestions",
    "conditional-get", "content-disposition-default-utf8", "deferred-input",
    "enable-async-dns6", "enable-dht6", "enable-http-pipelining",
    "enable-mmap", "ftp-pasv", "ftp-reuse-connection", "http-auth-challenge",
    "human-readable", "metalink-enable-unique-protocol",
    "no-want-digest-header", "optimize-concurrent-downloads",
    "parameterized-uri", "realtime-chunk-checksum", "reuse-uri", "rpc-secure",
    "select-least-used-host", "stderr", "truncate-console-readout",
    "use-head"};

void reportRetired(std::string_view name, std::string_view outcome)
{
  static std::mutex mutex;
  static std::set<std::string, std::less<>> reported;
  std::lock_guard lock(mutex);
  if (reported.emplace(name).second) {
    log::warn("Option --{} is retired; {}", name, outcome);
  }
}

bool isRetired(std::string_view name)
{
  return std::find(retired.begin(), retired.end(), name) != retired.end();
}

[[noreturn]] void invalid(std::string_view name, std::string_view value,
                          std::string_view reason)
{
  fail(ErrorCode::Option, "Invalid value for --{}: '{}' ({})", name, value,
       reason);
}

std::string normalizeInteger(const OptionSpec& spec, std::string_view value)
{
  int64_t number = 0;
  const auto text = text::trim(value);
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), number);
  if (text.empty() || error != std::errc() || end != text.data() + text.size()) {
    invalid(spec.name, value, "not an integer");
  }
  if (number < spec.min || number > spec.max) {
    invalid(spec.name, value,
            fmt::format("expected {}..{}", spec.min, spec.max));
  }
  return std::to_string(number);
}

} // namespace

bool isRetiredFlag(std::string_view name)
{
  return std::find(retiredFlags.begin(), retiredFlags.end(), name) !=
         retiredFlags.end();
}

std::string normalizeOption(const OptionSpec& spec, std::string_view value)
{
  switch (spec.type) {
  case OptionType::Flag:
    if (value == "true" || value == "false") {
      return std::string(value);
    }
    invalid(spec.name, value, "expected true or false");
  case OptionType::Integer:
    return normalizeInteger(spec, value);
  case OptionType::Size: {
    const auto size = text::toSize(value);
    if (!size || *size < spec.min || *size > spec.max) {
      invalid(spec.name, value, "expected a size such as 1M");
    }
    return std::to_string(*size);
  }
  case OptionType::Number: {
    double number = 0;
    const auto text = text::trim(value);
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), number);
    if (!text.empty() &&
        (error != std::errc() || end != text.data() + text.size() ||
         number < 0)) {
      invalid(spec.name, value, "expected a non-negative number");
    }
    return std::string(text);
  }
  case OptionType::Choice:
    for (const auto& choice : text::split(spec.choices, '|')) {
      if (choice == value) {
        return choice;
      }
    }
    invalid(spec.name, value, fmt::format("expected {}", spec.choices));
  case OptionType::Text:
  case OptionType::Path:
  case OptionType::List:
    return std::string(value);
  }
  return std::string(value);
}

Options::Options(std::shared_ptr<const Options> parent)
    : parent_(std::move(parent))
{
}

void Options::set(std::string_view name, std::string_view value)
{
  if (name == "stream-max-connections") {
    reportRetired(name, "use --split and --max-connection-per-server");
    set("split", value);
    set("max-connection-per-server", value);
    return;
  }
  if ((name == "bt-require-crypto" || name == "bt-force-encryption")) {
    reportRetired(name, "use --bt-encryption");
    if (value != "false") {
      set("bt-encryption", "required");
    }
    return;
  }
  if (name == "auto-save-interval") {
    reportRetired(name, "use --state-save-interval");
    set("state-save-interval", value);
    return;
  }
  if (isRetired(name)) {
    reportRetired(name, "ignored");
    return;
  }
  const auto* spec = findOption(name);
  if (!spec) {
    fail(ErrorCode::Option, "Unknown option --{}", name);
  }
  if (name == "listen-port") {
    // aria2 accepted ranges such as 6881-6999; libtorrent needs one port.
    value = value.substr(0, value.find_first_of("-,"));
  }
  if (name == "log-level" || name == "console-log-level") {
    if (value == "warning") {
      value = "warn";
    }
  }
  values_[std::string(name)] = normalizeOption(*spec, value);
}

void Options::add(std::string_view name, std::string_view value)
{
  const auto* spec = findOption(name);
  if (!spec || spec->type != OptionType::List) {
    set(name, value);
    return;
  }
  auto& current = values_[std::string(name)];
  if (!current.empty()) {
    current += '\n';
  }
  current += value;
}

bool Options::defined(std::string_view name) const
{
  return hasOwn(name) || (parent_ && parent_->defined(name));
}

bool Options::hasOwn(std::string_view name) const
{
  return values_.find(name) != values_.end();
}

std::string Options::get(std::string_view name) const
{
  for (const auto* layer = this; layer; layer = layer->parent_.get()) {
    if (const auto found = layer->values_.find(name);
        found != layer->values_.end()) {
      return found->second;
    }
  }
  const auto* spec = findOption(name);
  assert(spec && "unknown option name");
  return spec ? std::string(spec->defaultValue) : std::string();
}

bool Options::flag(std::string_view name) const { return get(name) == "true"; }

int64_t Options::integer(std::string_view name) const
{
  // Stored values are canonical integers; catalogue defaults may carry units.
  return text::toSize(get(name)).value_or(0);
}

double Options::number(std::string_view name) const
{
  const auto value = get(name);
  double number = 0;
  std::from_chars(value.data(), value.data() + value.size(), number);
  return number;
}

std::map<std::string, std::string> Options::visible() const
{
  std::map<std::string, std::string> result;
  if (parent_) {
    result = parent_->visible();
  }
  for (const auto& [name, value] : values_) {
    const auto* spec = findOption(name);
    if (spec && !(spec->scope & scope::Secret)) {
      result[name] = value;
    }
  }
  return result;
}

} // namespace aria2
