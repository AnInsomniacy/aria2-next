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
#include "LibtorrentSession.h"

#include "Option.h"
#include "prefs.h"
#include "util.h"
#include "SegList.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/session_params.hpp>

namespace aria2 {

namespace lt = libtorrent;

namespace {
int firstPort(const Option* option, PrefPtr pref)
{
  auto ports = util::parseIntSegments(option->get(pref));
  ports.normalize();
  if (!ports.hasNext()) {
    return 0;
  }
  return ports.next();
}

std::string listenInterfaces(const Option* option)
{
  auto tcpPort = firstPort(option, PREF_LISTEN_PORT);
  if (tcpPort == 0) {
    return {};
  }

  auto udpPort = tcpPort;
  if (option->getAsBool(PREF_ENABLE_DHT)) {
    auto dhtPort = firstPort(option, PREF_DHT_LISTEN_PORT);
    if (dhtPort != 0) {
      udpPort = dhtPort;
    }
  }

  auto spec = std::string("0.0.0.0:") + util::itos(tcpPort) +
              ",uTP:" + util::itos(udpPort);
  if (!option->getAsBool(PREF_DISABLE_IPV6)) {
    spec += ",[::]:" + util::itos(tcpPort) + ",uTP:[::]:" +
            util::itos(udpPort);
  }
  return spec;
}
} // namespace

LibtorrentSession::LibtorrentSession(const Option* option)
{
  lt::settings_pack settings;
  settings.set_str(lt::settings_pack::listen_interfaces,
                   listenInterfaces(option));
  settings.set_bool(lt::settings_pack::enable_dht,
                    option->getAsBool(PREF_ENABLE_DHT));
  settings.set_bool(lt::settings_pack::enable_lsd,
                    option->getAsBool(PREF_BT_ENABLE_LPD));
  settings.set_bool(lt::settings_pack::enable_incoming_utp, true);
  settings.set_bool(lt::settings_pack::enable_outgoing_utp, true);
  settings.set_int(lt::settings_pack::connections_limit,
                   option->getAsInt(PREF_BT_MAX_PEERS));
  settings.set_int(lt::settings_pack::download_rate_limit,
                   option->getAsInt(PREF_MAX_DOWNLOAD_LIMIT));
  settings.set_int(lt::settings_pack::upload_rate_limit,
                   option->getAsInt(PREF_MAX_UPLOAD_LIMIT));
  settings.set_int(lt::settings_pack::alert_mask,
                   static_cast<int>(static_cast<unsigned int>(
                       lt::alert_category::error |
                       lt::alert_category::storage |
                       lt::alert_category::status |
                       lt::alert_category::tracker |
                       lt::alert_category::connect | lt::alert_category::dht)));

  if (option->getAsBool(PREF_BT_FORCE_ENCRYPTION) ||
      option->getAsBool(PREF_BT_REQUIRE_CRYPTO)) {
    settings.set_int(lt::settings_pack::out_enc_policy,
                     lt::settings_pack::pe_forced);
    settings.set_int(lt::settings_pack::in_enc_policy,
                     lt::settings_pack::pe_forced);
  }
  else {
    settings.set_int(lt::settings_pack::out_enc_policy,
                     lt::settings_pack::pe_enabled);
    settings.set_int(lt::settings_pack::in_enc_policy,
                     lt::settings_pack::pe_enabled);
  }
  settings.set_int(lt::settings_pack::allowed_enc_level,
                   lt::settings_pack::pe_both);

  session_ = make_unique<lt::session>(lt::session_params(settings));
}

LibtorrentSession::~LibtorrentSession() = default;

lt::torrent_handle LibtorrentSession::addTorrent(lt::add_torrent_params params)
{
  lt::error_code ec;
  auto handle = session_->add_torrent(std::move(params), ec);
  if (ec) {
    throw DL_ABORT_EX(ec.message());
  }
  return handle;
}

void LibtorrentSession::removeTorrent(const lt::torrent_handle& handle)
{
  if (handle.is_valid()) {
    session_->remove_torrent(handle);
  }
}

void LibtorrentSession::pollAlerts(std::vector<lt::alert*>& alerts)
{
  session_->pop_alerts(&alerts);
}

} // namespace aria2
