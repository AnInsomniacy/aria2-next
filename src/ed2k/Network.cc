/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Network.h"

#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "core/Engine.h"
#include "ed2k/Client.h"
#include "ed2k/Connection.h"
#include "ed2k/Constants.h"
#include "ed2k/Crypto.h"
#include "ed2k/Ed2kTask.h"
#include "ed2k/Kad.h"
#include "ed2k/Packet.h"
#include "ed2k/Searches.h"
#include "ed2k/ServerProtocol.h"
#include "ed2k/ServerSession.h"
#include "ed2k/Store.h"
#include "ed2k/UploadQueue.h"
#include "net/PortMapper.h"
#include "state/StateStore.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <array>

namespace aria2::ed2k {

using asio::awaitable;
using asio::use_awaitable;
using namespace std::chrono_literals;

namespace {

// The address as eDonkey IDs and packets carry it: network byte order read
// as a little-endian number.
uint32_t idOf(const asio::ip::address_v4& address)
{
  const auto bytes = address.to_bytes();
  return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

constexpr auto creditSaveInterval = 5min;
// Ports tried after the configured one when it is taken.
constexpr int portRetries = 10;

} // namespace

Network::Network(Engine& engine) : engine_(engine), ticker_(engine.io())
{
  store_ = std::make_unique<Store>(
      joinPath(engine.state().directory("ed2k"), "state.db"));
  userHash_ = store_->userHash();
  uploads_ = std::make_unique<UploadQueue>(*this);
  uploads_->restore(store_->credits());
  server_ = std::make_unique<ServerSession>(*this);
  kad_ = std::make_unique<Kad>(*this);
  searches_ = std::make_unique<Searches>(*this);
  globalOptionsChanged();
  listen();
  const auto& global = engine.global();
  addBootstrap(global.get("ed2k-server"), global.get("ed2k-server-list"),
               global.get("ed2k-node-list"));
  updatePortMapping();
  scheduleTick();
  log::info("ED2K: user hash {}, TCP port {}, UDP port {}",
            text::toHex(userHash_), tcpPort_, udpPort_);
}

Network::~Network() = default;

void Network::listen()
{
  const auto& global = engine_.global();
  const auto tcpPort = static_cast<int>(global.integer("ed2k-listen-port"));
  const auto udpPort = static_cast<int>(global.integer("ed2k-udp-listen-port"));
  for (int i = 0; i <= portRetries && !acceptor_; ++i) {
    const auto port = tcpPort ? tcpPort + i : 0;
    if (port > 65535) {
      break;
    }
    try {
      auto acceptor = std::make_unique<asio::ip::tcp::acceptor>(
          engine_.io(), asio::ip::tcp::endpoint(asio::ip::tcp::v4(),
                                                static_cast<uint16_t>(port)));
      tcpPort_ = acceptor->local_endpoint().port();
      acceptor_ = std::move(acceptor);
    }
    catch (const std::exception& error) {
      log::debug("ED2K: TCP port {} unavailable: {}", port, error.what());
    }
  }
  if (!acceptor_) {
    log::warn(
        "ED2K: cannot listen on a TCP port; other clients cannot connect");
  }
  for (int i = 0; i <= portRetries && !udp_; ++i) {
    const auto port = udpPort ? udpPort + i : 0;
    if (port > 65535) {
      break;
    }
    try {
      auto socket = std::make_unique<asio::ip::udp::socket>(
          engine_.io(), asio::ip::udp::endpoint(asio::ip::udp::v4(),
                                                static_cast<uint16_t>(port)));
      udpPort_ = socket->local_endpoint().port();
      udp_ = std::move(socket);
    }
    catch (const std::exception& error) {
      log::debug("ED2K: UDP port {} unavailable: {}", port, error.what());
    }
  }
  if (!udp_) {
    log::warn("ED2K: cannot listen on a UDP port; Kad is unavailable");
  }
  if (acceptor_) {
    asio::co_spawn(engine_.io(), acceptLoop(), asio::detached);
  }
  if (udp_) {
    asio::co_spawn(engine_.io(), receiveLoop(), asio::detached);
  }
}

void Network::close(std::function<void()> done)
{
  if (closed_) {
    done();
    return;
  }
  closed_ = true;
  ticker_.cancel();
  boost::system::error_code ignored;
  if (acceptor_) {
    acceptor_->close(ignored);
  }
  if (udp_) {
    udp_->close(ignored);
  }
  server_->stop();
  for (const auto& [key, client] : std::map(clients_)) {
    client->disconnect("shutting down");
  }
  kad_->save();
  store_->saveCredits(uploads_->credits());
  if (portMapper_) {
    portMapper_->close(std::move(done));
  }
  else {
    done();
  }
}

void Network::globalOptionsChanged()
{
  const auto& global = engine_.global();
  uploadLimit_.setRate(global.integer("max-overall-upload-limit"));
  downloadLimit_.setRate(global.integer("max-overall-download-limit"));
  uploads_->setSlots(static_cast<int>(global.integer("ed2k-upload-slots")));
  if (acceptor_ || udp_) {
    updatePortMapping();
  }
}

void Network::updatePortMapping()
{
  const bool wanted = engine_.global().flag("ed2k-port-mapping");
  if (closed_ || wanted == static_cast<bool>(portMapper_)) {
    return;
  }
  if (!wanted) {
    // Removing the mappings takes a round trip; the mapper lives until then.
    std::shared_ptr<net::PortMapper> mapper(std::move(portMapper_));
    mapper->close([mapper] {});
    return;
  }
  portMapper_ =
      std::make_unique<net::PortMapper>(engine_.io(), "aria2-next ED2K");
  if (acceptor_) {
    portMapper_->add(net::PortMapper::Protocol::Tcp, tcpPort_);
  }
  if (udp_) {
    portMapper_->add(net::PortMapper::Protocol::Udp, udpPort_);
  }
}

uint32_t Network::clientId() const
{
  if (const auto id = server_->clientId()) {
    return id;
  }
  return publicAddress_ ? idOf(*publicAddress_) : 0;
}

Endpoint Network::serverEndpoint() const
{
  if (auto current = server_->current()) {
    return *current;
  }
  return Endpoint{"0.0.0.0", 0};
}

std::optional<asio::ip::address_v4> Network::publicAddress() const
{
  return publicAddress_;
}

void Network::observedAddress(const asio::ip::address_v4& address)
{
  if (address.is_unspecified() || address.is_loopback()) {
    return;
  }
  if (publicAddress_ != address) {
    log::info("ED2K: our public address is {}", address.to_string());
    publicAddress_ = address;
  }
}

// --- files ------------------------------------------------------------------

void Network::attach(Ed2kTask& task)
{
  files_[task.hash()] = &task;
  server_->filesChanged();
}

void Network::detach(Ed2kTask& task)
{
  auto found = files_.find(task.hash());
  if (found != files_.end() && found->second == &task) {
    files_.erase(found);
    server_->filesChanged();
  }
  for (const auto& [key, client] : clients_) {
    if (client->downloadFile() == &task) {
      client->dropSource();
    }
    if (client->uploadFile() == &task) {
      client->leaveUploadQueue();
      client->revokeSlot();
    }
  }
}

Ed2kTask* Network::file(const std::string& hash) const
{
  const auto found = files_.find(hash);
  return found == files_.end() ? nullptr : found->second;
}

void Network::addBootstrap(const std::string& servers,
                           const std::string& serverList,
                           const std::string& nodeList)
{
  std::vector<ServerMetEntry> entries;
  for (const auto& item : text::split(servers, ',')) {
    const auto value = std::string(text::trim(item));
    if (value.empty() || !bootstrapped_.insert("server:" + value).second) {
      continue;
    }
    try {
      ServerMetEntry entry;
      entry.endpoint = parseEndpoint(value);
      entries.push_back(std::move(entry));
    }
    catch (const std::exception& error) {
      log::warn("ED2K: ignoring server {}: {}", value, error.what());
    }
  }
  if (!entries.empty()) {
    server_->addServers(entries, true);
  }
  if (!serverList.empty() &&
      bootstrapped_.insert("list:" + serverList).second) {
    if (const auto data = readFile(serverList)) {
      auto listed = parseServerMetEntries(*data);
      if (listed.empty()) {
        // A text list of HOST:PORT lines.
        for (const auto& line : text::split(*data, '\n')) {
          const auto value = std::string(text::trim(line));
          if (value.empty() || value.front() == '#') {
            continue;
          }
          try {
            ServerMetEntry entry;
            entry.endpoint = parseEndpoint(value);
            listed.push_back(std::move(entry));
          }
          catch (const std::exception&) {
          }
        }
      }
      log::info("ED2K: {} servers from {}", listed.size(), serverList);
      server_->addServers(listed, false);
    }
    else {
      log::warn("ED2K: cannot read the server list {}", serverList);
    }
  }
  if (!nodeList.empty() && bootstrapped_.insert("nodes:" + nodeList).second) {
    kad_->addNodes(nodeList);
  }
}

// --- clients ----------------------------------------------------------------

std::shared_ptr<Client> Network::client(const asio::ip::address_v4& address,
                                        uint16_t port, uint32_t clientId)
{
  const std::pair key{address.is_unspecified() ? clientId : idOf(address),
                      port};
  auto& client = clients_[key];
  if (!client) {
    client = std::make_shared<Client>(*this, address, port);
  }
  return client;
}

std::shared_ptr<Client> Network::findByHash(const std::string& userHash) const
{
  if (userHash.size() != HASH_LENGTH) {
    return nullptr;
  }
  for (const auto& [key, client] : clients_) {
    if (client->userHash() == userHash) {
      return client;
    }
  }
  return nullptr;
}

std::shared_ptr<Client>
Network::identified(const std::shared_ptr<Client>& client)
{
  for (const auto& [key, known] : clients_) {
    if (known != client && known->userHash() == client->userHash()) {
      return known;
    }
  }
  const std::pair key{client->address().is_unspecified()
                          ? client->clientId()
                          : idOf(client->address()),
                      client->port()};
  auto& slot = clients_[key];
  if (slot && slot != client) {
    // A different client now uses this address and port.
    if (!slot->userHash().empty() && slot->userHash() != client->userHash()) {
      if (slot->connected() || slot->downloadFile() ||
          slot->upload() != Client::Upload::None) {
        return client; // keep both; this one stays unregistered
      }
      slot = client;
      return client;
    }
    return slot;
  }
  slot = client;
  return client;
}

void Network::connectionOpened() { ++openConnections_; }

void Network::connectionClosed() { --openConnections_; }

bool Network::mayConnect() const
{
  return openConnections_ < engine_.global().integer("ed2k-max-connections");
}

awaitable<void> Network::acceptLoop()
{
  while (acceptor_ && acceptor_->is_open()) {
    boost::system::error_code error;
    auto socket = co_await acceptor_->async_accept(
        asio::redirect_error(use_awaitable, error));
    if (error) {
      if (error == asio::error::operation_aborted || closed_) {
        co_return;
      }
      continue;
    }
    if (openConnections_ >=
        engine_.global().integer("ed2k-max-connections") + 10) {
      continue; // the socket closes; the client retries later
    }
    asio::co_spawn(engine_.io(), serve(std::move(socket)), asio::detached);
  }
}

awaitable<void> Network::serve(asio::ip::tcp::socket socket)
{
  boost::system::error_code error;
  const auto remote = socket.remote_endpoint(error);
  if (error || !remote.address().is_v4()) {
    co_return;
  }
  auto connection = std::make_shared<Connection>(std::move(socket));
  connection->setIdleTimeout(40s);
  try {
    co_await connection->accept(userHash_);
  }
  catch (const std::exception& failure) {
    log::debug("ED2K: incoming connection from {} failed: {}",
               remote.address().to_string(), failure.what());
    co_return;
  }
  // Who it is shows in its hello; until then the client is known by its
  // address and source port only.
  auto client =
      std::make_shared<Client>(*this, remote.address().to_v4(), remote.port());
  client->attach(std::move(connection), false);
}

void Network::sendUdp(const asio::ip::udp::endpoint& to, std::string datagram)
{
  if (!udp_ || !udp_->is_open()) {
    return;
  }
  auto buffer = std::make_shared<std::string>(std::move(datagram));
  udp_->async_send_to(asio::buffer(*buffer), to,
                      [buffer](const boost::system::error_code&, size_t) {});
}

awaitable<void> Network::receiveLoop()
{
  std::array<char, 65536> buffer;
  while (udp_ && udp_->is_open()) {
    asio::ip::udp::endpoint from;
    boost::system::error_code error;
    const auto size = co_await udp_->async_receive_from(
        asio::buffer(buffer), from, asio::redirect_error(use_awaitable, error));
    if (error) {
      if (error == asio::error::operation_aborted || closed_) {
        co_return;
      }
      continue; // e.g. ICMP port unreachable reported on Windows
    }
    datagram(from, std::string(buffer.data(), size));
  }
}

void Network::datagram(const asio::ip::udp::endpoint& from, std::string data)
{
  if (data.size() < 2 || !from.address().is_v4()) {
    return;
  }
  switch (static_cast<uint8_t>(data[0])) {
  case KAD_PROTOCOL:
  case KAD_PACKED_PROTOCOL:
    kad_->datagram(from, data);
    return;
  case PROTO_EMULE:
    clientDatagram(from, data);
    return;
  case PROTO_EDONKEY:
    return; // server UDP answers; servers are used over TCP only
  default:
    break;
  }
  if (kad_->obfuscated(from, data)) {
    return;
  }
  std::string plain;
  if (decryptPeerUdpDatagram(plain, data, userHash_,
                             idOf(from.address().to_v4())) &&
      plain.size() >= 2 && static_cast<uint8_t>(plain[0]) == PROTO_EMULE) {
    clientDatagram(from, plain);
  }
}

void Network::clientDatagram(const asio::ip::udp::endpoint& from,
                             const std::string& data)
{
  const auto address = from.address().to_v4();
  std::shared_ptr<Client> match;
  for (const auto& [key, client] : clients_) {
    if (client->address() != address) {
      continue;
    }
    if (client->udpPort() == from.port()) {
      match = client;
      break;
    }
    if (!match) {
      match = client;
    }
  }
  if (!match) {
    return;
  }
  try {
    match->udpPacket(static_cast<uint8_t>(data[1]), data.substr(2));
  }
  catch (const std::exception& error) {
    log::debug("ED2K: bad UDP packet from {}: {}", address.to_string(),
               error.what());
  }
}

// --- periodic work ----------------------------------------------------------

void Network::scheduleTick()
{
  ticker_.expires_after(1s);
  ticker_.async_wait([this](const boost::system::error_code& error) {
    if (!error && !closed_) {
      tick();
      scheduleTick();
    }
  });
}

void Network::tick()
{
  const auto now = Clock::now();
  try {
    server_->tick(now);
    kad_->tick(now);
    uploads_->tick(now);
    searches_->tick(now);
    for (const auto& [key, client] : std::map(clients_)) {
      client->tick(now);
    }
  }
  catch (const std::exception& error) {
    log::warn("ED2K: {}", error.what());
  }
  // Forget clients with nothing left to do.
  std::erase_if(clients_, [](const auto& item) {
    const auto& client = item.second;
    return client.use_count() == 1 && !client->connected() &&
           client->download() == Client::Download::None &&
           client->upload() == Client::Upload::None;
  });
  if (now - lastCreditSave_ > creditSaveInterval) {
    lastCreditSave_ = now;
    store_->saveCredits(uploads_->credits());
  }
}

void Network::describe(boost::json::object& out) const
{
  server_->describe(out);
  kad_->describe(out);
  // JSON booleans, as clients expect (unlike aria2's string flags).
  out["searchActive"] = searches_->active();
  out["searchMoreResults"] = searches_->moreResults();
  out["searchResultCount"] = std::to_string(searches_->resultCount());
  out["uploadingPeerCount"] = std::to_string(uploads_->uploading());
  out["waitingUploadPeerCount"] = std::to_string(uploads_->waiting());
  out["peerCreditCount"] = std::to_string(uploads_->credits().size());
  out["tcpPort"] = std::to_string(tcpPort_);
  out["udpPort"] = std::to_string(udpPort_);
  out["portMapping"] = portMapper_ ? portMapper_->state() : "off";
  out["userHash"] = text::toHex(userHash_);
  out["clientId"] = std::to_string(clientId());
}

} // namespace aria2::ed2k
