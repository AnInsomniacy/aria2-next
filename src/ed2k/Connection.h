/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "ed2k/Cipher.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace aria2::ed2k {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct Packet {
  uint8_t protocol = 0;
  uint8_t opcode = 0;
  std::string payload;
};

// One TCP connection to an ED2K server or client. Packets are framed as
// <protocol><length><opcode><payload>; packed packets are inflated on read.
// With eMule protocol obfuscation every byte after the handshake passes
// through RC4.
//
// Reads and writes run concurrently: one coroutine reads while send() queues
// bytes for a writer coroutine. An idle deadline closes the connection.
class Connection : public std::enable_shared_from_this<Connection> {
public:
  explicit Connection(tcp::socket socket);
  ~Connection();
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  const tcp::endpoint& remote() const { return remote_; }
  bool open() const { return socket_.is_open(); }
  bool obfuscated() const { return static_cast<bool>(send_); }

  // Outgoing handshakes. A client is keyed by its user hash; a server by a
  // Diffie-Hellman exchange.
  asio::awaitable<void> obfuscateToClient(const std::string& userHash);
  asio::awaitable<void> obfuscateToServer();
  // Incoming connections are either plain or obfuscated towards our hash.
  asio::awaitable<void> accept(const std::string& ownUserHash);

  // The next packet; throws on errors, timeouts and malformed data.
  asio::awaitable<Packet> read();
  // Queues a packet behind those sent before.
  void send(uint8_t protocol, uint8_t opcode, std::string_view payload);
  // Waits until at most `bytes` remain queued; used to pace uploads.
  asio::awaitable<void> drain(size_t bytes = 0);
  size_t queued() const { return outbox_.size() + inflight_.size(); }

  // No traffic for this long closes the connection.
  void setIdleTimeout(std::chrono::seconds timeout);
  void close();

  int64_t received() const { return received_; }
  int64_t sent() const { return sent_; }

private:
  asio::awaitable<void> readExactly(char* data, size_t size,
                                    bool decrypt = true);
  asio::awaitable<void> writeAll(std::string data);
  void startWriter();
  asio::awaitable<void> writer();
  asio::awaitable<void> watchdog();
  void touch();
  void keyStreams(const std::string& sendKey, const std::string& receiveKey);

  tcp::socket socket_;
  tcp::endpoint remote_;
  std::unique_ptr<Rc4> send_;
  std::unique_ptr<Rc4> receive_;
  std::optional<char> pending_; // a byte read while detecting obfuscation
  std::string outbox_;
  std::string inflight_;
  bool writing_ = false;
  asio::steady_timer drained_;
  asio::steady_timer deadline_;
  std::chrono::seconds idleTimeout_{40};
  Clock::time_point lastActivity_ = Clock::now();
  bool watching_ = false;
  int64_t received_ = 0;
  int64_t sent_ = 0;
};

} // namespace aria2::ed2k
