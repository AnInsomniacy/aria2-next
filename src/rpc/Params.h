/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "base/Error.h"
#include "core/Factory.h"
#include "core/Task.h"

#include <boost/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aria2 {
class Engine;
}

namespace aria2::rpc {

namespace json = boost::json;

[[noreturn]] inline void invalid(std::string_view what)
{
  throw Error(ErrorCode::Unknown, std::string(what));
}

inline const json::value* at(const json::array& params, size_t index)
{
  return index < params.size() && !params[index].is_null() ? &params[index] : nullptr;
}

std::string text(const json::array& params, size_t index, std::string_view name);
std::optional<int64_t> integer(const json::array& params, size_t index);
std::vector<std::string> strings(const json::array& params, size_t index);
std::vector<std::string> keys(const json::array& params, size_t index);
// An options object: string, number and boolean values; arrays for repeatable
// options.
OptionPairs optionPairs(const json::array& params, size_t index);

std::shared_ptr<Task> task(Engine& engine, const json::array& params, size_t index = 0);
json::object status(const Task& task, const std::vector<std::string>& keys);
std::string gidOf(const Task& task);

} // namespace aria2::rpc
