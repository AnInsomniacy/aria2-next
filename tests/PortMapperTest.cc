/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "net/PortMapper.h"

#include <doctest/doctest.h>

using aria2::net::findWanConnection;

namespace {

std::string description(const std::string& services,
                        const std::string& base = {})
{
  return "<?xml version=\"1.0\"?><root "
         "xmlns=\"urn:schemas-upnp-org:device-1-0\">" +
         (base.empty() ? std::string() : "<URLBase>" + base + "</URLBase>") +
         "<device><deviceType>urn:schemas-upnp-org:device:"
         "InternetGatewayDevice:1</deviceType>"
         "<serviceList><service><serviceType>urn:schemas-upnp-org:service:"
         "Layer3Forwarding:1"
         "</serviceType><controlURL>/l3f</controlURL></service></"
         "serviceList><deviceList><device>"
         "<serviceList>" +
         services + "</serviceList></device></deviceList></device></root>";
}

std::string service(const std::string& type, const std::string& control)
{
  return "<service><serviceType>urn:schemas-upnp-org:service:" + type +
         "</serviceType><serviceId>x</serviceId><controlURL>" + control +
         "</controlURL></service>";
}

} // namespace

TEST_CASE("the WAN IP connection service is preferred over PPP")
{
  const auto control =
      findWanConnection(description(service("WANPPPConnection:1", "/ppp") +
                                    service("WANIPConnection:2", "ctl/ip")),
                        "http://192.168.1.1:5000/rootDesc.xml");
  REQUIRE(control);
  CHECK(control->host == "192.168.1.1");
  CHECK(control->port == "5000");
  CHECK(control->path == "/ctl/ip"); // relative to the description's host
  CHECK(control->service == "urn:schemas-upnp-org:service:WANIPConnection:2");
}

TEST_CASE("control URLs honour URLBase and absolute forms")
{
  auto control =
      findWanConnection(description(service("WANPPPConnection:1", "/upnp/ppp"),
                                    "http://10.0.0.1:49152/"),
                        "http://10.0.0.1:1900/desc.xml");
  REQUIRE(control);
  CHECK(control->port == "49152");
  CHECK(control->path == "/upnp/ppp");

  control = findWanConnection(
      description(service("WANIPConnection:1",
                          "http://10.0.0.2/control?WANIPConnection")),
      "http://10.0.0.1:1900/desc.xml");
  REQUIRE(control);
  CHECK(control->host == "10.0.0.2");
  CHECK(control->port == "80");
  CHECK(control->path == "/control?WANIPConnection");
}

TEST_CASE("devices without a WAN connection are not gateways")
{
  CHECK_FALSE(
      findWanConnection(description(service("ContentDirectory:1", "/cd")),
                        "http://192.168.1.20:2869/desc.xml"));
  CHECK_FALSE(findWanConnection("not xml", "http://192.168.1.1/"));
  CHECK_FALSE(findWanConnection(description(service("WANIPConnection:1", "/c")),
                                "ftp://x/"));
}
