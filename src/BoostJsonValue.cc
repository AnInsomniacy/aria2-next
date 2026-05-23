/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#include "BoostJsonValue.h"

#ifdef ARIA2_BOOST_JSON_HEADER_ONLY
#include <boost/json/src.hpp>
#endif // ARIA2_BOOST_JSON_HEADER_ONLY
#include <boost/json.hpp>

#include "ValueBase.h"

namespace aria2 {

namespace json {

std::unique_ptr<ValueBase> fromBoostJson(const boost::json::value& value)
{
  if (value.is_null()) {
    return Null::g();
  }
  if (auto b = value.if_bool()) {
    return *b ? Bool::gTrue() : Bool::gFalse();
  }
  if (auto i = value.if_int64()) {
    return Integer::g(*i);
  }
  if (auto u = value.if_uint64()) {
    return Integer::g(static_cast<Integer::ValueType>(*u));
  }
  if (auto d = value.if_double()) {
    return Integer::g(static_cast<Integer::ValueType>(*d));
  }
  if (auto s = value.if_string()) {
    return String::g(std::string(s->data(), s->size()));
  }
  if (auto a = value.if_array()) {
    auto out = List::g();
    for (const auto& elem : *a) {
      out->append(fromBoostJson(elem));
    }
    return out;
  }
  if (auto o = value.if_object()) {
    auto out = Dict::g();
    for (const auto& elem : *o) {
      out->put(std::string(elem.key().data(), elem.key().size()),
               fromBoostJson(elem.value()));
    }
    return out;
  }
  return Null::g();
}

boost::json::value toBoostJson(const ValueBase* value)
{
  class Visitor : public ValueBaseVisitor {
  public:
    boost::json::value value;

    virtual void visit(const String& string) CXX11_OVERRIDE
    {
      value = string.s();
    }

    virtual void visit(const Integer& integer) CXX11_OVERRIDE
    {
      value = integer.i();
    }

    virtual void visit(const Bool& boolValue) CXX11_OVERRIDE
    {
      value = boolValue.val();
    }

    virtual void visit(const Null& nullValue) CXX11_OVERRIDE { value = nullptr; }

    virtual void visit(const List& list) CXX11_OVERRIDE
    {
      boost::json::array array;
      for (const auto& elem : list) {
        array.push_back(toBoostJson(elem.get()));
      }
      value = std::move(array);
    }

    virtual void visit(const Dict& dict) CXX11_OVERRIDE
    {
      boost::json::object object;
      for (const auto& elem : dict) {
        object[elem.first] = toBoostJson(elem.second.get());
      }
      value = std::move(object);
    }
  };

  Visitor visitor;
  value->accept(visitor);
  return std::move(visitor.value);
}

std::string serialize(const ValueBase* value)
{
  return boost::json::serialize(toBoostJson(value));
}

std::unique_ptr<ValueBase> parseValue(const std::string& body, bool& ok)
{
  boost::system::error_code ec;
  auto value = boost::json::parse(body, ec);
  ok = !ec;
  if (!ok) {
    return nullptr;
  }
  return fromBoostJson(value);
}

} // namespace json

} // namespace aria2
