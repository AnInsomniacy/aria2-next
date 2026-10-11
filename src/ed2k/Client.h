/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "ed2k/Compression.h"
#include "ed2k/Connection.h"
#include "ed2k/Link.h"
#include "ed2k/Parts.h"
#include "ed2k/PeerProtocol.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/address_v4.hpp>

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aria2::ed2k {

class Network;
class Ed2kTask;

// Where a source came from; it decides which sources are dropped first.
enum class Origin { Link, Server, Kad, Exchange, Incoming, Stored };

// Another eDonkey client, as in aMule's CUpDownClient: one object per remote
// client, with at most one TCP connection, a download side (a file we want
// from it) and an upload side (a file it wants from us). Clients are found by
// user hash or by address, so an incoming connection from a source we are
// queued at continues where the outgoing one left off.
class Client : public std::enable_shared_from_this<Client> {
public:
  enum class Download {
    None,          // not a source of anything we want
    Idle,          // a source, not connected
    Connecting,    // connecting, or waiting for a callback
    Asking,        // file request sent
    Queued,        // waiting in its upload queue
    Downloading,   // in an upload slot of it
    NoNeededParts, // has nothing we lack (yet)
    NoFile,        // does not share the file
  };
  enum class Upload { None, Waiting, Uploading };

  Client(Network& network, asio::ip::address_v4 address, uint16_t port);
  ~Client();

  const asio::ip::address_v4& address() const { return address_; }
  uint16_t port() const { return port_; }
  uint16_t udpPort() const { return udpPort_; }
  const std::string& userHash() const { return userHash_; }
  uint32_t clientId() const { return clientId_; }
  bool lowId() const;
  bool connected() const { return connection_ && connection_->open(); }
  bool handshaked() const { return handshaked_; }
  std::string describe() const;

  // Source details learned from a link, a server, Kad or another client.
  void learn(const Endpoint& endpoint, uint32_t clientId,
             const Endpoint& server);

  // Download side.
  Download download() const { return download_; }
  Ed2kTask* downloadFile() const { return downloadFile_; }
  Origin origin() const { return origin_; }
  void setSource(Ed2kTask& task, Origin origin);
  void dropSource();
  const std::vector<bool>& partStatus() const { return partStatus_; }
  uint16_t queueRank() const { return queueRank_; }
  Clock::time_point nextAttempt() const { return nextAttempt_; }
  int failures() const { return failures_; }
  int64_t downloadSpeed();
  // Connects (or asks for a callback) and asks for the file.
  void askForFile();
  // The file changed: verified parts (shared) or completion.
  void fileUpdated();

  // Upload side.
  Upload upload() const { return upload_; }
  Ed2kTask* uploadFile() const { return uploadFile_; }
  void grantSlot();
  // A low ID client asked through our server to be connected to.
  void callBack();
  // A Kad node asked us to test whether its TCP port is reachable.
  void checkFirewall();
  void revokeSlot();
  void leaveUploadQueue();
  int64_t uploadSpeed();
  uint64_t sessionUploaded() const { return sessionUploaded_; }

  // A connection, ours or accepted, carries this client from here on.
  void attach(std::shared_ptr<Connection> connection, bool outgoing);
  // UDP from this client (eMule extended protocol).
  void udpPacket(uint8_t opcode, const std::string& payload);
  // Called once a second by the network.
  void tick(Clock::time_point now);
  void disconnect(const std::string& why);

private:
  asio::awaitable<void> connectAndRun();
  asio::awaitable<void> run(std::shared_ptr<Connection> connection,
                            bool outgoing);
  void closed(const std::shared_ptr<Connection>& connection,
              const std::string& reason);
  asio::awaitable<void> uploadLoop(std::shared_ptr<Connection> connection);
  void handle(const Packet& packet);
  void handleEdonkey(uint8_t opcode, const std::string& payload);
  void handleEmule(uint8_t opcode, const std::string& payload);
  void send(uint8_t protocol, uint8_t opcode, std::string_view payload);

  void sendHello(bool answer);
  void readHello(const std::string& payload, bool hello);
  void handshakeDone();
  void requestFile();
  void fileAnswered(const std::string& name);
  void statusAnswered(std::vector<bool> parts, bool complete);
  void requestUpload();
  void requestBlocks();
  void receiveBlock(int64_t begin, int64_t end, const char* data, size_t size);
  void receiveCompressed(int64_t begin, uint32_t total,
                         const std::string& data);
  void blockDone(const Range& block);
  void queued(uint16_t rank);
  void stopDownloading(Download next, std::chrono::seconds retry);
  void releaseBlocks();

  // Answers to requests for files we share.
  Ed2kTask* sharedFile(const std::string& hash);
  void answerFileName(const std::string& payload);
  void answerFileStatus(const std::string& payload);
  void answerMultipacket(const std::string& payload, bool extended);
  void answerHashSet(const std::string& payload);
  void answerUploadRequest(const std::string& payload);
  void answerPartRequest(const std::string& payload, bool large);
  void answerSources(const std::string& payload, bool version2);
  std::string sourcesFor(Ed2kTask& task, uint8_t version, bool version2);

  Network& network_;
  asio::ip::address_v4 address_;
  uint16_t port_;
  std::string userHash_;
  uint32_t clientId_ = 0;
  Endpoint server_;
  uint16_t cryptOptions_ = 0;
  uint16_t udpPort_ = 0;
  EmulePeerInfo info_;
  std::shared_ptr<Connection> connection_;
  bool handshaked_ = false;
  bool outgoing_ = false;
  // The known client that took this connection over when its hello showed
  // who called; packets read here go to it.
  std::shared_ptr<Client> adopter_;
  bool connecting_ = false;
  bool firewallCheck_ = false; // confirm the connection once handshaked

  Download download_ = Download::None;
  Ed2kTask* downloadFile_ = nullptr;
  Origin origin_ = Origin::Link;
  std::vector<bool> partStatus_;
  bool completeSource_ = false;
  bool askedSources_ = false;
  bool askedHashSet_ = false;
  uint16_t queueRank_ = 0;
  std::deque<Range> blocks_;            // requested, in the order sent
  std::map<int64_t, int64_t> progress_; // block begin -> bytes received
  std::map<int64_t, std::unique_ptr<CompressedPartInflater>> inflaters_;
  Clock::time_point nextAttempt_{};
  Clock::time_point lastData_{};
  Clock::time_point lastAsked_{};
  Clock::time_point reaskSent_{};
  int failures_ = 0;
  SpeedMeter downloadSpeed_;

  Upload upload_ = Upload::None;
  Ed2kTask* uploadFile_ = nullptr;
  std::deque<Range> uploadRequests_;
  bool uploading_ = false;   // the upload coroutine runs
  bool slotPending_ = false; // tell it about the slot once connected
  uint64_t sessionUploaded_ = 0;
  SpeedMeter uploadSpeed_;
};

std::string_view downloadStateName(Client::Download state);

} // namespace aria2::ed2k
