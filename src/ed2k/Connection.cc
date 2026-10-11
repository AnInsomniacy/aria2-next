/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Connection.h"

#include "ed2k/Compression.h"
#include "ed2k/Constants.h"
#include "ed2k/Crypto.h"
#include "ed2k/Packet.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <openssl/bn.h>
#include <openssl/rand.h>

#include <array>

namespace aria2::ed2k {

using asio::awaitable;
using asio::use_awaitable;

namespace {

// eMule protocol obfuscation (EncryptedStreamSocket in eMule/aMule).
constexpr uint8_t magicRequester = 34;
constexpr uint8_t magicServer = 203;
constexpr uint32_t magicSync = 0x835E6FC4;
constexpr uint8_t methodObfuscation = 0;
constexpr size_t dhPrimeBytes = 96;
constexpr size_t dhExponentBytes = 16;
// Packed packets are small control messages; bound their inflated size.
constexpr size_t maxInflated = 2 << 20;
constexpr size_t maxPacket = 8 << 20;

bool isProtocolMarker(uint8_t value)
{
  return value == PROTO_EDONKEY || value == PROTO_PACKED ||
         value == PROTO_EMULE || value == KAD_PROTOCOL ||
         value == KAD_PACKED_PROTOCOL;
}

std::string randomBytes(size_t size)
{
  std::string bytes(size, '\0');
  RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()),
             static_cast<int>(size));
  return bytes;
}

uint8_t markerByte()
{
  for (;;) {
    const auto value = static_cast<uint8_t>(randomBytes(1)[0]);
    if (!isProtocolMarker(value)) {
      return value;
    }
  }
}

// <padding length><padding>, at most 15 random bytes.
std::string padding()
{
  const auto length = static_cast<uint8_t>(randomBytes(1)[0]) % 16;
  return std::string(1, static_cast<char>(length)) + randomBytes(length);
}

struct BigNum {
  BIGNUM* value = BN_new();
  ~BigNum() { BN_clear_free(value); }
};

std::string encode(const BIGNUM* value)
{
  std::string out(dhPrimeBytes, '\0');
  BN_bn2binpad(value, reinterpret_cast<unsigned char*>(out.data()),
               dhPrimeBytes);
  return out;
}

// g^e mod p with the fixed 768-bit server prime and g = 2.
std::string modExp(const BIGNUM* base, const BIGNUM* exponent)
{
  BigNum prime;
  BIGNUM* parsed = prime.value;
  BN_hex2bn(&parsed, SERVER_DH_PRIME_HEX);
  BigNum result;
  auto* context = BN_CTX_new();
  BN_mod_exp(result.value, base, exponent, prime.value, context);
  BN_CTX_free(context);
  return encode(result.value);
}

} // namespace

Connection::Connection(tcp::socket socket)
    : socket_(std::move(socket)),
      drained_(socket_.get_executor()),
      deadline_(socket_.get_executor())
{
  boost::system::error_code ignored;
  remote_ = socket_.remote_endpoint(ignored);
  socket_.set_option(tcp::no_delay(true), ignored);
}

Connection::~Connection() { close(); }

void Connection::keyStreams(const std::string& sendKey,
                            const std::string& receiveKey)
{
  std::array<unsigned char, 1024> discard{};
  send_ = std::make_unique<Rc4>();
  send_->init(reinterpret_cast<const unsigned char*>(sendKey.data()),
              sendKey.size());
  send_->encrypt(discard.size(), discard.data(), discard.data());
  receive_ = std::make_unique<Rc4>();
  receive_->init(reinterpret_cast<const unsigned char*>(receiveKey.data()),
                 receiveKey.size());
  receive_->encrypt(discard.size(), discard.data(), discard.data());
}

