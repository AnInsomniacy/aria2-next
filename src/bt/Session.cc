/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "bt/Session.h"

#include "base/Error.h"
#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "bt/Blocklist.h"
#include "bt/BtTask.h"
#include "core/Engine.h"
#include "state/StateStore.h"

#include <boost/asio/post.hpp>
#include <boost/json/array.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/bdecode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/session_stats.hpp>

#include <algorithm>
#include <thread>

namespace aria2::bt {

namespace asio = boost::asio;
namespace json = boost::json;
using namespace std::chrono_literals;

namespace {

std::vector<std::string> interfaces(const Options& options)
{
  std::vector<std::string> result;
  for (const auto& part : text::split(options.get("bt-interface"), ',')) {
    auto name = std::string(text::trim(part));
    if (name.size() > 2 && name.front() == '[' && name.back() == ']') {
      name = name.substr(1, name.size() - 2);
    }
    if (!name.empty() &&
        !(options.flag("disable-ipv6") && name.find(':') != std::string::npos)) {
      result.push_back(std::move(name));
    }
  }
  return result;
}

std::string listenInterfaces(const Options& options)
{
  const auto port = std::to_string(options.integer("listen-port"));
  const auto chosen = interfaces(options);
  if (chosen.empty()) {
    return options.flag("disable-ipv6") ? "0.0.0.0:" + port
                                        : "0.0.0.0:" + port + ",[::]:" + port;
  }
  std::string result;
  for (const auto& name : chosen) {
    result += (result.empty() ? "" : ",") +
              (name.find(':') != std::string::npos ? '[' + name + ']' : name) + ':' + port;
  }
  return result;
}

void applyProxy(lt::settings_pack& pack, const Options& options, bool& dht, bool& local)
{
  const auto value = options.get("bt-proxy");
  if (value.empty()) {
    pack.set_int(lt::settings_pack::proxy_type, lt::settings_pack::none);
    return;
  }
  const auto parts = uri::parse(value);
  if (!parts || parts->host.empty() || parts->port.empty()) {
    fail(ErrorCode::Option, "Invalid bt-proxy {}", uri::redact(value));
  }
  const bool credentials = !parts->user.empty();
  int type = lt::settings_pack::none;
  if (parts->scheme == "http") {
    type = credentials ? lt::settings_pack::http_pw : lt::settings_pack::http;
    dht = false; // DHT is UDP, which HTTP proxies cannot carry
  }
  else if (parts->scheme == "socks4") {
    type = lt::settings_pack::socks4;
    dht = false;
  }
  else if (parts->scheme == "socks5" || parts->scheme == "socks5h") {
    type = credentials ? lt::settings_pack::socks5_pw : lt::settings_pack::socks5;
  }
  else {
    fail(ErrorCode::Option, "bt-proxy must use http, socks4 or socks5");
  }
  // Discovery and port mapping would reveal the real address.
  local = false;
  pack.set_int(lt::settings_pack::proxy_type, type);
  pack.set_str(lt::settings_pack::proxy_hostname, parts->host);
  pack.set_int(lt::settings_pack::proxy_port, static_cast<int>(text::toInt(parts->port).value_or(0)));
  pack.set_str(lt::settings_pack::proxy_username, text::percentDecode(parts->user));
  pack.set_str(lt::settings_pack::proxy_password, text::percentDecode(parts->password));
  pack.set_bool(lt::settings_pack::proxy_hostnames, true);
  pack.set_bool(lt::settings_pack::proxy_peer_connections, true);
  pack.set_bool(lt::settings_pack::proxy_tracker_connections, true);
}

int clampInt(int64_t value) { return static_cast<int>(std::clamp<int64_t>(value, 0, INT32_MAX)); }

// Options map to libtorrent settings; everything else keeps tuned values in
// line with qBittorrent's defaults.
lt::settings_pack settingsFor(const Options& options)
{
  lt::settings_pack pack;
  const auto userAgent = options.get("bt-user-agent");
  if (userAgent.find_first_of("\r\n") != std::string::npos) {
    fail(ErrorCode::Option, "bt-user-agent must not contain line breaks");
  }
  if (options.get("bt-peer-id-prefix").size() > 20) {
    fail(ErrorCode::Option, "bt-peer-id-prefix must not exceed 20 bytes");
  }
  pack.set_str(lt::settings_pack::listen_interfaces, listenInterfaces(options));
  if (const auto chosen = interfaces(options); !chosen.empty()) {
    pack.set_str(lt::settings_pack::outgoing_interfaces, text::join(chosen, ","));
  }
  // An occupied port moves to the next free one rather than disabling
  // incoming connections; the status reports the port in use.
  pack.set_int(lt::settings_pack::max_retry_port_bind, 20);

  bool dht = options.flag("enable-dht");
  bool local = true;
  applyProxy(pack, options, dht, local);
  pack.set_bool(lt::settings_pack::enable_dht, dht);
  pack.set_bool(lt::settings_pack::enable_lsd, local && options.flag("bt-enable-lpd"));
  const bool mapping = local && options.flag("bt-port-mapping");
  pack.set_bool(lt::settings_pack::enable_upnp, mapping);
  pack.set_bool(lt::settings_pack::enable_natpmp, mapping);

  const auto transport = options.get("bt-transport");
  const bool tcp = transport != "utp";
  const bool utp = transport != "tcp";
  pack.set_bool(lt::settings_pack::enable_incoming_tcp, tcp);
  pack.set_bool(lt::settings_pack::enable_outgoing_tcp, tcp);
  pack.set_bool(lt::settings_pack::enable_incoming_utp, utp);
  pack.set_bool(lt::settings_pack::enable_outgoing_utp, utp);
  pack.set_int(lt::settings_pack::mixed_mode_algorithm, lt::settings_pack::prefer_tcp);

  const auto encryption = options.get("bt-encryption");
  const int policy = encryption == "required"   ? lt::settings_pack::pe_forced
                     : encryption == "disabled" ? lt::settings_pack::pe_disabled
                                                : lt::settings_pack::pe_enabled;
  pack.set_int(lt::settings_pack::out_enc_policy, policy);
  pack.set_int(lt::settings_pack::in_enc_policy, policy);
  pack.set_int(lt::settings_pack::allowed_enc_level, lt::settings_pack::pe_both);
  pack.set_bool(lt::settings_pack::prefer_rc4, false);
  pack.set_bool(lt::settings_pack::prefer_encrypted_connections, encryption == "preferred");

  pack.set_str(lt::settings_pack::user_agent, userAgent);
  pack.set_str(lt::settings_pack::peer_fingerprint, options.get("bt-peer-id-prefix"));
  pack.set_str(lt::settings_pack::dht_bootstrap_nodes, options.get("bt-dht-bootstrap-nodes"));
  pack.set_bool(lt::settings_pack::anonymous_mode, options.flag("bt-anonymous-mode"));
  if (!options.blank("bt-external-ip")) {
    pack.set_str(lt::settings_pack::announce_ip, options.get("bt-external-ip"));
  }
  pack.set_int(lt::settings_pack::announce_port, clampInt(options.integer("bt-external-port")));

  pack.set_int(lt::settings_pack::connections_limit, clampInt(options.integer("bt-max-connections")));
  pack.set_int(lt::settings_pack::unchoke_slots_limit, clampInt(options.integer("bt-max-uploads")));
  pack.set_int(lt::settings_pack::upload_rate_limit,
               clampInt(options.integer("max-overall-upload-limit")));
  pack.set_int(lt::settings_pack::download_rate_limit,
               clampInt(options.integer("max-overall-download-limit")));
  pack.set_bool(lt::settings_pack::rate_limit_ip_overhead, options.flag("bt-rate-limit-overhead"));

  const auto scope = options.get("bt-blocklist-scope");
  pack.set_bool(lt::settings_pack::apply_ip_filter_to_trackers, scope != "peers");
  pack.set_bool(lt::settings_pack::apply_filter_to_dht, scope == "all");

  // The engine queue decides which torrents run.
  pack.set_int(lt::settings_pack::active_downloads, -1);
  pack.set_int(lt::settings_pack::active_seeds, -1);
  pack.set_int(lt::settings_pack::active_limit, -1);
  pack.set_int(lt::settings_pack::active_tracker_limit, -1);
  pack.set_int(lt::settings_pack::active_dht_limit, -1);
  pack.set_int(lt::settings_pack::active_lsd_limit, -1);

  pack.set_bool(lt::settings_pack::announce_to_all_tiers, true);
  pack.set_bool(lt::settings_pack::announce_to_all_trackers, false);
  pack.set_int(lt::settings_pack::max_concurrent_http_announces, 50);
  pack.set_int(lt::settings_pack::stop_tracker_timeout, 2);
  pack.set_int(lt::settings_pack::connection_speed, 30);
  pack.set_int(lt::settings_pack::max_out_request_queue, 500);
  pack.set_int(lt::settings_pack::max_allowed_in_request_queue, 2000);
  pack.set_int(lt::settings_pack::send_buffer_watermark, 500 * 1024);
  pack.set_int(lt::settings_pack::send_buffer_low_watermark, 10 * 1024);
  pack.set_int(lt::settings_pack::send_buffer_watermark_factor, 50);
  pack.set_int(lt::settings_pack::peer_turnover, 4);
  pack.set_int(lt::settings_pack::peer_turnover_cutoff, 90);
  pack.set_int(lt::settings_pack::peer_turnover_interval, 300);
  pack.set_int(lt::settings_pack::aio_threads, 10);
  const auto cores = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
  pack.set_int(lt::settings_pack::hashing_threads, std::clamp(cores / 2, 1, 4));
  pack.set_int(lt::settings_pack::file_pool_size, 100);
  pack.set_int(lt::settings_pack::max_queued_disk_bytes, 64 << 20);
  pack.set_bool(lt::settings_pack::validate_https_trackers, true);
  pack.set_bool(lt::settings_pack::ssrf_mitigation, true);

  pack.set_int(lt::settings_pack::alert_queue_size, 16384);
  pack.set_int(lt::settings_pack::alert_mask,
               lt::alert_category::error | lt::alert_category::status |
                   lt::alert_category::storage | lt::alert_category::file_progress |
                   lt::alert_category::port_mapping |
                   lt::alert_category::performance_warning);
  return pack;
}

bool affectsSession(std::string_view name)
{
  return name.starts_with("bt-") || name == "listen-port" || name == "enable-dht" ||
         name == "enable-peer-exchange" || name == "disable-ipv6" ||
         name == "max-overall-upload-limit" || name == "max-overall-download-limit";
}

} // namespace

Session::Session(Engine& engine) : engine_(engine), ticker_(engine.io())
{
  lt::session_params params;
  const auto statePath = joinPath(engine.state().directory("bittorrent"), "session");
  if (const auto saved = readFile(statePath)) {
    lt::error_code error;
    const auto node = lt::bdecode({saved->data(), static_cast<std::ptrdiff_t>(saved->size())}, error);
    if (!error) {
      params = lt::read_session_params(node, lt::session::save_dht_state);
    }
  }
  params.settings = settingsFor(engine.global());
  session_ = std::make_unique<lt::session>(std::move(params));
  session_->set_alert_notify([this] { schedule(); });
  const auto& metrics = lt::session_stats_metrics();
  counters_.resize(metrics.size());
  dhtNodesMetric_ = lt::find_metric_idx("dht.dht_nodes");
  peersMetric_ = lt::find_metric_idx("peer.num_peers_connected");
  downloadedMetric_ = lt::find_metric_idx("net.recv_payload_bytes");
  uploadedMetric_ = lt::find_metric_idx("net.sent_payload_bytes");
  loadBlocklist();
  tick();
}

Session::~Session() { close(); }

void Session::schedule()
{
  // Runs on a libtorrent thread: only hand over to the control thread.
  if (!drainPosted_.exchange(true)) {
    asio::post(engine_.io(), [this] { drain(); });
  }
}

void Session::drain()
{
  drainPosted_ = false;
  if (!session_) {
    return;
  }
  std::vector<lt::alert*> alerts;
  session_->pop_alerts(&alerts);
  for (auto* alert : alerts) {
    try {
      dispatch(alert);
    }
    catch (const std::exception& error) {
      log::error("BitTorrent alert {} failed: {}", alert->what(), error.what());
    }
  }
}

std::shared_ptr<BtTask> Session::taskFor(const lt::torrent_handle& handle) const
{
  const auto found = tasks_.find(handle);
  return found == tasks_.end() ? nullptr : found->second.lock();
}

void Session::dispatch(lt::alert* alert)
{
  // Web seed, tracker, peer and file errors explain a stalled swarm.
  if (alert->category() & lt::alert_category::error) {
    log::debug("BitTorrent: {}", alert->message());
  }
  if (auto* added = lt::alert_cast<lt::add_torrent_alert>(alert)) {
    auto* owner = added->params.userdata.get<BtTask>();
    const auto found = adding_.find(owner);
    auto task = found == adding_.end() ? nullptr : found->second.lock();
    if (found != adding_.end()) {
      adding_.erase(found);
    }
    if (added->error) {
      if (task) {
        task->addFailed(added->error);
      }
      return;
    }
    if (!task) {
      session_->remove_torrent(added->handle); // the task is gone
      return;
    }
    tasks_[added->handle] = task;
    task->attached(added->handle);
    return;
  }
  if (auto* updates = lt::alert_cast<lt::state_update_alert>(alert)) {
    for (const auto& status : updates->status) {
      if (auto task = taskFor(status.handle)) {
        task->updated(status);
      }
    }
    return;
  }
  if (auto* removed = lt::alert_cast<lt::torrent_removed_alert>(alert)) {
    const auto task = taskFor(removed->handle);
    tasks_.erase(removed->handle);
    if (task) {
      task->detached();
    }
    return;
  }
  if (auto* torrent = dynamic_cast<lt::torrent_alert*>(alert)) {
    if (auto task = taskFor(torrent->handle)) {
      task->handle(alert);
    }
    return;
  }
  if (auto* stats = lt::alert_cast<lt::session_stats_alert>(alert)) {
    const auto values = stats->counters();
    std::copy(values.begin(), values.end(), counters_.begin());
  }
  else if (auto* listening = lt::alert_cast<lt::listen_succeeded_alert>(alert)) {
    const auto address = listening->address.to_string();
    listenPorts_[listening->address.is_v6() ? '[' + address + ']' : address] = listening->port;
    // Names the socket type: TCP and uTP listen on the same address and port.
    log::info("BitTorrent: {}", listening->message());
  }
  else if (auto* failed = lt::alert_cast<lt::listen_failed_alert>(alert)) {
    log::warn("BitTorrent: cannot listen on {}: {}", failed->address.to_string(),
              failed->error.message());
  }
  else if (auto* mapped = lt::alert_cast<lt::portmap_alert>(alert)) {
    (mapped->map_protocol == lt::portmap_protocol::tcp ? mappedTcpPort_ : mappedUdpPort_) =
        mapped->external_port;
    portMappingError_.clear();
  }
  else if (auto* mapError = lt::alert_cast<lt::portmap_error_alert>(alert)) {
    portMappingError_ = mapError->error.message();
    log::info("BitTorrent: port mapping failed: {}", portMappingError_);
  }
  else if (auto* external = lt::alert_cast<lt::external_ip_alert>(alert)) {
    externalAddress_ = external->external_address.to_string();
  }
  else if (auto* warning = lt::alert_cast<lt::performance_alert>(alert)) {
    log::info("BitTorrent: {}", warning->message());
  }
}

void Session::tick()
{
  if (!session_) {
    return;
  }
  session_->post_torrent_updates(lt::torrent_handle::query_pieces);
  if (ticks_++ % 5 == 0) {
    session_->post_session_stats();
  }
  ticker_.expires_after(1s);
  ticker_.async_wait([this](const boost::system::error_code& error) {
    if (!error) {
      tick();
    }
  });
}

void Session::loadBlocklist()
{
  const auto path = engine_.global().get("bt-peer-blocklist");
  if (path.empty()) {
    session_->set_ip_filter({});
    blocklistRules_ = 0;
    return;
  }
  const auto content = readFile(path);
  if (!content) {
    log::warn("Cannot read the peer blocklist {}", path);
    return;
  }
  auto blocklist = parseBlocklist(*content);
  blocklistRules_ = blocklist.rules;
  log::info("Peer blocklist {}: {} rules, {} lines skipped", path, blocklist.rules,
            blocklist.skipped);
  session_->set_ip_filter(std::move(blocklist.filter));
}

void Session::optionsChanged(const std::vector<std::string>& names)
{
  if (!session_ || std::none_of(names.begin(), names.end(), affectsSession)) {
    return;
  }
  try {
    session_->apply_settings(settingsFor(engine_.global()));
  }
  catch (const Error& error) {
    log::error("BitTorrent settings not applied: {}", error.what());
  }
  if (std::find(names.begin(), names.end(), "bt-peer-blocklist") != names.end()) {
    loadBlocklist();
  }
  if (std::find(names.begin(), names.end(), "listen-port") != names.end()) {
    listenPorts_.clear();
  }
}

void Session::add(const std::shared_ptr<BtTask>& task, lt::add_torrent_params params)
{
  params.userdata = lt::client_data_t(task.get());
  adding_[task.get()] = task;
  session_->async_add_torrent(std::move(params));
}

void Session::remove(const lt::torrent_handle& handle)
{
  session_->remove_torrent(handle);
}

void Session::saveState()
{
  if (!engine_.global().flag("enable-dht")) {
    return;
  }
  const auto params = session_->session_state(lt::session::save_dht_state);
  const auto data = lt::write_session_params_buf(params, lt::session::save_dht_state);
  const auto path = joinPath(engine_.state().directory("bittorrent"), "session");
  if (!writeFileAtomically(path, {data.data(), data.size()})) {
    log::warn("Cannot save the DHT state to {}", path);
  }
}

void Session::close()
{
  if (closed_ || !session_) {
    return;
  }
  closed_ = true;
  ticker_.cancel();
  session_->set_alert_notify({});
  saveState();
  // Destroying the session waits for tracker "stopped" announces, bounded by
  // stop_tracker_timeout.
  session_.reset();
}

json::object Session::status() const
{
  json::object out;
  int listenPort = 0;
  json::array endpoints;
  for (const auto& [endpoint, port] : listenPorts_) {
    endpoints.push_back(json::string(endpoint + ':' + std::to_string(port)));
    listenPort = listenPort ? listenPort : port;
  }
  const auto counter = [this](int index) {
    return index >= 0 && static_cast<size_t>(index) < counters_.size()
               ? std::to_string(counters_[static_cast<size_t>(index)])
               : std::string("0");
  };
  const auto dhtNodes = counter(dhtNodesMetric_);
  const auto announcePort = engine_.global().integer("bt-external-port");
  out["listenPort"] = std::to_string(listenPort);
  out["announcePort"] = std::to_string(announcePort > 0 ? announcePort : listenPort);
  out["listenEndpoints"] = std::move(endpoints);
  out["externalIp"] = externalAddress_;
  out["mappedTcpPort"] = std::to_string(mappedTcpPort_);
  out["mappedUdpPort"] = std::to_string(mappedUdpPort_);
  if (!portMappingError_.empty()) {
    out["portMappingError"] = portMappingError_;
  }
  out["dhtNodes"] = dhtNodes;
  out["dhtStateHealthy"] = dhtNodes != "0" ? "true" : "false";
  out["peerSockets"] = counter(peersMetric_);
  out["payloadDownloaded"] = counter(downloadedMetric_);
  out["payloadUploaded"] = counter(uploadedMetric_);
  out["blocklistRules"] = std::to_string(blocklistRules_);
  return out;
}

} // namespace aria2::bt
