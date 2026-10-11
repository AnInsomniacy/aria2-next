/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/ServerSession.h"

#include "base/Log.h"
#include "core/Engine.h"
#include "ed2k/Client.h"
#include "ed2k/Connection.h"
#include "ed2k/Constants.h"
#include "ed2k/Ed2kTask.h"
#include "ed2k/Endpoint.h"
#include "ed2k/Hash.h"
#include "ed2k/Network.h"
#include "ed2k/Packet.h"
#include "ed2k/Searches.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <limits>

namespace aria2::ed2k {

using asio::awaitable;
using asio::use_awaitable;
using namespace std::chrono_literals;

namespace {

constexpr auto connectTimeout = 15s;
constexpr size_t maxDials = 3; // one more each tick
constexpr auto loginTimeout = 40s;
// eMule asks a server for the sources of each file every 20 minutes.
constexpr auto sourceInterval = 20min;
constexpr auto offerInterval = 10min;
constexpr size_t maxOffered = 200;
constexpr uint8_t tagName = 0x01;
constexpr uint8_t tagSize = 0x02;
constexpr uint8_t tagSizeHigh = 0x3a;

std::string key(const Endpoint& endpoint)
{
  return endpoint.host + ':' + std::to_string(endpoint.port);
}

} // namespace

ServerSession::ServerSession(Network& network) : network_(network) {}

ServerSession::~ServerSession() { stop(); }

void ServerSession::addServers(const std::vector<ServerMetEntry>& servers,
                               bool configured)
{
  for (const auto& entry : servers) {
    if (entry.endpoint.host.empty() || entry.endpoint.port == 0) {
      continue;
    }
    auto found = std::find_if(
        servers_.begin(), servers_.end(), [&entry](const Server& server) {
          return key(server.entry.endpoint) == key(entry.endpoint);
        });
    if (found != servers_.end()) {
      found->configured = found->configured || configured;
      if (!entry.name.empty()) {
        found->entry = entry;
      }
      continue;
    }
    // Configured servers are tried first.
    Server server{entry, configured};
    if (configured) {
      const auto firstLearned =
          std::find_if(servers_.begin(), servers_.end(),
                       [](const Server& s) { return !s.configured; });
      servers_.insert(firstLearned, server);
    }
    else {
      servers_.push_back(server);
    }
  }
}

ServerSession::Server* ServerSession::find(const std::string& serverKey)
{
  auto found = std::find_if(servers_.begin(), servers_.end(),
                            [&serverKey](const Server& server) {
                              return key(server.entry.endpoint) == serverKey;
                            });
  return found == servers_.end() ? nullptr : &*found;
}

const ServerSession::Server*
ServerSession::find(const std::string& serverKey) const
{
  return const_cast<ServerSession*>(this)->find(serverKey);
}

std::optional<Endpoint> ServerSession::current() const
{
  const auto* server = loggedIn_ ? find(current_) : nullptr;
  if (!server) {
    return std::nullopt;
  }
  return server->entry.endpoint;
}

void ServerSession::stop()
{
  stopped_ = true;
  for (const auto& [serverKey, socket] : std::exchange(dials_, {})) {
    boost::system::error_code ignored;
    socket->close(ignored);
  }
  if (connection_) {
    connection_->close();
  }
}

void ServerSession::tick(Clock::time_point now)
{
  if (loggedIn_) {
    if (offerDue_ || now - lastOffer_ > offerInterval) {
      offerFiles();
    }
    requestSources(now);
    return;
  }
  // Connected while there is something to download, share or search.
  if (stopped_ || connection_ || dials_.size() >= maxDials ||
      servers_.empty() ||
      (network_.files().empty() && !network_.searches().active())) {
    return;
  }
  for (size_t tried = 0; tried < servers_.size(); ++tried) {
    const auto index = (next_ + tried) % servers_.size();
    auto serverKey = key(servers_[index].entry.endpoint);
    if (now < servers_[index].retryAt || dials_.contains(serverKey)) {
      continue;
    }
    next_ = index + 1;
    dials_[serverKey] = std::make_shared<tcp::socket>(network_.engine().io());
    asio::co_spawn(network_.engine().io(), run(std::move(serverKey)),
                   asio::detached);
    return;
  }
}

void ServerSession::failed(const std::string& serverKey, const std::string& why)
{
  auto* found = find(serverKey);
  if (!found) {
    return;
  }
  auto& server = *found;
  ++server.failures;
  server.retryAt =
      Clock::now() + std::min<Clock::duration>(
                         30s * (1 << std::min(server.failures, 6)), 30min);
  log::info("ED2K: server {} ({}): {}", key(server.entry.endpoint),
            server.entry.name, why);
}

awaitable<void> ServerSession::run(std::string serverKey)
{
  const auto socket = dials_.at(serverKey);
  const auto server = *find(serverKey);
  const auto& endpoint = server.entry.endpoint;
  const bool obfuscate =
      server.entry.tcpObfuscationPort != 0 && !server.obfuscationFailed;
  std::shared_ptr<Connection> connection;
  std::string reason = "connection closed";
  bool won = false;
  try {
    boost::system::error_code error;
    auto address = asio::ip::make_address(endpoint.host, error);
    asio::steady_timer timer(network_.engine().io(), connectTimeout);
    timer.async_wait([socket](const boost::system::error_code& error) {
      if (!error) {
        boost::system::error_code ignored;
        socket->close(ignored);
      }
    });
    const auto port =
        obfuscate ? server.entry.tcpObfuscationPort : endpoint.port;
    if (error) {
      tcp::resolver resolver(network_.engine().io());
      const auto results = co_await resolver.async_resolve(
          tcp::v4(), endpoint.host, std::to_string(port), use_awaitable);
      co_await asio::async_connect(*socket, results, use_awaitable);
    }
    else {
      co_await socket->async_connect(tcp::endpoint(address, port),
                                     use_awaitable);
    }
    timer.cancel();
    connection = std::make_shared<Connection>(std::move(*socket));
    connection->setIdleTimeout(
        std::chrono::duration_cast<std::chrono::seconds>(loginTimeout));
    if (obfuscate) {
      try {
        co_await connection->obfuscateToServer();
      }
      catch (...) {
        if (auto* found = find(serverKey)) {
          found->obfuscationFailed = true;
        }
        throw;
      }
    }
    dials_.erase(serverKey);
    if (connection_) {
      connection
          ->close(); // another server answered first; this one stays eligible
      co_return;
    }
    won = true;
    connection_ = connection;
    current_ = serverKey;
    name_.clear();
    message_.clear();
    // The others are not needed any more.
    for (const auto& [otherKey, other] : std::exchange(dials_, {})) {
      boost::system::error_code ignored;
      other->close(ignored);
    }
    send(OP_LOGINREQUEST,
         createLoginRequestPayload(network_.userHash(), 0, network_.tcpPort(),
                                   "aria2-next"));
    log::debug("ED2K: logging in to server {}", serverKey);
    while (connection->open()) {
      handle(co_await connection->read());
    }
  }
  catch (const std::exception& error) {
    reason = error.what();
  }
  if (!won) {
    // A dial cancelled because another server answered first did not fail.
    if (dials_.erase(serverKey) != 0 && !stopped_) {
      failed(serverKey, reason);
    }
    co_return;
  }
  const bool wasLoggedIn = loggedIn_;
  if (connection_ == connection) {
    connection_.reset();
  }
  loggedIn_ = false;
  if (stopped_) {
    co_return;
  }
  if (wasLoggedIn) {
    log::notice("ED2K: disconnected from server {} ({})", serverKey, reason);
    if (auto* found = find(serverKey)) {
      found->retryAt = Clock::now() + 30s;
    }
  }
  else {
    failed(serverKey, reason);
  }
}

void ServerSession::send(uint8_t opcode, const std::string& payload)
{
  if (connection_) {
    connection_->send(PROTO_EDONKEY, opcode, payload);
  }
}

void ServerSession::handle(const Packet& packet)
{
  const auto& payload = packet.payload;
  switch (packet.opcode) {
  case OP_IDCHANGE: {
    ServerIdChange change;
    if (!parseServerIdChangePayload(change, payload)) {
      throw ProtocolError("Bad ID change");
    }
    loggedIn(change.clientId, change.tcpFlags);
    return;
  }
  case OP_SERVERMESSAGE: {
    std::string message;
    if (parseServerMessagePayload(message, payload)) {
      message_ = message;
      log::info("ED2K: server message: {}", message);
    }
    return;
  }
  case OP_SERVERSTATUS: {
    ServerStatus status;
    if (parseServerStatusPayload(status, payload)) {
      users_ = status.users;
      files_ = status.files;
    }
    return;
  }
  case OP_SERVERIDENT: {
    ServerIdent ident;
    if (parseServerIdentPayload(ident, payload)) {
      name_ = ident.name;
      if (auto* server = find(current_)) {
        server->entry.name = ident.name;
        server->entry.description = ident.description;
      }
    }
    return;
  }
  case OP_SERVERLIST: {
    std::vector<Endpoint> learned;
    if (parseServerListPayload(learned, payload)) {
      std::vector<ServerMetEntry> entries;
      for (const auto& endpoint : learned) {
        ServerMetEntry entry;
        entry.endpoint = endpoint;
        entries.push_back(entry);
      }
      addServers(entries, false);
    }
    return;
  }
  case OP_FOUNDSOURCES:
  case OP_FOUNDSOURCES_OBFU: {
    if (payload.size() < HASH_LENGTH) {
      return;
    }
    auto* task = network_.file(payload.substr(0, HASH_LENGTH));
    std::vector<FoundSource> sources;
    if (!task ||
        !parseFoundSourcesPayload(sources, payload, task->hash(),
                                  packet.opcode == OP_FOUNDSOURCES_OBFU)) {
      return;
    }
    const auto server = current().value_or(Endpoint{});
    for (const auto& source : sources) {
      task->addSource(source.endpoint, source.clientId, server, Origin::Server);
    }
    log::debug("ED2K: server sent {} sources for {}", sources.size(),
               task->name());
    return;
  }
  case OP_CALLBACKREQUESTED: {
    // A low ID client asks us, through the server, to connect to it.
    Endpoint endpoint;
    if (!parseCallbackRequestIncomingPayload(endpoint, payload)) {
      return;
    }
    boost::system::error_code error;
    const auto address = asio::ip::make_address_v4(endpoint.host, error);
    if (error) {
      return;
    }
    auto client = network_.client(address, endpoint.port);
    client->learn(endpoint, 0, {});
    client->callBack();
    return;
  }
  case OP_CALLBACK_FAIL:
    return;
  case OP_SEARCHRESULT: {
    SearchResult result;
    if (parseSearchResultPayload(result, payload, "server")) {
      network_.searches().serverResults(result);
    }
    return;
  }
  case OP_REJECT:
    log::info("ED2K: the server rejected a request");
    return;
  default:
    return;
  }
}

void ServerSession::loggedIn(uint32_t clientId, uint32_t flags)
{
  const bool first = !loggedIn_;
  loggedIn_ = true;
  clientId_ = clientId;
  flags_ = flags;
  if (connection_) {
    connection_->setIdleTimeout(std::chrono::seconds(std::chrono::hours(2)));
  }
  if (auto* server = find(current_)) {
    server->failures = 0;
  }
  if (clientId >= 0x1000000) {
    const auto bytes = std::array<unsigned char, 4>{
        static_cast<unsigned char>(clientId),
        static_cast<unsigned char>(clientId >> 8),
        static_cast<unsigned char>(clientId >> 16),
        static_cast<unsigned char>(clientId >> 24)};
    network_.observedAddress(asio::ip::address_v4(bytes));
  }
  if (first) {
    log::notice("ED2K: connected to server {} with {} ID", current_,
                clientId < 0x1000000 ? "a low" : "a high");
    asked_.clear();
    offerFiles();
  }
}

void ServerSession::offerFiles()
{
  offerDue_ = false;
  lastOffer_ = Clock::now();
  const bool compression = (flags_ & SRV_TCPFLG_COMPRESSION) != 0;
  std::string entries;
  uint32_t count = 0;
  for (const auto& [hash, task] : network_.files()) {
    if (count >= maxOffered || !task->sharesAnything()) {
      continue;
    }
    entries += hash;
    if (compression) {
      // Markers instead of our address: complete or partial.
      const bool complete = !task->downloading();
      entries += packUInt32(complete ? 0xfbfbfbfb : 0xfcfcfcfc);
      entries += packUInt16(complete ? 0xfbfb : 0xfcfc);
    }
    else {
      entries += packUInt32(network_.clientId());
      entries += packUInt16(network_.tcpPort());
    }
    const auto size = static_cast<uint64_t>(task->size());
    const bool large = size > std::numeric_limits<uint32_t>::max();
    entries += packUInt32(large ? 3 : 2);
    entries += createStringTag(tagName, task->name());
    entries += createUInt32Tag(tagSize, static_cast<uint32_t>(size));
    if (large) {
      entries +=
          createUInt32Tag(tagSizeHigh, static_cast<uint32_t>(size >> 32));
    }
    ++count;
  }
  send(OP_OFFERFILES, packUInt32(count) + entries);
}

void ServerSession::requestSources(Clock::time_point now)
{
  // A couple of files a second keeps within the servers' flood limits.
  int budget = 2;
  for (const auto& [hash, task] : network_.files()) {
    if (budget == 0) {
      return;
    }
    if (!task->downloading()) {
      continue;
    }
    auto& last = asked_[hash];
    if (last != Clock::time_point{} && now - last < sourceInterval) {
      continue;
    }
    last = now;
    send(OP_GETSOURCES, createGetSourcesPayload(hash, task->size()));
    --budget;
  }
}

bool ServerSession::requestCallback(uint32_t clientId,
                                    const Endpoint& sourceServer)
{
  const auto server = current();
  if (!server || lowId()) {
    return false; // two low ID clients cannot reach each other
  }
  if (!sourceServer.host.empty() && sourceServer.port &&
      (sourceServer.host != server->host ||
       sourceServer.port != server->port)) {
    return false; // the server only reaches its own clients
  }
  send(OP_CALLBACKREQUEST, createCallbackRequestPayload(clientId));
  return true;
}

bool ServerSession::search(const SearchQuery& query)
{
  if (!loggedIn_) {
    return false;
  }
  send(OP_SEARCHREQUEST, createSearchRequestPayload(
                             query, (flags_ & SRV_TCPFLG_LARGEFILES) != 0));
  return true;
}

bool ServerSession::moreResults()
{
  if (!loggedIn_) {
    return false;
  }
  send(OP_QUERY_MORE_RESULT, {});
  return true;
}

void ServerSession::describe(boost::json::object& out) const
{
  out["serverCount"] = std::to_string(servers_.size());
  out["connectedServerCount"] = loggedIn_ ? "1" : "0";
  if (const auto server = current()) {
    out["server"] = key(*server);
    out["serverName"] = name_.empty() ? find(current_)->entry.name : name_;
    out["lowId"] = lowId();
    out["serverUsers"] = std::to_string(users_);
    out["serverFiles"] = std::to_string(files_);
  }
}

} // namespace aria2::ed2k
