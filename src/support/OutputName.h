/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef ARIA2_OUTPUT_NAME_H
#define ARIA2_OUTPUT_NAME_H

#include <string>

namespace aria2 {
class Option;
namespace output {
// Names are Unicode text after this boundary, never another encoded URL.
std::string safeName(const std::string& name);
std::string urlName(const std::string& uri);
std::string suggestedName(const Option& option, const std::string& uri,
                          const std::string& disposition = {},
                          const std::string& originalUri = {});
std::string mediaName(const Option& option, const std::string& uri);
} // namespace output
} // namespace aria2
#endif
