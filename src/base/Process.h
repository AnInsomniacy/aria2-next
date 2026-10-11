/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

bool processAlive(int64_t pid);
// Starts a program without waiting for it; its output is discarded.
bool spawnDetached(const std::string& program,
                   const std::vector<std::string>& arguments);
// Detaches from the terminal (POSIX). Returns false on failure.
bool daemonize();

} // namespace aria2
