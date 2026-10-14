/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Error.h"
#include "rpc/XmlRpc.h"

#include <boost/json/parse.hpp>

#include <doctest/doctest.h>

using namespace aria2;
namespace json = boost::json;

TEST_CASE("XML-RPC calls become JSON-RPC requests with typed params")
{
  const auto request = rpc::xml::parseCall(R"(<?xml version="1.0"?>
<methodCall>
  <methodName> aria2.addUri </methodName>
  <params>
    <param><value><string>token:secret</string></value></param>
    <param><value><array><data>
      <value>http://example/a&amp;b</value>
      <value><string></string></value>
    </data></array></value></param>
    <param><value><struct>
      <member><name>split</name><value><i4>4</i4></value></member>
      <member><name>continue</name><value><boolean>1</boolean></value></member>
      <member><name>ratio</name><value><double>1.5</double></value></member>
      <member><name>empty</name><value></value></member>
    </struct></value></param>
    <param><value><int>-7</int></value></param>
    <param><value><base64>
      ZGF0
      YQ==
    </base64></value></param>
  </params>
</methodCall>)");
  CHECK(request.at("method") == "aria2.addUri");
  CHECK(request.at("params") == json::parse(R"([
    "token:secret",
    ["http://example/a&b", ""],
    {"split": 4, "continue": true, "ratio": 1.5, "empty": ""},
    -7,
    "ZGF0YQ=="
  ])"));
}

TEST_CASE("XML-RPC calls without params or with bad values are handled")
{
  const auto request = rpc::xml::parseCall(
      "<methodCall><methodName>aria2.getVersion</methodName></methodCall>");
  CHECK(request.at("method") == "aria2.getVersion");
  CHECK(request.at("params").as_array().empty());

  CHECK_THROWS_AS(rpc::xml::parseCall("<methodResponse/>"), Error);
  CHECK_THROWS_AS(rpc::xml::parseCall("<methodCall><params/></methodCall>"),
                  Error);
  CHECK_THROWS_AS(rpc::xml::parseCall("<methodCall><methodName>x"), Error);
  CHECK_THROWS_AS(
      rpc::xml::parseCall("<methodCall><methodName>x</methodName><params>"
                          "<param><value><int>nope</int></value></param>"
                          "</params></methodCall>"),
      Error);
  CHECK_THROWS_AS(rpc::xml::parseCall(R"(<?xml version="1.0"?>
<!DOCTYPE methodCall [<!ENTITY a "aaaaaaaaaa">]>
<methodCall><methodName>&a;</methodName></methodCall>)"),
                  Error);
  std::string deep = "<methodCall><methodName>x</methodName><params><param>";
  for (int i = 0; i < 100; ++i) {
    deep += "<value><array><data>";
  }
  CHECK_THROWS_AS(rpc::xml::parseCall(deep), Error);
}

TEST_CASE("XML-RPC responses carry the result or a fault")
{
  CHECK(
      rpc::xml::encodeResponse(json::parse(R"({"id":0,"jsonrpc":"2.0","result":{
              "gid":"2089b05ecca3d829","files":[{"path":"a<b"}],
              "count":3,"big":5000000000,"done":true,"none":null}})"),
                               false) ==
      "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
      "<member><name>gid</name><value><string>2089b05ecca3d829</string>"
      "</value></member>"
      "<member><name>files</name><value><array><data><value><struct>"
      "<member><name>path</name><value><string>a&lt;b</string></value>"
      "</member></struct></value></data></array></value></member>"
      "<member><name>count</name><value><int>3</int></value></member>"
      "<member><name>big</name><value><i8>5000000000</i8></value></member>"
      "<member><name>done</name><value><boolean>1</boolean></value>"
      "</member>"
      "<member><name>none</name><value><string></string></value></member>"
      "</struct></value></param></params></methodResponse>");
  CHECK(rpc::xml::encodeResponse(json::parse(R"({"id":0,"jsonrpc":"2.0",
              "error":{"code":1,"message":"Unauthorized"}})"),
                                 false) ==
        "<?xml version=\"1.0\"?><methodResponse><fault><value><struct>"
        "<member><name>faultCode</name><value><int>1</int></value></member>"
        "<member><name>faultString</name><value><string>Unauthorized</string>"
        "</value></member></struct></value></fault></methodResponse>");
}

TEST_CASE("XML-RPC multicall failures are fault structs")
{
  const auto encoded =
      rpc::xml::encodeResponse(json::parse(R"({"id":0,"jsonrpc":"2.0","result":[
        ["ok"], {"code":1,"message":"No such method: x"}]})"),
                               true);
  CHECK(encoded.find("<value><array><data><value><string>ok</string>") !=
        std::string::npos);
  CHECK(encoded.find("<name>faultCode</name><value><int>1</int>") !=
        std::string::npos);
  CHECK(encoded.find("<name>faultString</name><value><string>No such method: "
                     "x</string>") != std::string::npos);
}
