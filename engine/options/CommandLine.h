/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "options/Options.h"

#include <string>
#include <vector>

namespace aria2 {

struct CommandLine {
  std::vector<std::pair<std::string, std::string>> options; // in given order
  std::vector<std::string> uris;
};

// getopt_long-compatible parsing: --name=value, --name value, -x16, -x 16,
// bare flags, and "--" ending option processing. Throws Error(Option).
CommandLine parseCommandLine(const std::vector<std::string>& arguments);

// name=value lines; '#' starts a comment line. Throws Error(Option) with the
// line number on malformed input.
std::vector<std::pair<std::string, std::string>>
parseConfigFile(std::string_view content, std::string_view origin);

// Applies parsed pairs to an option layer. List options accumulate.
void applyOptions(Options& target,
                  const std::vector<std::pair<std::string, std::string>>& pairs);

std::string helpText(std::string_view topic);

} // namespace aria2
