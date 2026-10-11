/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "net/PortMapper.h"

#include "base/Log.h"
#include "base/Text.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <set>
#include <string_view>

namespace aria2::net {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using asio::awaitable;
using asio::use_awaitable;
using asio::ip::tcp;
using asio::ip::udp;
using namespace std::chrono_literals;

namespace {

constexpr auto discoveryTime = 3s;
constexpr auto answerGrace = 300ms;
constexpr auto requestTime = 3s;
constexpr auto closeRequestTime = 1500ms;
constexpr auto renewInterval = 30min;
constexpr auto rediscoverInterval = 10min;
constexpr int leaseSeconds = 3600;

// The text of the first element named `tag`, with or without a namespace
// prefix, at or after `from`.
std::string element(const std::string& xml, std::string_view tag,
                    size_t from = 0, size_t* end = nullptr)
{
  for (auto at = xml.find(tag, from); at != std::string::npos;
       at = xml.find(tag, at + 1)) {
    if (at == 0 || (xml[at - 1] != '<' && xml[at - 1] != ':') ||
        at + tag.size() >= xml.size()) {
      continue;
    }
    const auto open = at + tag.size();
    if (xml[open] != '>' && xml[open] != ' ') {
      continue;
    }
    const auto begin = xml.find('>', open);
    const auto close = xml.find("</", begin);
    if (begin == std::string::npos || close == std::string::npos) {
      return {};
    }
    if (end) {
      *end = close;
    }
    return std::string(
        text::trim(std::string_view(xml).substr(begin + 1, close - begin - 1)));
  }
  return {};
}

struct Url {
  std::string host;
  std::string port = "80";
  std::string path = "/";
};

std::optional<Url> parseUrl(std::string_view url)
{
  constexpr std::string_view scheme = "http://";
  if (url.size() <= scheme.size() ||
      !text::iequals(url.substr(0, scheme.size()), scheme)) {
    return std::nullopt;
  }
  url.remove_prefix(scheme.size());
  Url parsed;
  const auto slash = url.find('/');
  const auto authority = url.substr(0, slash);
  if (slash != std::string_view::npos) {
    parsed.path = std::string(url.substr(slash));
  }
  const auto colon = authority.rfind(':');
  parsed.host = std::string(authority.substr(0, colon));
  if (colon != std::string_view::npos) {
    parsed.port = std::string(authority.substr(colon + 1));
  }
  if (parsed.host.empty()) {
    return std::nullopt;
  }
  return parsed;
}

bool privateOrShared(const asio::ip::address_v4& address)
{
  const auto bytes = address.to_bytes();
  return bytes[0] == 10 || (bytes[0] == 172 && (bytes[1] & 0xf0) == 16) ||
         (bytes[0] == 192 && bytes[1] == 168) ||
         (bytes[0] == 100 && (bytes[1] & 0xc0) == 64);
}

std::string_view protocolName(PortMapper::Protocol protocol)
{
  return protocol == PortMapper::Protocol::Tcp ? "TCP" : "UDP";
}

// One request on its own connection; `local` receives our address on the
// path to the server.
awaitable<std::pair<unsigned, std::string>>
exchange(asio::io_context& io, const std::string& host, const std::string& port,
         http::request<http::string_body> request,
         std::chrono::milliseconds limit, std::string* local = nullptr)
{
  tcp::resolver resolver(io);
  beast::tcp_stream stream(io);
  stream.expires_after(limit);
  const auto endpoints =
      co_await resolver.async_resolve(host, port, use_awaitable);
  co_await stream.async_connect(endpoints, use_awaitable);
  if (local) {
    *local = stream.socket().local_endpoint().address().to_string();
  }
  request.set(http::field::host, host + ':' + port);
  request.set(http::field::connection, "close");
  request.prepare_payload();
  co_await http::async_write(stream, request, use_awaitable);
  beast::flat_buffer buffer;
  http::response_parser<http::string_body> parser;
  parser.body_limit(1 << 20);
  co_await http::async_read(stream, buffer, parser, use_awaitable);
  auto response = parser.release();
  co_return std::make_pair(response.result_int(), std::move(response.body()));
}

} // namespace

std::optional<UpnpControl> findWanConnection(const std::string& xml,
                                             const std::string& location)
{
  const auto url = parseUrl(location);
  if (!url) {
    return std::nullopt;
  }
  // The connection service of the WAN device; IP before PPP.
  for (const auto* wanted : {"WANIPConnection:", "WANPPPConnection:"}) {
    size_t at = 0;
    size_t end = 0;
    for (auto type = element(xml, "serviceType", at, &end); !type.empty();
         type = element(xml, "serviceType", at, &end)) {
      at = end;
      if (type.find(wanted) == std::string::npos) {
        continue;
      }
      auto path = element(xml, "controlURL", at);
      if (path.empty()) {
        continue;
      }
      UpnpControl control;
      if (const auto absolute = parseUrl(path)) {
        control.host = absolute->host;
        control.port = absolute->port;
        path = absolute->path;
      }
      else {
        const auto base = element(xml, "URLBase");
        const auto origin = base.empty() ? url : parseUrl(base);
        control.host = origin ? origin->host : url->host;
        control.port = origin ? origin->port : url->port;
        if (path.front() != '/') {
          path.insert(0, "/");
        }
      }
      control.path = std::move(path);
      control.service = std::move(type);
      return control;
    }
  }
  return std::nullopt;
}

PortMapper::PortMapper(asio::io_context& io, std::string description)
    : io_(io), description_(std::move(description)), wake_(io)
{
}

PortMapper::~PortMapper() { *alive_ = false; }

void PortMapper::add(Protocol protocol, uint16_t port)
{
  if (closed_ || port == 0) {
    return;
  }
  mappings_.push_back({protocol, port});
  if (running_) {
    wake_.cancel(); // map it now
    return;
  }
  running_ = true;
  asio::co_spawn(
      io_,
      [this, alive = alive_]() -> awaitable<void> {
        try {
          co_await run();
        }
        catch (const std::exception& error) {
          if (*alive) {
            log::debug("Port mapping stopped: {}", error.what());
          }
        }
      },
      asio::detached);
}

void PortMapper::close(std::function<void()> done)
{
  closed_ = true;
  wake_.cancel();
  if (!gateway_ ||
      std::none_of(mappings_.begin(), mappings_.end(),
                   [](const Mapping& mapping) { return mapping.mapped; })) {
    done();
    return;
  }
  asio::co_spawn(
      io_,
      [this, alive = alive_, done = std::move(done)]() -> awaitable<void> {
        try {
          co_await unmapAll();
        }
        catch (const std::exception&) {
        }
        if (*alive) {
          done();
        }
      },
      asio::detached);
}

awaitable<void> PortMapper::run()
{
  auto alive = alive_;
  for (bool searchedBefore = false; *alive && !closed_; searchedBefore = true) {
    if (!gateway_) {
      state_ = "discovering";
      for (const auto& location : co_await discover()) {
        if (!*alive || closed_) {
          co_return;
        }
        if ((gateway_ = co_await describe(location))) {
          log::info("Port mapping: UPnP gateway at {}:{}",
                    gateway_->control.host, gateway_->control.port);
          break;
        }
      }
    }
    if (!*alive || closed_) {
      co_return;
    }
    auto wait = renewInterval;
    if (!gateway_) {
      if (!searchedBefore) {
        log::info("Port mapping: no UPnP gateway answered");
      }
      state_ = "no-gateway";
      wait = rediscoverInterval;
    }
    else {
      bool mapped = true;
      for (auto& mapping : mappings_) {
        co_await map(mapping);
        if (!*alive || closed_) {
          co_return;
        }
        mapped = mapped && mapping.mapped;
      }
      unsigned status = 0;
      const auto answer = co_await soap("GetExternalIPAddress", {}, status);
      boost::system::error_code error;
      const auto external = asio::ip::make_address_v4(
          element(answer, "NewExternalIPAddress"), error);
      if (!error && !external.is_unspecified()) {
        external_ = external;
      }
      if (!mapped) {
        state_ = "failed";
      }
      else if (external_ && privateOrShared(*external_)) {
        // The gateway is behind another NAT; its mapping does not reach us.
        state_ = "double-nat";
      }
      else {
        state_ = "mapped";
      }
      if (status == 0) {
        gateway_.reset(); // unreachable now; look for it again
        wait = 1min;
      }
    }
    wake_.expires_after(wait);
    boost::system::error_code ignored;
    co_await wake_.async_wait(asio::redirect_error(use_awaitable, ignored));
  }
}

// M-SEARCH on every IPv4 interface; returns the description URLs heard.
awaitable<std::vector<std::string>> PortMapper::discover()
{
  std::vector<std::string> locations;
  udp::socket socket(io_);
  socket.open(udp::v4());
  socket.bind(udp::endpoint(udp::v4(), 0));
  const udp::endpoint group(asio::ip::make_address_v4("239.255.255.250"), 1900);
  std::set<asio::ip::address_v4> interfaces;
  boost::system::error_code error;
  tcp::resolver resolver(io_);
  for (const auto& entry : resolver.resolve(asio::ip::host_name(), "", error)) {
    if (entry.endpoint().address().is_v4() &&
        !entry.endpoint().address().is_loopback()) {
      interfaces.insert(entry.endpoint().address().to_v4());
    }
  }
  interfaces.insert(asio::ip::address_v4::any());
  for (const auto& address : interfaces) {
    socket.set_option(asio::ip::multicast::outbound_interface(address), error);
    for (const auto* target :
         {"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
          "urn:schemas-upnp-org:service:WANIPConnection:1",
          "urn:schemas-upnp-org:service:WANPPPConnection:1"}) {
      const auto search =
          fmt::format("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n"
                      "MAN: \"ssdp:discover\"\r\nMX: 2\r\nST: {}\r\n\r\n",
                      target);
      socket.send_to(asio::buffer(search), group, 0, error);
    }
  }
  asio::steady_timer deadline(io_, discoveryTime);
  const auto closeSocket =
      [&socket](const boost::system::error_code& cancelled) {
        if (!cancelled) {
          boost::system::error_code ignored;
          socket.close(ignored);
        }
      };
  deadline.async_wait(closeSocket);
  std::array<char, 2048> buffer{};
  while (socket.is_open()) {
    udp::endpoint from;
    const auto size = co_await socket.async_receive_from(
        asio::buffer(buffer), from, asio::redirect_error(use_awaitable, error));
    if (error) {
      break;
    }
    for (const auto line :
         text::split(std::string_view(buffer.data(), size), '\n')) {
      const auto colon = line.find(':');
      if (colon == std::string_view::npos ||
          !text::iequals(text::trim(line.substr(0, colon)), "location")) {
        continue;
      }
      auto location = std::string(text::trim(line.substr(colon + 1)));
      if (std::find(locations.begin(), locations.end(), location) ==
          locations.end()) {
        locations.push_back(std::move(location));
      }
      // Gateways answer within milliseconds; a moment more lets a second
      // one be heard.
      if (deadline.expiry() - std::chrono::steady_clock::now() > answerGrace) {
        deadline.expires_after(answerGrace);
        deadline.async_wait(closeSocket);
      }
    }
  }
  deadline.cancel();
  co_return locations;
}

awaitable<std::optional<PortMapper::Gateway>>
PortMapper::describe(const std::string& location)
{
  const auto url = parseUrl(location);
  if (!url) {
    co_return std::nullopt;
  }
  Gateway gateway;
  std::pair<unsigned, std::string> answer;
  try {
    http::request<http::string_body> request{http::verb::get, url->path, 11};
    answer = co_await exchange(io_, url->host, url->port, std::move(request),
                               requestTime, &gateway.localAddress);
  }
  catch (const std::exception& error) {
    log::debug("Port mapping: cannot read {}: {}", location, error.what());
    co_return std::nullopt;
  }
  auto control = findWanConnection(answer.second, location);
  if (!control) {
    co_return std::nullopt;
  }
  gateway.control = std::move(*control);
  co_return gateway;
}

awaitable<std::string> PortMapper::soap(const std::string& action,
                                        const std::string& arguments,
                                        unsigned& status)
{
  status = 0;
  if (!gateway_) {
    co_return std::string();
  }
  const auto gateway = gateway_->control;
  http::request<http::string_body> request{http::verb::post, gateway.path, 11};
  request.set(http::field::content_type, "text/xml; charset=\"utf-8\"");
  request.set("SOAPAction", fmt::format("\"{}#{}\"", gateway.service, action));
  request.body() =
      fmt::format("<?xml version=\"1.0\"?>\r\n<s:Envelope "
                  "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                  "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/"
                  "\"><s:Body><u:{0} xmlns:u=\"{1}\">{2}"
                  "</u:{0}></s:Body></s:Envelope>\r\n",
                  action, gateway.service, arguments);
  try {
    auto [code, body] =
        co_await exchange(io_, gateway.host, gateway.port, std::move(request),
                          closed_ ? closeRequestTime : requestTime);
    status = code;
    co_return std::move(body);
  }
  catch (const std::exception& error) {
    log::debug("Port mapping: {} failed: {}", action, error.what());
    co_return std::string();
  }
}

awaitable<void> PortMapper::map(Mapping& mapping)
{
  const auto protocol = protocolName(mapping.protocol);
  for (int attempt = 0; attempt < 2; ++attempt) {
    const auto arguments = fmt::format(
        "<NewRemoteHost></NewRemoteHost><NewExternalPort>{0}</NewExternalPort>"
        "<NewProtocol>{1}</NewProtocol><NewInternalPort>{0}</NewInternalPort>"
        "<NewInternalClient>{2}</NewInternalClient><NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>{3}</NewPortMappingDescription>"
        "<NewLeaseDuration>{4}</NewLeaseDuration>",
        mapping.port, protocol,
        gateway_ ? gateway_->localAddress : std::string(), description_,
        permanent_ ? 0 : leaseSeconds);
    unsigned status = 0;
    const auto answer = co_await soap("AddPortMapping", arguments, status);
    if (status == 200) {
      if (!mapping.mapped) {
        log::info("Port mapping: {} port {} forwarded", protocol, mapping.port);
      }
      mapping.mapped = true;
      co_return;
    }
    const auto code = element(answer, "errorCode");
    if (code == "725" && !permanent_) {
      permanent_ = true; // OnlyPermanentLeasesSupported
      continue;
    }
    mapping.mapped = false;
    log::info("Port mapping: {} port {} not forwarded ({})", protocol,
              mapping.port,
              code == "718"   ? "taken by another device"
              : !code.empty() ? "UPnP error " + code
              : status        ? "HTTP " + std::to_string(status)
                              : "no answer");
    co_return;
  }
}

awaitable<void> PortMapper::unmapAll()
{
  for (auto& mapping : mappings_) {
    if (!mapping.mapped) {
      continue;
    }
    unsigned status = 0;
    co_await soap(
        "DeletePortMapping",
        fmt::format("<NewRemoteHost></NewRemoteHost><NewExternalPort>{}</"
                    "NewExternalPort>"
                    "<NewProtocol>{}</NewProtocol>",
                    mapping.port, protocolName(mapping.protocol)),
        status);
    mapping.mapped = false;
  }
}

} // namespace aria2::net
