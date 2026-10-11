/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Client.h"

#include "base/Log.h"
#include "base/Text.h"
#include "core/Engine.h"
#include "ed2k/Constants.h"
#include "ed2k/Ed2kTask.h"
#include "ed2k/Endpoint.h"
#include "ed2k/Hash.h"
#include "ed2k/Kad.h"
#include "ed2k/Network.h"
#include "ed2k/Packet.h"
#include "ed2k/ServerSession.h"
#include "ed2k/SharedFile.h"
#include "ed2k/UploadQueue.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <limits>

namespace aria2::ed2k {

using asio::awaitable;
using asio::use_awaitable;
using namespace std::chrono_literals;

namespace {

constexpr auto connectTimeout = 20s;
constexpr auto handshakeTimeout = 40s;
// A slot that delivers nothing for this long is given up.
constexpr auto transferTimeout = 100s;
// eMule re-asks a queued source every 29 minutes (FILEREASKTIME).
constexpr auto reaskInterval = 29min;
constexpr auto udpReaskTimeout = 30s;
// Blocks requested ahead from one source.
constexpr size_t blocksAhead = 3;
// Data packets of an upload, as eMule sends them.
constexpr size_t chunkSize = 10240;
constexpr uint32_t lowIdLimit = 0x1000000;

uint32_t idOf(const asio::ip::address_v4& address)
{
  const auto bytes = address.to_bytes();
  return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

std::string nameAnswer(const Ed2kTask& task)
{
  const auto& name = task.name();
  const auto size =
      std::min<size_t>(name.size(), std::numeric_limits<uint16_t>::max());
  return task.hash() + packUInt16(static_cast<uint16_t>(size)) +
         name.substr(0, size);
}

// <count:2><bits>, as written after OP_REQUESTFILENAME by clients that know
// our extended request version.
void skipPartStatus(const std::string& payload, size_t& offset)
{
  std::vector<bool> ignored;
  if (!parsePartStatusPayload(ignored, payload, offset)) {
    throw ProtocolError("Bad part status");
  }
}

} // namespace

std::string_view downloadStateName(Client::Download state)
{
  switch (state) {
  case Client::Download::None:
    return "none";
  case Client::Download::Idle:
    return "idle";
  case Client::Download::Connecting:
    return "connecting";
  case Client::Download::Asking:
    return "asking";
  case Client::Download::Queued:
    return "queued";
  case Client::Download::Downloading:
    return "downloading";
  case Client::Download::NoNeededParts:
    return "no-needed-parts";
  case Client::Download::NoFile:
    return "no-file";
  }
  return "none";
}

Client::Client(Network& network, asio::ip::address_v4 address, uint16_t port)
    : network_(network), address_(address), port_(port)
{
  if (!address.is_unspecified()) {
    clientId_ = idOf(address);
  }
}

Client::~Client() = default;

bool Client::lowId() const { return clientId_ != 0 && clientId_ < lowIdLimit; }

std::string Client::describe() const
{
  if (address_.is_unspecified()) {
    return fmt::format("low ID {}", clientId_);
  }
  return fmt::format("{}:{}", address_.to_string(), port_);
}

void Client::learn(const Endpoint& endpoint, uint32_t clientId,
                   const Endpoint& server)
{
  if (userHash_.empty() && endpoint.userHash.size() == HASH_LENGTH) {
    userHash_ = endpoint.userHash;
  }
  if (endpoint.cryptOptions) {
    cryptOptions_ = endpoint.cryptOptions;
  }
  if (clientId && address_.is_unspecified()) {
    clientId_ = clientId;
  }
  if (!server.host.empty() && server.port) {
    server_ = server;
  }
}

// --- download side --------------------------------------------------------

void Client::setSource(Ed2kTask& task, Origin origin)
{
  if (downloadFile_ == &task) {
    return;
  }
  if (downloadFile_) {
    // One file per source at a time, as in eMule; the other task keeps the
    // source and gets it back when this one lets go.
    return;
  }
  downloadFile_ = &task;
  origin_ = origin;
  download_ = Download::Idle;
  nextAttempt_ = {};
  failures_ = 0;
  partStatus_.clear();
  completeSource_ = false;
  askedSources_ = false;
}

void Client::dropSource()
{
  if (!downloadFile_) {
    return;
  }
  if (download_ == Download::Downloading && connected()) {
    send(PROTO_EDONKEY, OP_CANCELTRANSFER, {});
  }
  releaseBlocks();
  downloadFile_ = nullptr;
  download_ = Download::None;
  queueRank_ = 0;
  if (upload_ == Upload::None && connected() && !connecting_) {
    disconnect("source no longer needed");
  }
}

int64_t Client::downloadSpeed() { return downloadSpeed_.rate(); }

int64_t Client::uploadSpeed() { return uploadSpeed_.rate(); }

void Client::askForFile()
{
  if (!downloadFile_ || !downloadFile_->downloading()) {
    return;
  }
  lastAsked_ = Clock::now();
  if (connected()) {
    if (handshaked_) {
      requestFile();
    }
    return;
  }
  if (connecting_) {
    return;
  }
  if (lowId()) {
    // A low ID client cannot be reached; our server asks it to call us.
    if (network_.server().requestCallback(clientId_, server_)) {
      download_ = Download::Connecting;
      connecting_ = true;
      nextAttempt_ = Clock::now() + 45s;
      log::debug("ED2K: asked the server for a callback from {}", describe());
    }
    else {
      stopDownloading(Download::Idle, 10min);
    }
    return;
  }
  if (!network_.mayConnect()) {
    nextAttempt_ = Clock::now() + 5s;
    return;
  }
  download_ = Download::Connecting;
  connecting_ = true;
  asio::co_spawn(
      network_.engine().io(),
      [self = shared_from_this()]() { return self->connectAndRun(); },
      asio::detached);
}

awaitable<void> Client::connectAndRun()
{
  auto self = shared_from_this();
  std::shared_ptr<Connection> connection;
  // Obfuscated whenever the client supports it and we know its key, as we
  // request obfuscation like eMule does; a client that failed obfuscation
  // once is reached in plain next time.
  const bool obfuscate =
      userHash_.size() == HASH_LENGTH &&
      (cryptOptions_ & (SOURCE_CRYPT_SUPPORT | SOURCE_CRYPT_REQUIRE));
  network_.connectionOpened();
  try {
    tcp::socket socket(network_.engine().io());
    asio::steady_timer timer(network_.engine().io(), connectTimeout);
    timer.async_wait([&socket](const boost::system::error_code& error) {
      if (!error) {
        boost::system::error_code ignored;
        socket.close(ignored);
      }
    });
    co_await socket.async_connect(tcp::endpoint(address_, port_),
                                  use_awaitable);
    timer.cancel();
    connection = std::make_shared<Connection>(std::move(socket));
    connection->setIdleTimeout(
        std::chrono::duration_cast<std::chrono::seconds>(handshakeTimeout));
    if (obfuscate) {
      co_await connection->obfuscateToClient(userHash_);
    }
  }
  catch (const std::exception& error) {
    network_.connectionClosed();
    connecting_ = false;
    log::debug("ED2K: cannot connect to {}: {}", describe(), error.what());
    if (obfuscate && !(cryptOptions_ & SOURCE_CRYPT_REQUIRE)) {
      cryptOptions_ = 0;
    }
    if (download_ == Download::Connecting) {
      ++failures_;
      stopDownloading(Download::Idle, std::chrono::seconds(std::min(
                                          60 << std::min(failures_, 5), 1800)));
    }
    if (slotPending_) {
      slotPending_ = false;
      network_.uploads().remove(*this);
    }
    co_return;
  }
  network_.connectionClosed(); // run() counts it again
  connecting_ = false;
  co_await run(std::move(connection), true);
}

void Client::attach(std::shared_ptr<Connection> connection, bool outgoing)
{
  if (connected() && connection_ != connection) {
    connection_->close();
  }
  connecting_ = false;
  asio::co_spawn(
      network_.engine().io(),
      [self = shared_from_this(), connection, outgoing]() {
        return self->run(connection, outgoing);
      },
      asio::detached);
}

awaitable<void> Client::run(std::shared_ptr<Connection> connection,
                            bool outgoing)
{
  auto self = shared_from_this();
  connection_ = connection;
  outgoing_ = outgoing;
  handshaked_ = false;
  network_.connectionOpened();
  std::string reason = "closed";
  try {
    if (outgoing) {
      sendHello(false);
    }
    while (connection->open()) {
      const auto packet = co_await connection->read();
      (adopter_ ? *adopter_ : *this).handle(packet);
    }
  }
  catch (const std::exception& error) {
    reason = error.what();
  }
  network_.connectionClosed();
  (adopter_ ? *adopter_ : *this).closed(connection, reason);
}

void Client::closed(const std::shared_ptr<Connection>& connection,
                    const std::string& reason)
{
  if (connection_ != connection) {
    return; // replaced by a newer connection
  }
  connection_.reset();
  handshaked_ = false;
  uploading_ = false;
  log::debug("ED2K: {} disconnected ({})", describe(), reason);
  inflaters_.clear();
  releaseBlocks();
  switch (download_) {
  case Download::Connecting:
  case Download::Asking:
    ++failures_;
    stopDownloading(Download::Idle, std::chrono::seconds(std::min(
                                        60 << std::min(failures_, 5), 1800)));
    break;
  case Download::Downloading:
    // The slot is gone with the connection; eMule queues us again.
    stopDownloading(Download::Idle, 30s);
    break;
  default:
    break;
  }
  if (upload_ == Upload::Uploading) {
    network_.uploads().remove(*this);
  }
}

void Client::disconnect(const std::string& why)
{
  if (connection_) {
    log::debug("ED2K: closing connection to {}: {}", describe(), why);
    connection_->close();
  }
}

void Client::send(uint8_t protocol, uint8_t opcode, std::string_view payload)
{
  if (connection_) {
    connection_->send(protocol, opcode, payload);
  }
}

void Client::sendHello(bool answer)
{
  auto info = createLocalEmulePeerInfo();
  info.udpPort = network_.udpPort();
  const auto payload = createPeerHelloPayload(
      network_.userHash(), network_.clientId(), network_.tcpPort(),
      network_.serverEndpoint(), "aria2-next", info, !answer);
  send(PROTO_EDONKEY, answer ? OP_HELLOANSWER : OP_HELLO, payload);
}

void Client::readHello(const std::string& payload, bool hello)
{
  EmulePeerInfo info;
  if (!parsePeerHelloPayload(info, payload, hello)) {
    throw ProtocolError("Bad hello");
  }
  size_t offset = (hello ? 1 : 0) + HASH_LENGTH;
  const auto id = readUInt32(payload.data() + offset);
  const auto port = readUInt16(payload.data() + offset + 4);
  const auto serverOffset = payload.size() - 6;
  Endpoint server;
  server.host = ipv4FromEndpoint(readUInt32(payload.data() + serverOffset));
  server.port = readUInt16(payload.data() + serverOffset + 4);
  if (server.port && server.host != "0.0.0.0") {
    server_ = server;
  }
  userHash_ = info.userHash;
  info_ = info;
  udpPort_ = info.udpPort;
  if (info.miscOptions2.supportsCryptLayer) {
    cryptOptions_ = static_cast<uint16_t>(
        SOURCE_CRYPT_SUPPORT |
        (info.miscOptions2.requestsCryptLayer ? SOURCE_CRYPT_REQUEST : 0) |
        (info.miscOptions2.requiresCryptLayer ? SOURCE_CRYPT_REQUIRE : 0));
  }
  if (!outgoing_ && port) {
    port_ = port; // the listen port, not the ephemeral source port
  }
  if (id >= lowIdLimit && !address_.is_unspecified()) {
    clientId_ = idOf(address_);
  }
  else {
    clientId_ = id;
  }
}

void Client::handle(const Packet& packet)
{
  if (packet.protocol == PROTO_EDONKEY) {
    handleEdonkey(packet.opcode, packet.payload);
  }
  else {
    handleEmule(packet.opcode, packet.payload);
  }
}

void Client::handleEdonkey(uint8_t opcode, const std::string& payload)
{
  switch (opcode) {
  case OP_HELLO: {
    readHello(payload, true);
    // The same client may be known already, e.g. a source we are queued
    // at that now calls us; that object continues with this connection.
    auto self = network_.identified(shared_from_this());
    if (self.get() != this) {
      auto connection = std::exchange(connection_, nullptr);
      if (const auto old = self->connection_; old && old != connection) {
        self->closed(old, "replaced by a newer connection");
        old->close();
      }
      self->connection_ = connection;
      self->outgoing_ = false;
      self->info_ = info_;
      self->udpPort_ = udpPort_;
      self->cryptOptions_ = cryptOptions_;
      self->connecting_ = false;
      self->sendHello(true);
      self->handshakeDone();
      // This coroutine keeps reading, for the other object.
      adopter_ = std::move(self);
      return;
    }
    sendHello(true);
    handshakeDone();
    return;
  }
  case OP_HELLOANSWER:
    readHello(payload, false);
    network_.identified(shared_from_this());
    handshakeDone();
    return;
  case OP_REQUESTFILENAME:
    return answerFileName(payload);
  case OP_SETREQFILEID:
    return answerFileStatus(payload);
  case OP_HASHSETREQUEST:
    return answerHashSet(payload);
  case OP_STARTUPLOADREQ:
    return answerUploadRequest(payload);
  case OP_REQUESTPARTS:
    return answerPartRequest(payload, false);
  case OP_CANCELTRANSFER:
    log::debug("ED2K: {} cancelled its download", describe());
    leaveUploadQueue();
    return;
  case OP_REQFILENAMEANSWER: {
    if (!downloadFile_ || payload.size() < HASH_LENGTH + 2 ||
        payload.compare(0, HASH_LENGTH, downloadFile_->hash()) != 0) {
      return;
    }
    const auto length = readUInt16(payload.data() + HASH_LENGTH);
    fileAnswered(payload.substr(HASH_LENGTH + 2, length));
    return;
  }
  case OP_FILESTATUS: {
    if (!downloadFile_) {
      return;
    }
    std::vector<bool> parts;
    if (!parseFileStatusPayload(parts, payload, downloadFile_->hash())) {
      throw ProtocolError("Bad file status");
    }
    const bool complete = parts.empty();
    statusAnswered(std::move(parts), complete);
    return;
  }
  case OP_FILEREQANSNOFIL:
    if (downloadFile_ && payload.size() >= HASH_LENGTH &&
        payload.compare(0, HASH_LENGTH, downloadFile_->hash()) == 0) {
      log::debug("ED2K: {} does not have the file", describe());
      stopDownloading(Download::NoFile, 1h);
    }
    return;
  case OP_HASHSETANSWER: {
    if (!downloadFile_) {
      return;
    }
    std::vector<std::string> hashes;
    if (!parseHashSetAnswerPayload(hashes, payload, downloadFile_->hash()) ||
        !downloadFile_->acceptHashSet(std::move(hashes))) {
      throw ProtocolError("Bad hash set");
    }
    requestUpload();
    return;
  }
  case OP_ACCEPTUPLOADREQ:
    if (!downloadFile_ || !downloadFile_->downloading()) {
      send(PROTO_EDONKEY, OP_CANCELTRANSFER, {});
      return;
    }
    log::debug("ED2K: {} granted an upload slot", describe());
    download_ = Download::Downloading;
    queueRank_ = 0;
    lastData_ = Clock::now();
    connection_->setIdleTimeout(
        std::chrono::duration_cast<std::chrono::seconds>(transferTimeout));
    requestBlocks();
    return;
  case OP_QUEUERANK: {
    uint16_t rank = 0;
    if (parseQueueRankPayload(rank, payload)) {
      queued(rank);
    }
    return;
  }
  case OP_OUTOFPARTREQS:
    if (download_ == Download::Downloading) {
      log::debug("ED2K: {} ended our upload slot", describe());
      releaseBlocks();
      download_ = Download::Queued;
      nextAttempt_ = Clock::now() + reaskInterval;
      downloadFile_->sourceChanged();
    }
    return;
  case OP_SENDINGPART: {
    if (payload.size() < HASH_LENGTH + 8) {
      throw ProtocolError("Bad part");
    }
    const int64_t begin = readUInt32(payload.data() + HASH_LENGTH);
    const int64_t end = readUInt32(payload.data() + HASH_LENGTH + 4);
    receiveBlock(begin, end, payload.data() + HASH_LENGTH + 8,
                 payload.size() - HASH_LENGTH - 8);
    return;
  }
  default:
    return;
  }
}

void Client::handleEmule(uint8_t opcode, const std::string& payload)
{
  switch (opcode) {
  case OP_EMULEINFO: {
    EmulePeerInfo info;
    if (parseEmuleInfoPayload(info, payload)) {
      info_.miscOptions = info.miscOptions;
      udpPort_ = info.udpPort ? info.udpPort : udpPort_;
    }
    auto ours = createLocalEmulePeerInfo();
    ours.udpPort = network_.udpPort();
    send(PROTO_EMULE, OP_EMULEINFOANSWER, createEmuleInfoPayload(ours));
    return;
  }
  case OP_EMULEINFOANSWER:
    return;
  case OP_MULTIPACKET:
    return answerMultipacket(payload, false);
  case OP_MULTIPACKET_EXT:
    return answerMultipacket(payload, true);
  case OP_MULTIPACKETANSWER: {
    if (!downloadFile_) {
      return;
    }
    MultipacketAnswer answer;
    if (!parseMultipacketAnswerPayload(answer, payload,
                                       downloadFile_->hash())) {
      throw ProtocolError("Bad multipacket answer");
    }
    if (answer.hasFileName) {
      fileAnswered(answer.fileName);
    }
    if (answer.hasFileStatus) {
      statusAnswered(std::move(answer.partStatus), answer.completeSource);
    }
    else if (downloadFile_ && downloadFile_->size() <= PIECE_LENGTH) {
      statusAnswered({}, true); // single-part files have no status
    }
    return;
  }
  case OP_QUEUERANKING:
    if (payload.size() >= 2) {
      queued(readUInt16(payload.data()));
    }
    return;
  case OP_REQUESTPARTS_I64:
    return answerPartRequest(payload, true);
  case OP_SENDINGPART_I64: {
    if (payload.size() < HASH_LENGTH + 16) {
      throw ProtocolError("Bad part");
    }
    const auto begin =
        static_cast<int64_t>(readUInt64(payload.data() + HASH_LENGTH));
    const auto end =
        static_cast<int64_t>(readUInt64(payload.data() + HASH_LENGTH + 8));
    receiveBlock(begin, end, payload.data() + HASH_LENGTH + 16,
                 payload.size() - HASH_LENGTH - 16);
    return;
  }
  case OP_COMPRESSEDPART: {
    if (payload.size() < HASH_LENGTH + 8) {
      throw ProtocolError("Bad compressed part");
    }
    receiveCompressed(readUInt32(payload.data() + HASH_LENGTH),
                      readUInt32(payload.data() + HASH_LENGTH + 4),
                      payload.substr(HASH_LENGTH + 8));
    return;
  }
  case OP_COMPRESSEDPART_I64: {
    if (payload.size() < HASH_LENGTH + 12) {
      throw ProtocolError("Bad compressed part");
    }
    receiveCompressed(
        static_cast<int64_t>(readUInt64(payload.data() + HASH_LENGTH)),
        readUInt32(payload.data() + HASH_LENGTH + 8),
        payload.substr(HASH_LENGTH + 12));
    return;
  }
  case OP_REQUESTSOURCES:
    return answerSources(payload, false);
  case OP_REQUESTSOURCES2:
    return answerSources(payload, true);
  case OP_ANSWERSOURCES:
  case OP_ANSWERSOURCES2: {
    if (!downloadFile_) {
      return;
    }
    SourceExchangeAnswer answer;
    const bool parsed =
        opcode == OP_ANSWERSOURCES2
            ? parseAnswerSources2Payload(answer, payload, downloadFile_->hash())
            : parseAnswerSourcesPayload(
                  answer, payload, downloadFile_->hash(),
                  info_.miscOptions.sourceExchange1Version);
    if (!parsed) {
      return;
    }
    for (const auto& entry : answer.entries) {
      auto endpoint = entry.endpoint;
      endpoint.userHash = entry.userHash;
      endpoint.cryptOptions = entry.cryptOptions;
      downloadFile_->addSource(endpoint, 0, entry.server, Origin::Exchange);
    }
    log::debug("ED2K: {} sent {} sources", describe(), answer.entries.size());
    return;
  }
  case OP_AICHFILEHASHREQ:
  case OP_AICHREQUEST:
  case OP_AICHFILEHASHANS:
  case OP_AICHANSWER:
    return; // AICH recovery is not used; corrupt parts are downloaded again
  case OP_KAD_FWTCPCHECK_ACK:
    network_.kad().tcpReachable(address_);
    return;
  default:
    return;
  }
}

void Client::handshakeDone()
{
  handshaked_ = true;
  if (connection_) {
    connection_->setIdleTimeout(
        std::chrono::duration_cast<std::chrono::seconds>(handshakeTimeout));
  }
  if (std::exchange(firewallCheck_, false)) {
    send(PROTO_EMULE, OP_KAD_FWTCPCHECK_ACK, {});
  }
  if (slotPending_) {
    slotPending_ = false;
    grantSlot();
  }
  if (downloadFile_ && downloadFile_->downloading() &&
      (download_ == Download::Connecting || download_ == Download::Idle ||
       (download_ == Download::Queued && outgoing_))) {
    requestFile();
  }
}

void Client::requestFile()
{
  auto& file = *downloadFile_;
  download_ = Download::Asking;
  askedHashSet_ = false;
  lastAsked_ = Clock::now();
  const auto ours = file.sharedParts();
  const bool wantSources = !askedSources_ && file.sources().size() < 200;
  if (info_.miscOptions.multiPacket) {
    auto request = info_;
    if (!wantSources) {
      request.miscOptions2.supportsSourceExchange2 = false;
      request.miscOptions.sourceExchange1Version = 0;
    }
    request.miscOptions.aichVersion = 0;
    const bool extended = info_.miscOptions2.supportsExtendedMultipacket;
    send(PROTO_EMULE, extended ? OP_MULTIPACKET_EXT : OP_MULTIPACKET,
         createMultipacketFileRequestPayload(file.hash(), file.size(), ours,
                                             request, extended));
  }
  else {
    std::string name = file.hash();
    if (info_.miscOptions.extendedRequestsVersion > 0) {
      name = createFileStatusPayload(file.hash(), ours);
      if (info_.miscOptions.extendedRequestsVersion > 1) {
        name += packUInt16(0);
      }
    }
    send(PROTO_EDONKEY, OP_REQUESTFILENAME, name);
    send(PROTO_EDONKEY, OP_SETREQFILEID, file.hash());
    if (wantSources) {
      const auto sources = createRequestSourcesPayload(file.hash(), info_);
      if (sources.opcode) {
        send(PROTO_EMULE, sources.opcode, sources.payload);
      }
    }
  }
  askedSources_ = askedSources_ || wantSources;
}

void Client::fileAnswered(const std::string&)
{
  if (download_ != Download::Asking) {
    return;
  }
  // Single-part files have no status; the name answer is all there is.
  if (downloadFile_->size() <= PIECE_LENGTH && !info_.miscOptions.multiPacket) {
    statusAnswered({}, true);
  }
}

void Client::statusAnswered(std::vector<bool> parts, bool complete)
{
  if (download_ != Download::Asking && download_ != Download::Queued) {
    return;
  }
  auto& file = *downloadFile_;
  completeSource_ = complete;
  partStatus_ = complete ? std::vector<bool>() : std::move(parts);
  if (!complete && partStatus_.size() != file.parts().count()) {
    throw ProtocolError("File status of the wrong size");
  }
  if (!file.wants(partStatus_)) {
    log::debug("ED2K: {} has no part we need", describe());
    stopDownloading(Download::NoNeededParts, 10min);
    return;
  }
  if (file.needsHashSet()) {
    if (!askedHashSet_) {
      askedHashSet_ = true;
      send(PROTO_EDONKEY, OP_HASHSETREQUEST, file.hash());
    }
    return;
  }
  requestUpload();
}

void Client::requestUpload()
{
  if (download_ != Download::Asking && download_ != Download::Queued) {
    return;
  }
  send(PROTO_EDONKEY, OP_STARTUPLOADREQ, downloadFile_->hash());
}

void Client::queued(uint16_t rank)
{
  if (!downloadFile_ || download_ == Download::Downloading) {
    return;
  }
  log::debug("ED2K: queued at {} with rank {}", describe(), rank);
  download_ = Download::Queued;
  queueRank_ = rank;
  failures_ = 0;
  nextAttempt_ = Clock::now() + reaskInterval;
  // eMule drops the connection of a queued client; it calls back when a
  // slot opens, or we ask again later.
  if (upload_ == Upload::None) {
    disconnect("queued");
  }
}

void Client::requestBlocks()
{
  if (download_ != Download::Downloading || !downloadFile_) {
    return;
  }
  auto& file = *downloadFile_;
  if (blocks_.size() >= blocksAhead) {
    return;
  }
  const auto claimed =
      file.claim(*this, partStatus_, blocksAhead - blocks_.size());
  if (claimed.empty()) {
    if (blocks_.empty()) {
      log::debug("ED2K: nothing left to request from {}", describe());
      send(PROTO_EDONKEY, OP_CANCELTRANSFER, {});
      stopDownloading(file.wants(partStatus_) ? Download::Idle
                                              : Download::NoNeededParts,
                      1min);
    }
    return;
  }
  std::vector<PartRange> ranges;
  for (const auto& [begin, end] : claimed) {
    blocks_.push_back({begin, end});
    progress_[begin] = 0;
    ranges.push_back({begin, end});
  }
  const bool large = file.size() > std::numeric_limits<uint32_t>::max();
  send(large ? PROTO_EMULE : PROTO_EDONKEY,
       large ? OP_REQUESTPARTS_I64 : OP_REQUESTPARTS,
       createRequestPartsPayload(file.hash(), ranges, large));
}

void Client::receiveBlock(int64_t begin, int64_t end, const char* data,
                          size_t size)
{
  if (!downloadFile_ || download_ != Download::Downloading) {
    return; // late data after a cancel
  }
  if (end <= begin || static_cast<size_t>(end - begin) != size) {
    throw ProtocolError("Bad part range");
  }
  if (!downloadFile_->claimed(*this, begin, end)) {
    return; // a block released after a timeout or the endgame
  }
  lastData_ = Clock::now();
  downloadSpeed_.add(static_cast<int64_t>(size));
  network_.uploads().downloaded(userHash_, size);
  downloadFile_->receive(begin, std::string(data, size));
  for (const auto& block : blocks_) {
    if (block.first <= begin && end <= block.second) {
      auto& got = progress_[block.first];
      got += end - begin;
      if (got >= block.second - block.first) {
        const auto done = block;
        blockDone(done);
      }
      break;
    }
  }
}

void Client::receiveCompressed(int64_t begin, uint32_t, const std::string& data)
{
  if (!downloadFile_ || download_ != Download::Downloading) {
    return;
  }
  // A compressed block is one zlib stream sent in several packets, each
  // naming the block start.
  auto block =
      std::find_if(blocks_.begin(), blocks_.end(), [begin](const Range& range) {
        return range.first <= begin && begin < range.second;
      });
  if (block == blocks_.end()) {
    return;
  }
  auto& inflater = inflaters_[block->first];
  if (!inflater) {
    inflater = std::make_unique<CompressedPartInflater>();
  }
  const auto offset = block->first + inflater->inflatedLength();
  std::string plain;
  if (!inflater->inflateChunk(plain, data, block->first,
                              static_cast<size_t>(block->second - offset))) {
    throw ProtocolError("Bad compressed data");
  }
  if (!plain.empty()) {
    const auto copy = *block;
    if (!inflater->active()) {
      inflaters_.erase(copy.first);
    }
    receiveBlock(offset, offset + static_cast<int64_t>(plain.size()),
                 plain.data(), plain.size());
  }
}

void Client::blockDone(const Range& block)
{
  blocks_.erase(std::remove(blocks_.begin(), blocks_.end(), block),
                blocks_.end());
  progress_.erase(block.first);
  inflaters_.erase(block.first);
  if (downloadFile_) {
    downloadFile_->release(*this, block);
  }
  requestBlocks();
}

void Client::releaseBlocks()
{
  if (downloadFile_) {
    downloadFile_->releaseAll(*this);
  }
  blocks_.clear();
  progress_.clear();
}

void Client::stopDownloading(Download next, std::chrono::seconds retry)
{
  releaseBlocks();
  if (!downloadFile_) {
    download_ = Download::None;
    return;
  }
  download_ = next;
  connecting_ = connecting_ && connected();
  nextAttempt_ = Clock::now() + retry;
  downloadFile_->sourceChanged();
  if (connected() && upload_ == Upload::None && next != Download::Downloading) {
    disconnect(std::string(downloadStateName(next)));
  }
}

void Client::fileUpdated()
{
  if (!downloadFile_) {
    return;
  }
  if (!downloadFile_->downloading()) {
    // Complete: this client is no source anymore, but it may still be
    // downloading from us.
    if (download_ == Download::Downloading && connected()) {
      send(PROTO_EDONKEY, OP_CANCELTRANSFER, {});
    }
    releaseBlocks();
    download_ = Download::Idle;
  }
}

void Client::tick(Clock::time_point now)
{
  if (download_ == Download::Downloading && now - lastData_ > transferTimeout) {
    log::debug("ED2K: {} stopped sending", describe());
    send(PROTO_EDONKEY, OP_CANCELTRANSFER, {});
    stopDownloading(Download::Idle, 1min);
  }
  if (download_ == Download::Connecting && connecting_ && lowId() &&
      now > nextAttempt_) {
    connecting_ = false;
    stopDownloading(Download::Idle, 20min); // no callback came
  }
  // A queued source is asked again by UDP when it supports it, else by TCP.
  if (download_ == Download::Queued && !connected() && now >= nextAttempt_ &&
      downloadFile_ && downloadFile_->downloading()) {
    if (udpPort_ && info_.miscOptions.udpVersion >= 3 &&
        !address_.is_unspecified() && reaskSent_ == Clock::time_point{}) {
      reaskSent_ = now;
      const auto ours = downloadFile_->sharedParts();
      network_.sendUdp(asio::ip::udp::endpoint(address_, udpPort_),
                       createDatagram(PROTO_EMULE, OP_REASKFILEPING,
                                      info_.miscOptions.udpVersion >= 4
                                          ? createUdpReaskFilePingPayload(
                                                downloadFile_->hash(), ours, 0)
                                          : createUdpReaskFilePingPayload(
                                                downloadFile_->hash())));
    }
    else if (reaskSent_ == Clock::time_point{} ||
             now - reaskSent_ > udpReaskTimeout) {
      reaskSent_ = {};
      askForFile();
    }
  }
}

void Client::udpPacket(uint8_t opcode, const std::string& payload)
{
  switch (opcode) {
  case OP_REASKFILEPING: {
    // A client waiting in our queue asks for its rank.
    UdpReask reask;
    if (!parseUdpReaskFilePingPayload(reask, payload)) {
      return;
    }
    auto* file = network_.file(reask.fileHash);
    const asio::ip::udp::endpoint to(address_, udpPort_);
    if (!file || !file->sharesAnything()) {
      network_.sendUdp(to, createDatagram(PROTO_EMULE, OP_FILENOTFOUND, {}));
      return;
    }
    const auto rank = network_.uploads().rank(*this);
    if (!rank) {
      network_.sendUdp(to, createDatagram(PROTO_EMULE, OP_QUEUEFULL, {}));
      return;
    }
    network_.uploads().touched(*this);
    network_.sendUdp(to, createDatagram(PROTO_EMULE, OP_REASKACK,
                                        info_.miscOptions.udpVersion >= 4
                                            ? createUdpReaskAckPayload(
                                                  file->sharedParts(), rank)
                                            : createUdpReaskAckPayload(rank)));
    return;
  }
  case OP_REASKACK: {
    UdpReaskAck ack;
    if (download_ != Download::Queued ||
        !parseUdpReaskAckPayload(ack, payload)) {
      return;
    }
    reaskSent_ = {};
    queueRank_ = ack.rank;
    if (!ack.bitfield.empty()) {
      partStatus_ = ack.bitfield;
    }
    nextAttempt_ = Clock::now() + reaskInterval;
    return;
  }
  case OP_QUEUEFULL:
    reaskSent_ = {};
    nextAttempt_ = Clock::now() + reaskInterval;
    return;
  case OP_FILENOTFOUND:
    reaskSent_ = {};
    stopDownloading(Download::NoFile, 1h);
    return;
  default:
    return;
  }
}

// --- upload side ----------------------------------------------------------

Ed2kTask* Client::sharedFile(const std::string& hash)
{
  auto* file = network_.file(hash);
  return file && file->sharesAnything() ? file : nullptr;
}

void Client::answerFileName(const std::string& payload)
{
  if (payload.size() < HASH_LENGTH) {
    throw ProtocolError("Bad file request");
  }
  const auto hash = payload.substr(0, HASH_LENGTH);
  if (auto* file = sharedFile(hash)) {
    send(PROTO_EDONKEY, OP_REQFILENAMEANSWER, nameAnswer(*file));
  }
  else {
    send(PROTO_EDONKEY, OP_FILEREQANSNOFIL, hash);
  }
}

void Client::answerFileStatus(const std::string& payload)
{
  if (payload.size() != HASH_LENGTH) {
    throw ProtocolError("Bad status request");
  }
  if (auto* file = sharedFile(payload)) {
    send(PROTO_EDONKEY, OP_FILESTATUS,
         createFileStatusPayload(file->hash(), file->sharedParts()));
  }
  else {
    send(PROTO_EDONKEY, OP_FILEREQANSNOFIL, payload);
  }
}

void Client::answerMultipacket(const std::string& payload, bool extended)
{
  size_t offset = 0;
  const auto hash = readBytes(payload, offset, HASH_LENGTH);
  if (extended) {
    offset += 8; // file size
  }
  auto* file = sharedFile(hash);
  if (!file) {
    send(PROTO_EDONKEY, OP_FILEREQANSNOFIL, hash);
    return;
  }
  std::string answer = hash;
  std::optional<std::pair<uint8_t, bool>> sources;
  while (offset < payload.size()) {
    switch (readByte(payload, offset)) {
    case OP_REQUESTFILENAME:
      // Our advertised extended request version 2: part status and the
      // number of complete sources follow.
      skipPartStatus(payload, offset);
      readBytes(payload, offset, 2);
      answer.push_back(static_cast<char>(OP_REQFILENAMEANSWER));
      answer += nameAnswer(*file).substr(HASH_LENGTH);
      break;
    case OP_SETREQFILEID:
      answer.push_back(static_cast<char>(OP_FILESTATUS));
      answer += createFileStatusPayload(file->hash(), file->sharedParts())
                    .substr(HASH_LENGTH);
      break;
    case OP_REQUESTSOURCES:
      sources = {static_cast<uint8_t>(info_.miscOptions.sourceExchange1Version),
                 false};
      break;
    case OP_REQUESTSOURCES2: {
      const auto version = readByte(payload, offset);
      readBytes(payload, offset, 2);
      sources = {version, true};
      break;
    }
    case OP_AICHFILEHASHREQ:
      break;
    default:
      offset = payload.size(); // unknown request: answer what we have
      break;
    }
  }
  send(PROTO_EMULE, OP_MULTIPACKETANSWER, answer);
  if (sources) {
    if (auto data = sourcesFor(*file, sources->first, sources->second);
        !data.empty()) {
      send(PROTO_EMULE, sources->second ? OP_ANSWERSOURCES2 : OP_ANSWERSOURCES,
           data);
    }
  }
}

void Client::answerHashSet(const std::string& payload)
{
  if (payload.size() != HASH_LENGTH) {
    throw ProtocolError("Bad hash set request");
  }
  auto* file = sharedFile(payload);
  if (!file || file->needsHashSet()) {
    send(PROTO_EDONKEY, OP_FILEREQANSNOFIL, payload);
    return;
  }
  send(PROTO_EDONKEY, OP_HASHSETANSWER,
       createHashSetAnswerPayload(file->hash(), file->pieceHashes()));
}

void Client::answerUploadRequest(const std::string& payload)
{
  if (payload.size() < HASH_LENGTH) {
    throw ProtocolError("Bad upload request");
  }
  auto* file = sharedFile(payload.substr(0, HASH_LENGTH));
  if (!file) {
    send(PROTO_EDONKEY, OP_FILEREQANSNOFIL, payload.substr(0, HASH_LENGTH));
    return;
  }
  if (upload_ == Upload::Uploading && uploadFile_ == file) {
    send(PROTO_EDONKEY, OP_ACCEPTUPLOADREQ, {});
    return;
  }
  uploadFile_ = file;
  const auto rank = network_.uploads().request(shared_from_this());
  if (rank > 0 && upload_ != Upload::Uploading) {
    upload_ = Upload::Waiting;
    if (info_.version || info_.miscOptions.multiPacket ||
        info_.miscOptions2.kadVersion) {
      std::string ranking = packUInt16(rank);
      ranking.append(10, '\0');
      send(PROTO_EMULE, OP_QUEUERANKING, ranking);
    }
    else {
      send(PROTO_EDONKEY, OP_QUEUERANK, createQueueRankPayload(rank));
    }
  }
}

void Client::grantSlot()
{
  if (!uploadFile_) {
    network_.uploads().remove(*this);
    return;
  }
  if (!connected() || !handshaked_) {
    // Call it, then tell it after the handshake.
    slotPending_ = true;
    upload_ = Upload::Uploading;
    if (!connected() && !connecting_) {
      if (lowId() || address_.is_unspecified()) {
        slotPending_ = false;
        network_.uploads().remove(*this);
        return;
      }
      connecting_ = true;
      asio::co_spawn(
          network_.engine().io(),
          [self = shared_from_this()]() { return self->connectAndRun(); },
          asio::detached);
    }
    return;
  }
  log::debug("ED2K: upload slot for {}", describe());
  upload_ = Upload::Uploading;
  sessionUploaded_ = 0;
  send(PROTO_EDONKEY, OP_ACCEPTUPLOADREQ, {});
  connection_->setIdleTimeout(
      std::chrono::duration_cast<std::chrono::seconds>(transferTimeout));
}

void Client::callBack()
{
  if (connected() || connecting_ || address_.is_unspecified() ||
      !network_.mayConnect()) {
    return;
  }
  connecting_ = true;
  asio::co_spawn(
      network_.engine().io(),
      [self = shared_from_this()]() { return self->connectAndRun(); },
      asio::detached);
}

void Client::checkFirewall()
{
  if (connected()) {
    if (handshaked_) {
      send(PROTO_EMULE, OP_KAD_FWTCPCHECK_ACK, {});
    }
    else {
      firewallCheck_ = true;
    }
    return;
  }
  if (connecting_ || address_.is_unspecified() || !network_.mayConnect()) {
    return;
  }
  firewallCheck_ = true;
  connecting_ = true;
  asio::co_spawn(
      network_.engine().io(),
      [self = shared_from_this()]() { return self->connectAndRun(); },
      asio::detached);
}

void Client::revokeSlot()
{
  if (upload_ != Upload::Uploading) {
    return;
  }
  log::debug("ED2K: ending the upload slot of {}", describe());
  send(PROTO_EDONKEY, OP_OUTOFPARTREQS, {});
  upload_ = Upload::None;
  uploadRequests_.clear();
}

void Client::leaveUploadQueue() { network_.uploads().remove(*this); }

void Client::answerPartRequest(const std::string& payload, bool large)
{
  std::vector<PartRange> ranges;
  std::string hash;
  if (!parsePartRequestPayload(ranges, hash, payload, large)) {
    throw ProtocolError("Bad part request");
  }
  if (upload_ != Upload::Uploading || slotPending_ || !uploadFile_ ||
      hash != uploadFile_->hash()) {
    return;
  }
  for (const auto& range : ranges) {
    const Range wanted{range.begin, range.end};
    if (range.end > uploadFile_->size() ||
        range.end - range.begin > 3 * BLOCK_LENGTH) {
      throw ProtocolError("Bad part request");
    }
    if (std::find(uploadRequests_.begin(), uploadRequests_.end(), wanted) ==
        uploadRequests_.end()) {
      uploadRequests_.push_back(wanted);
    }
  }
  if (!uploading_ && connection_) {
    uploading_ = true;
    asio::co_spawn(
        network_.engine().io(),
        [self = shared_from_this(), connection = connection_]() {
          return self->uploadLoop(connection);
        },
        asio::detached);
  }
}

// Sends requested blocks in 10 KB packets, paced by the upload limit and by
// the socket, reading each block from disk as it goes.
awaitable<void> Client::uploadLoop(std::shared_ptr<Connection> connection)
{
  auto self = shared_from_this();
  asio::steady_timer timer(network_.engine().io());
  try {
    while (upload_ == Upload::Uploading && !uploadRequests_.empty() &&
           connection->open() && uploadFile_) {
      const auto [begin, end] = uploadRequests_.front();
      auto* file = uploadFile_;
      const auto shared = file->sharedParts();
      const auto first = begin / PIECE_LENGTH;
      const auto last = (end - 1) / PIECE_LENGTH;
      for (auto part = first; part <= last; ++part) {
        if (!shared.empty() && !shared[static_cast<size_t>(part)]) {
          throw ProtocolError("Requested a part we do not share");
        }
      }
      std::optional<Ed2kTask::Block> block;
      bool done = false;
      const bool compress = info_.miscOptions.dataCompressionVersion >= 1;
      file->read(
          begin, end, compress,
          [&block, &done, &timer](std::optional<Ed2kTask::Block> result) {
            block = std::move(result);
            done = true;
            timer.cancel();
          });
      while (!done) {
        timer.expires_after(1h);
        boost::system::error_code ignored;
        co_await timer.async_wait(asio::redirect_error(use_awaitable, ignored));
      }
      if (!block || uploadFile_ != file || upload_ != Upload::Uploading) {
        break;
      }
      const bool large = file->size() > std::numeric_limits<uint32_t>::max();
      // A compressed block is one zlib stream; each of its packets names the
      // block start and the compressed length.
      const bool packed = !block->compressed.empty();
      const auto& wire = packed ? block->compressed : block->data;
      size_t delivered = 0;
      for (size_t offset = 0; offset < wire.size(); offset += chunkSize) {
        const auto size = std::min(chunkSize, wire.size() - offset);
        // Pace by the global upload limit.
        auto& limit = network_.uploadLimit();
        while (!limit.unlimited() && !limit.available()) {
          timer.expires_after(50ms);
          boost::system::error_code ignored;
          co_await timer.async_wait(
              asio::redirect_error(use_awaitable, ignored));
        }
        co_await connection->drain(64 << 10);
        std::string packet = file->hash();
        if (packed) {
          packet += large ? packUInt64(static_cast<uint64_t>(begin))
                          : packUInt32(static_cast<uint32_t>(begin));
          packet += packUInt32(static_cast<uint32_t>(wire.size()));
        }
        else {
          const auto from = begin + static_cast<int64_t>(offset);
          const auto to = from + static_cast<int64_t>(size);
          packet += large ? packUInt64(static_cast<uint64_t>(from))
                          : packUInt32(static_cast<uint32_t>(from));
          packet += large ? packUInt64(static_cast<uint64_t>(to))
                          : packUInt32(static_cast<uint32_t>(to));
        }
        packet.append(wire, offset, size);
        if (packed) {
          connection->send(PROTO_EMULE,
                           large ? OP_COMPRESSEDPART_I64 : OP_COMPRESSEDPART,
                           packet);
        }
        else {
          connection->send(large ? PROTO_EMULE : PROTO_EDONKEY,
                           large ? OP_SENDINGPART_I64 : OP_SENDINGPART, packet);
        }
        // The wire is paced; ratio and credit count the content delivered.
        limit.charge(static_cast<int64_t>(size));
        uploadSpeed_.add(static_cast<int64_t>(size));
        const auto content =
            block->data.size() * (offset + size) / wire.size() - delivered;
        delivered += content;
        sessionUploaded_ += content;
        file->noteUploaded(static_cast<int64_t>(content));
        network_.uploads().uploaded(userHash_, content);
      }
      if (!uploadRequests_.empty() &&
          uploadRequests_.front() == Range{begin, end}) {
        uploadRequests_.pop_front();
      }
    }
  }
  catch (const std::exception& error) {
    log::debug("ED2K: upload to {} ended: {}", describe(), error.what());
    connection->close();
  }
  uploading_ = false;
}

void Client::answerSources(const std::string& payload, bool version2)
{
  uint8_t version = 1;
  std::string hash;
  if (version2) {
    if (payload.size() < 3 + HASH_LENGTH) {
      return;
    }
    version = static_cast<uint8_t>(payload[0]);
    hash = payload.substr(3, HASH_LENGTH);
  }
  else {
    if (payload.size() < HASH_LENGTH) {
      return;
    }
    hash = payload.substr(0, HASH_LENGTH);
    version = info_.miscOptions.sourceExchange1Version;
  }
  auto* file = network_.file(hash);
  if (!file) {
    return;
  }
  if (auto data = sourcesFor(*file, version, version2); !data.empty()) {
    send(PROTO_EMULE, version2 ? OP_ANSWERSOURCES2 : OP_ANSWERSOURCES, data);
  }
}

std::string Client::sourcesFor(Ed2kTask& task, uint8_t version, bool version2)
{
  std::vector<SourceExchangeEntry> entries;
  for (const auto& source : task.sources()) {
    if (entries.size() >= 500) {
      break;
    }
    if (source.get() == this || source->lowId() ||
        source->address().is_unspecified() || source->failures() > 0 ||
        (source->download() != Download::Queued &&
         source->download() != Download::Downloading)) {
      continue;
    }
    SourceExchangeEntry entry;
    entry.endpoint.host = source->address().to_string();
    entry.endpoint.port = source->port();
    entry.server.host = "0.0.0.0";
    entry.userHash = source->userHash().empty() ? std::string(HASH_LENGTH, '\0')
                                                : source->userHash();
    entry.cryptOptions = static_cast<uint8_t>(source->cryptOptions_ & 0xff);
    entries.push_back(std::move(entry));
  }
  if (entries.empty() || version == 0) {
    return {};
  }
  return version2 ? createAnswerSources2Payload(task.hash(), version, entries)
                  : createAnswerSourcesPayload(task.hash(), version, entries);
}

} // namespace aria2::ed2k
