/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/CommandLine.h"

#include "base/Error.h"
#include "base/Text.h"

namespace aria2 {
namespace {

bool takesValue(std::string_view name, const OptionSpec* spec)
{
  return spec ? spec->type != OptionType::Flag : !isRetiredFlag(name);
}

} // namespace

CommandLine parseCommandLine(const std::vector<std::string>& arguments)
{
  CommandLine result;
  for (size_t i = 0; i < arguments.size(); ++i) {
    const auto& argument = arguments[i];
    if (argument == "--") {
      result.uris.insert(result.uris.end(), arguments.begin() + i + 1,
                         arguments.end());
      break;
    }
    if (argument.starts_with("--")) {
      const auto equals = argument.find('=');
      const auto name = argument.substr(2, equals - 2);
      const auto* spec = findOption(name);
      if (equals != std::string::npos) {
        result.options.emplace_back(name, argument.substr(equals + 1));
      }
      else if (!takesValue(name, spec)) {
        result.options.emplace_back(name, "true");
      }
      else if (name == "help") {
        result.options.emplace_back(name, "#basic");
      }
      else if (i + 1 < arguments.size()) {
        result.options.emplace_back(name, arguments[++i]);
      }
      else {
        fail(ErrorCode::Option, "--{} requires a value", name);
      }
      continue;
    }
    if (argument.size() > 1 && argument[0] == '-') {
      // A cluster of short options: flags first, then at most one with a value.
      for (size_t k = 1; k < argument.size(); ++k) {
        const auto* spec = findShortOption(argument[k]);
        if (!spec) {
          fail(ErrorCode::Option, "Unknown option -{}", argument[k]);
        }
        if (!takesValue(spec->name, spec)) {
          result.options.emplace_back(spec->name, "true");
          continue;
        }
        if (k + 1 < argument.size()) {
          result.options.emplace_back(spec->name, argument.substr(k + 1));
        }
        else if (spec->name == "help") {
          result.options.emplace_back(spec->name, "#basic");
        }
        else if (i + 1 < arguments.size()) {
          result.options.emplace_back(spec->name, arguments[++i]);
        }
        else {
          fail(ErrorCode::Option, "-{} requires a value", argument[k]);
        }
        break;
      }
      continue;
    }
    result.uris.push_back(argument);
  }
  return result;
}

std::vector<std::pair<std::string, std::string>>
parseConfigFile(std::string_view content, std::string_view origin)
{
  std::vector<std::pair<std::string, std::string>> result;
  size_t number = 0;
  for (const auto& raw : text::split(content, '\n', true)) {
    ++number;
    const auto line = text::trim(raw);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const auto equals = line.find('=');
    if (equals == std::string_view::npos) {
      fail(ErrorCode::Option, "{}:{}: expected name=value", origin, number);
    }
    result.emplace_back(std::string(text::trim(line.substr(0, equals))),
                        std::string(text::trim(line.substr(equals + 1))));
  }
  return result;
}

void applyOptions(Options& target,
                  const std::vector<std::pair<std::string, std::string>>& pairs)
{
  for (const auto& [name, value] : pairs) {
    target.add(name, value);
  }
}

std::string helpText(std::string_view topic)
{
  std::string result = "Usage: aria2-next [OPTIONS] [URI | MAGNET | TORRENT | "
                       "METALINK | ED2K]...\n\n";
  const bool all = topic == "#all";
  const auto tag = topic.starts_with('#') ? topic.substr(1) : std::string_view();
  for (const auto& spec : optionCatalog()) {
    const bool match = all || (tag.empty() ? spec.name.find(topic) !=
                                                 std::string_view::npos
                                           : spec.tag == tag);
    if (!match) {
      continue;
    }
    std::string head = spec.shortName
                           ? fmt::format(" -{}, --{}", spec.shortName, spec.name)
                           : fmt::format("     --{}", spec.name);
    if (spec.type != OptionType::Flag) {
      head += spec.type == OptionType::Choice
                  ? fmt::format("={}", spec.choices)
                  : "=" + text::lower(spec.type == OptionType::Size ? "SIZE"
                                      : spec.type == OptionType::Path
                                          ? "PATH"
                                          : "VALUE");
    }
    result += head + "\n        " + std::string(spec.help) + "\n";
    if (!spec.defaultValue.empty()) {
      result += "        Default: " + std::string(spec.defaultValue) + "\n";
    }
  }
  result += "\nTags: #basic #http #file #bittorrent #metalink #ed2k #media "
            "#rpc #advanced #all\n";
  return result;
}

} // namespace aria2
