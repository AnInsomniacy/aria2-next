/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "rpc/XmlRpc.h"

#include "base/Error.h"
#include "base/Text.h"

#include <boost/json/array.hpp>

#include <expat.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <limits>
#include <memory>
#include <vector>

namespace aria2::rpc::xml {

namespace json = boost::json;

namespace {

// Deeper documents are not calls any client makes; the limit keeps the
// recursive conversion below off the end of the stack.
constexpr size_t maxDepth = 64;

struct Node {
  std::string name;
  std::string text;
  std::vector<std::unique_ptr<Node>> children;
};

class Reader {
public:
  XML_Parser parser = nullptr;
  std::unique_ptr<Node> root;
  std::string error;

  void stop(std::string why)
  {
    if (error.empty()) {
      error = std::move(why);
    }
    XML_StopParser(parser, XML_FALSE);
  }

  // Expat may deliver events of the current token after stop().
  void start(const XML_Char* name)
  {
    if (!error.empty()) {
      return;
    }
    if (open_.size() >= maxDepth) {
      stop("XML-RPC document is nested too deeply");
      return;
    }
    auto node = std::make_unique<Node>();
    node->name = name;
    auto* raw = node.get();
    if (open_.empty()) {
      root = std::move(node);
    }
    else {
      open_.back()->children.push_back(std::move(node));
    }
    open_.push_back(raw);
  }

  void end()
  {
    if (error.empty() && !open_.empty()) {
      open_.pop_back();
    }
  }

  void characters(std::string_view data)
  {
    if (error.empty() && !open_.empty()) {
      open_.back()->text.append(data);
    }
  }

private:
  std::vector<Node*> open_;
};

std::unique_ptr<Node> parseDocument(std::string_view document)
{
  std::unique_ptr<XML_ParserStruct, decltype(&XML_ParserFree)> parser(
      XML_ParserCreate("UTF-8"), XML_ParserFree);
  if (!parser) {
    throw std::bad_alloc();
  }
  Reader reader;
  reader.parser = parser.get();
  XML_SetUserData(parser.get(), &reader);
  XML_SetElementHandler(
      parser.get(),
      [](void* self, const XML_Char* name, const XML_Char**) {
        static_cast<Reader*>(self)->start(name);
      },
      [](void* self, const XML_Char*) { static_cast<Reader*>(self)->end(); });
  XML_SetCharacterDataHandler(
      parser.get(), [](void* self, const XML_Char* data, int size) {
        static_cast<Reader*>(self)->characters(
            std::string_view(data, static_cast<size_t>(size)));
      });
  // Entity declarations serve no purpose in a call and enable expansion
  // attacks; reject documents that declare any.
  XML_SetEntityDeclHandler(parser.get(),
                           [](void* self, const XML_Char*, int, const XML_Char*,
                              int, const XML_Char*, const XML_Char*,
                              const XML_Char*, const XML_Char*) {
                             static_cast<Reader*>(self)->stop(
                                 "XML-RPC documents must not declare entities");
                           });
  if (document.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    fail(ErrorCode::Unknown, "XML-RPC document is too large");
  }
  const auto parsed = XML_Parse(parser.get(), document.data(),
                                static_cast<int>(document.size()), XML_TRUE);
  if (!reader.error.empty()) {
    fail(ErrorCode::Unknown, "{}", reader.error);
  }
  if (parsed != XML_STATUS_OK) {
    fail(ErrorCode::Unknown, "Invalid XML-RPC document: {}",
         XML_ErrorString(XML_GetErrorCode(parser.get())));
  }
  return std::move(reader.root);
}

const Node* child(const Node& node, std::string_view name)
{
  for (const auto& item : node.children) {
    if (item->name == name) {
      return item.get();
    }
  }
  return nullptr;
}

json::value toJson(const Node& value);

json::value typed(const Node& type)
{
  const auto& name = type.name;
  if (name == "string" || name == "dateTime.iso8601") {
    return json::string(type.text);
  }
  if (name == "int" || name == "i4" || name == "i8") {
    // Signed, unlike the option values text::toInt reads; '+' is allowed too.
    auto digits = text::trim(type.text);
    if (digits.starts_with('+') && !digits.substr(1).starts_with('-')) {
      digits.remove_prefix(1);
    }
    int64_t number = 0;
    const auto [end, error] =
        std::from_chars(digits.data(), digits.data() + digits.size(), number);
    if (digits.empty() || error != std::errc() ||
        end != digits.data() + digits.size()) {
      fail(ErrorCode::Unknown, "Invalid XML-RPC integer {}", type.text);
    }
    return number;
  }
  if (name == "boolean") {
    const auto flag = text::trim(type.text);
    if (flag != "0" && flag != "1") {
      fail(ErrorCode::Unknown, "Invalid XML-RPC boolean {}", type.text);
    }
    return flag == "1";
  }
  if (name == "double") {
    const auto number = text::toDouble(text::trim(type.text));
    if (!number) {
      fail(ErrorCode::Unknown, "Invalid XML-RPC double {}", type.text);
    }
    return *number;
  }
  if (name == "base64") {
    // Encoders may wrap the text; the methods decode it themselves.
    std::string encoded;
    for (const char c : type.text) {
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
        encoded.push_back(c);
      }
    }
    return json::string(encoded);
  }
  if (name == "nil") {
    return nullptr;
  }
  if (name == "array") {
    json::array items;
    if (const auto* data = child(type, "data")) {
      for (const auto& item : data->children) {
        if (item->name == "value") {
          items.push_back(toJson(*item));
        }
      }
    }
    return items;
  }
  if (name == "struct") {
    json::object members;
    for (const auto& member : type.children) {
      const auto* key =
          member->name == "member" ? child(*member, "name") : nullptr;
      const auto* value = key ? child(*member, "value") : nullptr;
      if (!value) {
        fail(ErrorCode::Unknown, "Invalid XML-RPC struct member");
      }
      members[key->text] = toJson(*value);
    }
    return members;
  }
  fail(ErrorCode::Unknown, "Unsupported XML-RPC type {}", name);
}

json::value toJson(const Node& value)
{
  if (value.children.empty()) {
    return json::string(value.text);
  }
  return typed(*value.children.front());
}

void escape(std::string& out, std::string_view value)
{
  const auto valid =
      text::isUtf8(value) ? std::string(value) : text::repairUtf8(value);
  for (const char c : valid) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    default:
      // XML 1.0 has no way to carry the other control characters.
      if (static_cast<unsigned char>(c) >= 0x20 || c == '\t' || c == '\n' ||
          c == '\r') {
        out.push_back(c);
      }
    }
  }
}

