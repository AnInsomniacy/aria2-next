/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aria2 {

enum class OptionType {
  Flag,    // true|false; a bare --name means true
  Integer, // bounded integer
  Size,    // bytes with optional K/M/G suffix
  Number,  // non-negative decimal
  Text,
  Path,
  Choice, // one of `choices`
  List,   // repeatable; values are joined by '\n'
};

namespace scope {
// The option may be given per task (addUri options, input file).
inline constexpr unsigned Task = 1;
// changeOption may modify it on an active task.
inline constexpr unsigned Live = 2;
// changeGlobalOption may modify it.
inline constexpr unsigned Global = 4;
// Never echoed by getOption/getGlobalOption. Task values are still saved to
// the session file, which a restarted task needs to resume.
inline constexpr unsigned Secret = 8;
} // namespace scope

struct OptionSpec {
  std::string_view name;
  char shortName;
  OptionType type;
  std::string_view defaultValue;
  unsigned scope;
  std::string_view tag; // help section
  std::string_view help;
  int64_t min = 0;
  int64_t max = INT64_MAX;
  std::string_view choices = {}; // "a|b|c"
};

std::span<const OptionSpec> optionCatalog();
const OptionSpec* findOption(std::string_view name);
const OptionSpec* findShortOption(char name);
// Retired options that were flags take no value on the command line.
bool isRetiredFlag(std::string_view name);

// One option layer. Task options fall back to the global layer, which falls
// back to catalogue defaults.
class Options {
public:
  explicit Options(std::shared_ptr<const Options> parent = nullptr);

  // Validates and stores a value. Retired aria2 names are mapped or ignored
  // with a warning. Throws Error(ErrorCode::Option) on invalid input.
  void set(std::string_view name, std::string_view value);
  // Like set(), but appends for List options.
  void add(std::string_view name, std::string_view value);
  void erase(std::string_view name) { values_.erase(std::string(name)); }

  bool defined(std::string_view name) const;
  bool hasOwn(std::string_view name) const;
  std::string get(std::string_view name) const;
  bool flag(std::string_view name) const;
  int64_t integer(std::string_view name) const; // Integer and Size options
  double number(std::string_view name) const;
  bool blank(std::string_view name) const { return get(name).empty(); }

  const std::map<std::string, std::string, std::less<>>& own() const
  {
    return values_;
  }
  const std::shared_ptr<const Options>& parent() const { return parent_; }
  // Own values merged over the parent chain, secrets excluded.
  std::map<std::string, std::string> visible() const;
  // A parentless copy of every value set along the chain, for worker threads
  // that must not observe later changes.
  std::shared_ptr<Options> snapshot() const;

private:
  std::shared_ptr<const Options> parent_;
  std::map<std::string, std::string, std::less<>> values_;
};

// Validates `value` for `spec` and returns its canonical form.
std::string normalizeOption(const OptionSpec& spec, std::string_view value);

} // namespace aria2
