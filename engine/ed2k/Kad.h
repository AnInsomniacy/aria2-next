/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Gid.h"
#include "core/Speed.h"
#include "ed2k/KadRouting.h"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/json/object.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace aria2::ed2k {

class Network;

// A Kademlia 2 node as in eMule: it keeps a routing table bootstrapped from
// nodes.dat, answers other nodes, finds sources of downloads, publishes our
// shared files as sources and runs keyword searches. The routing table is
// saved with the ED2K state, so later starts need no nodes.dat.
class Kad {
public:
  explicit Kad(Network& network);
  ~Kad();

  void addNodes(const std::string& nodesDatPath);
  void tick(Clock::time_point now);
  void save();
  // A plain Kad datagram (protocol byte first).
  void datagram(const boost::asio::ip::udp::endpoint& from,
                const std::string& data);
  // An obfuscated datagram that may be Kad; false when it is not ours.
  bool obfuscated(const boost::asio::ip::udp::endpoint& from,
                  const std::string& data);

  // False while the keyword cannot be looked up yet (no contacts).
  bool search(Gid id, const std::string& keyword);
  void stopSearch(Gid id);
  // A client we asked to test our TCP port reached it.
  void tcpReachable(const boost::asio::ip::address_v4& from);

  bool firewalled() const { return firewalled_; }
  size_t contacts() const { return table_.liveSize(); }
  void describe(boost::json::object& out) const;

private:
  enum class Purpose { Sources, Keyword, Publish, Refresh };
  static std::string_view lookupName(Purpose purpose);
  struct Lookup {
    Purpose purpose;
    KadTraversal traversal;
    std::string fileHash; // Sources and Publish
    uint64_t size = 0;
    Gid search = 0; // Keyword
    Clock::time_point started;
    Clock::time_point lastAnswer; // any answer; a quiet lookup widens
    size_t results = 0;
  };

  void handle(const boost::asio::ip::udp::endpoint& from, uint8_t opcode,
              const std::string& payload, uint32_t senderKey);
  void send(const KadContact& contact, uint8_t opcode,
            const std::string& payload);
  void reply(const boost::asio::ip::udp::endpoint& to, uint32_t senderKey,
             uint8_t opcode, const std::string& payload);
  void request(const KadContact& contact, uint8_t opcode,
               const std::string& payload, uint8_t answer,
               KadTransactionPurpose purpose, const std::string& target);
  void bootstrap(Clock::time_point now);
  void checkFirewall(Clock::time_point now);
  void startLookups(Clock::time_point now);
  void startLookup(Purpose purpose, const std::string& target, uint64_t size,
                   const std::string& fileHash, Gid search);
  void perform(Lookup& lookup, const std::vector<KadTraversalAction>& actions);
  void searchResult(const KadContact& contact, const KadSearchResult& result);
  void expire(Clock::time_point now);
  KadContact contactFor(const boost::asio::ip::udp::endpoint& from,
                        const std::string& id, uint16_t tcpPort,
                        uint8_t version, uint32_t senderKey) const;
  KadContact known(const boost::asio::ip::udp::endpoint& from) const;
  int64_t seconds() const;

  Network& network_;
  KadRoutingTable table_;
  std::string selfId_;
  uint32_t udpKey_ = 0;
  KadTransactionTable transactions_;
  std::map<std::string, Lookup> lookups_; // by target ID
  KadSourceIndex sources_;                // sources others published to us
  std::map<std::string, Clock::time_point> lastSourceLookup_;
  std::map<std::string, Clock::time_point> lastPublish_;
  size_t nextRouter_ = 0;
  bool firewalled_ = true;
  Clock::time_point firewallCheck_{};
  Clock::time_point reachedAt_{};
  std::set<boost::asio::ip::address_v4> firewallCheckers_;
  std::set<std::string> observed_;
  Clock::time_point lastSave_ = Clock::now();
};

} // namespace aria2::ed2k
