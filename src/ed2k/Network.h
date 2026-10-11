/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "ed2k/Link.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/json/object.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace aria2 {
class Engine;
}

namespace aria2::net {
class PortMapper;
}

namespace aria2::ed2k {

namespace asio = boost::asio;

class Client;
class Connection;
class Ed2kTask;
class Kad;
class Searches;
class ServerSession;
class Store;
class UploadQueue;

// The eDonkey network presence shared by all ED2K tasks, as aMule has one:
// our user hash, the TCP and UDP listeners, the server connection, Kad, the
// known clients and the upload queue. Created when the first ED2K task
// starts or a search is made; it lives until the engine shuts down.
class Network {
public:
  explicit Network(Engine& engine);
  ~Network();
  Network(const Network&) = delete;
  Network& operator=(const Network&) = delete;

  // Leaves the network; `done` runs once the port mappings are removed.
  void close(std::function<void()> done);
  Engine& engine() { return engine_; }
  Store& store() { return *store_; }
  ServerSession& server() { return *server_; }
  Kad& kad() { return *kad_; }
  UploadQueue& uploads() { return *uploads_; }
  Searches& searches() { return *searches_; }

  const std::string& userHash() const { return userHash_; }
  uint16_t tcpPort() const { return tcpPort_; }
  uint16_t udpPort() const { return udpPort_; }
  // The ID other clients know us by: the server's, else our public address
  // if known, else 0.
  uint32_t clientId() const;
  Endpoint serverEndpoint() const;
  std::optional<asio::ip::address_v4> publicAddress() const;
  void observedAddress(const asio::ip::address_v4& address);
  // Global upload and download limits (max-overall-*-limit).
  RateLimit& uploadLimit() { return uploadLimit_; }
  RateLimit& downloadLimit() { return downloadLimit_; }
  void globalOptionsChanged();

  // Files we download or share, by MD4 hash.
  void attach(Ed2kTask& task);
  void detach(Ed2kTask& task);
  Ed2kTask* file(const std::string& hash) const;
  const std::map<std::string, Ed2kTask*>& files() const { return files_; }
  // Servers and Kad nodes named by a task's options join the global ones.
  void addBootstrap(const std::string& servers, const std::string& serverList,
                    const std::string& nodeList);

  // The client at this address, or with this low ID when the address is
  // unspecified; created when unknown.
  std::shared_ptr<Client> client(const asio::ip::address_v4& address,
                                 uint16_t port, uint32_t clientId = 0);
  std::shared_ptr<Client> findByHash(const std::string& userHash) const;
  // An accepted connection said who it is: an earlier object for the same
  // client takes it over.
  std::shared_ptr<Client> identified(const std::shared_ptr<Client>& client);
  void connectionOpened();
  void connectionClosed();
  bool mayConnect() const;

  void sendUdp(const asio::ip::udp::endpoint& to, std::string datagram);

  // The "ed2k" object of a task status and aria2.getEd2kStatus.
  void describe(boost::json::object& out) const;

private:
  asio::awaitable<void> acceptLoop();
  asio::awaitable<void> serve(asio::ip::tcp::socket socket);
  asio::awaitable<void> receiveLoop();
  void datagram(const asio::ip::udp::endpoint& from, std::string data);
  void clientDatagram(const asio::ip::udp::endpoint& from,
                      const std::string& data);
  void scheduleTick();
  void tick();
  void listen();
  // Starts or stops the UPnP forwarding to follow ed2k-port-mapping.
  void updatePortMapping();

  Engine& engine_;
  std::unique_ptr<Store> store_;
  std::string userHash_;
  uint16_t tcpPort_ = 0;
  uint16_t udpPort_ = 0;
  std::optional<asio::ip::address_v4> publicAddress_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::unique_ptr<asio::ip::udp::socket> udp_;
  std::unique_ptr<ServerSession> server_;
  std::unique_ptr<Kad> kad_;
  std::unique_ptr<UploadQueue> uploads_;
  std::unique_ptr<Searches> searches_;
  std::unique_ptr<net::PortMapper> portMapper_;
  std::map<std::string, Ed2kTask*> files_;
  std::map<std::pair<uint32_t, uint16_t>, std::shared_ptr<Client>> clients_;
  RateLimit uploadLimit_;
  RateLimit downloadLimit_;
  asio::steady_timer ticker_;
  int openConnections_ = 0;
  std::set<std::string> bootstrapped_; // servers and lists already added
  bool closed_ = false;
  Clock::time_point lastCreditSave_ = Clock::now();
};

} // namespace aria2::ed2k
