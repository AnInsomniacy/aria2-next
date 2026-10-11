/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/steady_timer.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aria2::net {

// Where to send SOAP requests for a gateway's WAN connection service.
struct UpnpControl {
  std::string host;
  std::string port;
  std::string path;
  std::string service; // the service type, which names the actions
};

// Finds the WANIPConnection (else WANPPPConnection) service in a device
// description read from `location`.
std::optional<UpnpControl> findWanConnection(const std::string& description,
                                             const std::string& location);

// Keeps ports forwarded on the local Internet gateway with UPnP IGD: finds
// the gateway, adds the mappings with a lease it renews, and removes them
// when closed. Without a usable gateway the ports stay unmapped and state()
// says why.
class PortMapper {
public:
  enum class Protocol { Tcp, Udp };

  PortMapper(boost::asio::io_context& io, std::string description);
  ~PortMapper();
  PortMapper(const PortMapper&) = delete;
  PortMapper& operator=(const PortMapper&) = delete;

  void add(Protocol protocol, uint16_t port);
  // Removes the mappings, then calls `done`; gives up after a few seconds.
  void close(std::function<void()> done);

  // "discovering", "mapped", "no gateway", "double NAT", or an error.
  const std::string& state() const { return state_; }
  std::optional<boost::asio::ip::address_v4> externalAddress() const
  {
    return external_;
  }

private:
  struct Mapping {
    Protocol protocol;
    uint16_t port;
    bool mapped = false;
  };
  struct Gateway {
    UpnpControl control;
    std::string localAddress; // ours, on the way to the gateway
  };

  boost::asio::awaitable<void> run();
  boost::asio::awaitable<std::vector<std::string>> discover();
  boost::asio::awaitable<std::optional<Gateway>>
  describe(const std::string& location);
  boost::asio::awaitable<std::string> soap(const std::string& action,
                                           const std::string& arguments,
                                           unsigned& status);
  boost::asio::awaitable<void> map(Mapping& mapping);
  boost::asio::awaitable<void> unmapAll();

  boost::asio::io_context& io_;
  std::string description_;
  std::vector<Mapping> mappings_;
  std::optional<Gateway> gateway_;
  std::optional<boost::asio::ip::address_v4> external_;
  std::string state_ = "discovering";
  boost::asio::steady_timer wake_;
  bool permanent_ = false; // the gateway only grants leases without expiry
  bool running_ = false;
  bool closed_ = false;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace aria2::net
