/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "ed2k/Link.h"
#include "ed2k/Search.h"
#include "ed2k/ServerProtocol.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json/object.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aria2::ed2k {

namespace asio = boost::asio;

class Connection;
class Network;
struct Packet;

// The connection to one eDonkey server at a time, as eMule keeps it: login
// (which assigns our client ID), offering shared files, asking for sources
// of downloads every 20 minutes, callbacks for low ID clients and keyword
// searches. A few servers are dialled at once, a second apart, and the first
// to answer is used, so dead servers in the list cost little; failing ones
// back off.
class ServerSession {
public:
  explicit ServerSession(Network& network);
  ~ServerSession();

  void addServers(const std::vector<ServerMetEntry>& servers, bool configured);
  void tick(Clock::time_point now);
  void stop();

  bool connected() const { return loggedIn_; }
  uint32_t clientId() const { return loggedIn_ ? clientId_ : 0; }
  bool lowId() const { return loggedIn_ && clientId_ < 0x1000000; }
  std::optional<Endpoint> current() const;
  size_t serverCount() const { return servers_.size(); }

  // Asks the server to have a low ID client connect to us.
  bool requestCallback(uint32_t clientId, const Endpoint& sourceServer);
  bool search(const SearchQuery& query);
  bool moreResults();
  // Shared files changed; they are offered again soon.
  void filesChanged() { offerDue_ = true; }
  void describe(boost::json::object& out) const;

private:
  struct Server {
    ServerMetEntry entry;
    bool configured = false;
    bool obfuscationFailed = false;
    int failures = 0;
    Clock::time_point retryAt{};
  };

  Server* find(const std::string& key);
  const Server* find(const std::string& key) const;
  asio::awaitable<void> run(std::string key);
  void handle(const Packet& packet);
  void loggedIn(uint32_t clientId, uint32_t flags);
  void offerFiles();
  void requestSources(Clock::time_point now);
  void send(uint8_t opcode, const std::string& payload);
  void failed(const std::string& key, const std::string& why);

  Network& network_;
  std::vector<Server> servers_;
  // Dials still racing, by server; the first connected one logs in.
  std::map<std::string, std::shared_ptr<asio::ip::tcp::socket>> dials_;
  std::shared_ptr<Connection> connection_;
  std::string current_; // the server logging in or logged in
  bool loggedIn_ = false;
  bool stopped_ = false;
  uint32_t clientId_ = 0;
  uint32_t flags_ = 0;
  std::string name_;
  std::string message_;
  uint32_t users_ = 0;
  uint32_t files_ = 0;
  std::map<std::string, Clock::time_point>
      asked_; // file hash -> last source request
  Clock::time_point lastOffer_{};
  bool offerDue_ = true;
  size_t next_ = 0;
};

} // namespace aria2::ed2k