awaitable<void> Connection::obfuscateToClient(const std::string& userHash)
{
  const auto random = randomBytes(4);
  const auto keyPart = readUInt32(random.data());
  keyStreams(createTcpObfuscationKey(userHash, magicRequester, keyPart),
             createTcpObfuscationKey(userHash, magicServer, keyPart));
  std::string hidden = packUInt32(magicSync);
  hidden.push_back(static_cast<char>(methodObfuscation)); // supported
  hidden.push_back(static_cast<char>(methodObfuscation)); // preferred
  hidden += padding();
  send_->apply(hidden);
  co_await writeAll(std::string(1, static_cast<char>(markerByte())) + random +
                    hidden);

  std::array<char, 6> answer{};
  co_await readExactly(answer.data(), answer.size());
  if (readUInt32(answer.data()) != magicSync ||
      static_cast<uint8_t>(answer[4]) != methodObfuscation) {
    throw ProtocolError("Obfuscation handshake failed");
  }
  std::string skipped(static_cast<uint8_t>(answer[5]), '\0');
  co_await readExactly(skipped.data(), skipped.size());
}

awaitable<void> Connection::obfuscateToServer()
{
  BigNum exponent;
  const auto secret = randomBytes(dhExponentBytes);
  BN_bin2bn(reinterpret_cast<const unsigned char*>(secret.data()),
            static_cast<int>(secret.size()), exponent.value);
  BigNum generator;
  BN_set_word(generator.value, 2);
  const auto ours = modExp(generator.value, exponent.value);
  co_await writeAll(std::string(1, static_cast<char>(markerByte())) + ours +
                    padding());

  std::string theirs(dhPrimeBytes, '\0');
  co_await readExactly(theirs.data(), theirs.size(), false);
  BigNum remote;
  BN_bin2bn(reinterpret_cast<const unsigned char*>(theirs.data()),
            static_cast<int>(theirs.size()), remote.value);
  const auto shared = modExp(remote.value, exponent.value);
  keyStreams(createServerTcpObfuscationKey(shared, magicRequester),
             createServerTcpObfuscationKey(shared, magicServer));

  std::array<char, 7> answer{}; // magic, supported, preferred, padding length
  co_await readExactly(answer.data(), answer.size());
  if (readUInt32(answer.data()) != magicSync) {
    throw ProtocolError("Server obfuscation handshake failed");
  }
  std::string skipped(static_cast<uint8_t>(answer[6]), '\0');
  co_await readExactly(skipped.data(), skipped.size());

  std::string response = packUInt32(magicSync);
  response.push_back(static_cast<char>(methodObfuscation));
  response += padding();
  send_->apply(response);
  co_await writeAll(std::move(response));
}

awaitable<void> Connection::accept(const std::string& ownUserHash)
{
  char marker = 0;
  co_await readExactly(&marker, 1, false);
  if (isProtocolMarker(static_cast<uint8_t>(marker))) {
    pending_ = marker;
    co_return;
  }
  std::array<char, 4> random{};
  co_await readExactly(random.data(), random.size(), false);
  const auto keyPart = readUInt32(random.data());
  keyStreams(createTcpObfuscationKey(ownUserHash, magicServer, keyPart),
             createTcpObfuscationKey(ownUserHash, magicRequester, keyPart));
  std::array<char, 7> request{}; // magic, supported, preferred, padding length
  co_await readExactly(request.data(), request.size());
  if (readUInt32(request.data()) != magicSync) {
    throw ProtocolError("Incoming obfuscation handshake failed");
  }
  std::string skipped(static_cast<uint8_t>(request[6]), '\0');
  co_await readExactly(skipped.data(), skipped.size());

  std::string response = packUInt32(magicSync);
  response.push_back(static_cast<char>(methodObfuscation));
  response += padding();
  send_->apply(response);
  co_await writeAll(std::move(response));
}

awaitable<Packet> Connection::read()
{
  std::array<char, 6> header{};
  if (pending_) {
    header[0] = *std::exchange(pending_, std::nullopt);
    co_await readExactly(header.data() + 1, header.size() - 1);
  }
  else {
    co_await readExactly(header.data(), header.size());
  }
  PacketHeader parsed;
  if (!readPacketHeader(parsed, header.data(), header.size()) ||
      (parsed.protocol != PROTO_EDONKEY && parsed.protocol != PROTO_PACKED &&
       parsed.protocol != PROTO_EMULE)) {
    throw ProtocolError("Bad packet header");
  }
  if (parsed.payloadSize() > maxPacket) {
    throw ProtocolError("Packet too large");
  }
  Packet packet{parsed.protocol, parsed.opcode,
                std::string(parsed.payloadSize(), '\0')};
  co_await readExactly(packet.payload.data(), packet.payload.size());
  if (packet.protocol == PROTO_PACKED) {
    std::string inflated;
    if (!inflatePackedPacketPayload(inflated, packet.payload, maxInflated)) {
      throw ProtocolError("Bad packed packet");
    }
    packet.payload = std::move(inflated);
    // Servers pack eDonkey messages, clients pack eMule extensions; their
    // opcodes do not collide on either kind of connection.
    packet.protocol = PROTO_EMULE;
  }
  co_return packet;
}