void encodeValue(std::string& out, const json::value& value)
{
  out += "<value>";
  switch (value.kind()) {
  case json::kind::string:
    out += "<string>";
    escape(out, value.get_string());
    out += "</string>";
    break;
  case json::kind::int64:
  case json::kind::uint64: {
    const auto number = value.to_number<double>();
    const bool small = number >= std::numeric_limits<int32_t>::min() &&
                       number <= std::numeric_limits<int32_t>::max();
    out += small ? "<int>" : "<i8>";
    out += value.is_int64() ? std::to_string(value.get_int64())
                            : std::to_string(value.get_uint64());
    out += small ? "</int>" : "</i8>";
    break;
  }
  case json::kind::double_:
    out += "<double>";
    out += std::isfinite(value.get_double())
               ? fmt::format("{}", value.get_double())
               : std::string("0");
    out += "</double>";
    break;
  case json::kind::bool_:
    out += value.get_bool() ? "<boolean>1</boolean>" : "<boolean>0</boolean>";
    break;
  case json::kind::array:
    out += "<array><data>";
    for (const auto& item : value.get_array()) {
      encodeValue(out, item);
    }
    out += "</data></array>";
    break;
  case json::kind::object:
    out += "<struct>";
    for (const auto& [key, item] : value.get_object()) {
      out += "<member><name>";
      escape(out, key);
      out += "</name>";
      encodeValue(out, item);
      out += "</member>";
    }
    out += "</struct>";
    break;
  case json::kind::null:
    // aria2 never sent nil, which many clients reject.
    out += "<string></string>";
    break;
  }
  out += "</value>";
}

json::object fault(const json::value& error)
{
  const auto* object = error.if_object();
  const auto* code = object ? object->if_contains("code") : nullptr;
  const auto* message = object ? object->if_contains("message") : nullptr;
  return json::object{
      {"faultCode", code && code->is_int64() ? code->get_int64() : 1},
      {"faultString", message && message->is_string() ? message->get_string()
                                                      : json::string()}};
}

} // namespace

json::object parseCall(std::string_view document)
{
  const auto root = parseDocument(document);
  if (!root || root->name != "methodCall") {
    fail(ErrorCode::Unknown, "Not an XML-RPC methodCall");
  }
  const auto* name = child(*root, "methodName");
  if (!name || text::trim(name->text).empty()) {
    fail(ErrorCode::Unknown, "XML-RPC methodCall has no methodName");
  }
  json::array params;
  if (const auto* list = child(*root, "params")) {
    for (const auto& param : list->children) {
      const auto* value =
          param->name == "param" ? child(*param, "value") : nullptr;
      if (!value) {
        fail(ErrorCode::Unknown, "Invalid XML-RPC param");
      }
      params.push_back(toJson(*value));
    }
  }
  return json::object{{"jsonrpc", "2.0"},
                      {"id", 0},
                      {"method", text::trim(name->text)},
                      {"params", std::move(params)}};
}

std::string encodeResponse(const json::value& response, bool multicall)
{
  std::string out = "<?xml version=\"1.0\"?><methodResponse>";
  const auto* object = response.if_object();
  const auto* error = object ? object->if_contains("error") : nullptr;
  if (error || !object) {
    out += "<fault>";
    encodeValue(out, fault(error ? *error : json::value()));
    out += "</fault>";
  }
  else {
    json::value result = object->contains("result") ? object->at("result")
                                                    : json::value(nullptr);
    if (auto* calls = result.if_array(); calls && multicall) {
      for (auto& call : *calls) {
        if (call.is_object()) {
          call = fault(call);
        }
      }
    }
    out += "<params><param>";
    encodeValue(out, result);
    out += "</param></params>";
  }
  out += "</methodResponse>";
  return out;
}

} // namespace aria2::rpc::xml
