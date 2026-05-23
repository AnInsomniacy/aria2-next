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
#ifndef D_BOOST_JSON_VALUE_H
#define D_BOOST_JSON_VALUE_H

#include "common.h"

#include <memory>
#include <string>

#include <boost/json/value.hpp>

namespace aria2 {

class ValueBase;

namespace json {

std::unique_ptr<ValueBase> fromBoostJson(const boost::json::value& value);
boost::json::value toBoostJson(const ValueBase* value);
std::string serialize(const ValueBase* value);
std::unique_ptr<ValueBase> parseValue(const std::string& body, bool& ok);

} // namespace json

} // namespace aria2

#endif // D_BOOST_JSON_VALUE_H