void Connection::send(uint8_t protocol, uint8_t opcode,
                      std::string_view payload)
{
  if (!socket_.is_open()) {
    return;
  }
  const auto begin = outbox_.size();
  outbox_.push_back(static_cast<char>(protocol));
  outbox_ += packUInt32(static_cast<uint32_t>(payload.size() + 1));
  outbox_.push_back(static_cast<char>(opcode));
  outbox_.append(payload);
  if (send_) {
    auto* bytes = reinterpret_cast<unsigned char*>(outbox_.data() + begin);
    send_->encrypt(outbox_.size() - begin, bytes, bytes);
  }
  startWriter();
}

void Connection::startWriter()
{
  if (writing_) {
    return;
  }
  writing_ = true;
  asio::co_spawn(
      socket_.get_executor(),
      [self = shared_from_this()]() { return self->writer(); }, asio::detached);
}

awaitable<void> Connection::writer()
{
  while (!outbox_.empty() && socket_.is_open()) {
    // send() may append while the batch is on the wire.
    inflight_ = std::exchange(outbox_, {});
    boost::system::error_code error;
    co_await asio::async_write(socket_, asio::buffer(inflight_),
                               asio::redirect_error(use_awaitable, error));
    if (error) {
      close();
      break;
    }
    sent_ += static_cast<int64_t>(inflight_.size());
    inflight_.clear();
    touch();
    drained_.cancel();
  }
  writing_ = false;
  drained_.cancel();
}

awaitable<void> Connection::drain(size_t bytes)
{
  while (queued() > bytes && socket_.is_open()) {
    drained_.expires_at(Clock::time_point::max());
    boost::system::error_code ignored;
    co_await drained_.async_wait(asio::redirect_error(use_awaitable, ignored));
  }
  if (!socket_.is_open()) {
    throw boost::system::system_error(asio::error::operation_aborted);
  }
}

awaitable<void> Connection::writeAll(std::string data)
{
  co_await asio::async_write(socket_, asio::buffer(data), use_awaitable);
  sent_ += static_cast<int64_t>(data.size());
  touch();
}

awaitable<void> Connection::readExactly(char* data, size_t size, bool decrypt)
{
  if (!watching_) {
    watching_ = true;
    asio::co_spawn(
        socket_.get_executor(),
        [self = shared_from_this()]() { return self->watchdog(); },
        asio::detached);
  }
  if (size == 0) {
    co_return;
  }
  co_await asio::async_read(socket_, asio::buffer(data, size), use_awaitable);
  received_ += static_cast<int64_t>(size);
  touch();
  if (decrypt && receive_) {
    auto* bytes = reinterpret_cast<unsigned char*>(data);
    receive_->encrypt(size, bytes, bytes);
  }
}

void Connection::setIdleTimeout(std::chrono::seconds timeout)
{
  idleTimeout_ = timeout;
  touch();
}

void Connection::touch() { lastActivity_ = Clock::now(); }

// Closes the socket after idleTimeout_ without traffic in either direction,
// which fails any pending read or write.
awaitable<void> Connection::watchdog()
{
  while (socket_.is_open()) {
    const auto expiry = lastActivity_ + idleTimeout_;
    if (Clock::now() >= expiry) {
      close();
      break;
    }
    deadline_.expires_at(expiry);
    boost::system::error_code ignored;
    co_await deadline_.async_wait(asio::redirect_error(use_awaitable, ignored));
  }
}

void Connection::close()
{
  boost::system::error_code ignored;
  if (socket_.is_open()) {
    socket_.shutdown(tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
  }
  deadline_.cancel();
  drained_.cancel();
}

} // namespace aria2::ed2k
