/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <string>
#include <vector>

namespace aria2 {

std::string engineVersion();
std::vector<const char*> enabledFeatures();
// Multi-line text for --version.
std::string versionText();

} // namespace aria2
