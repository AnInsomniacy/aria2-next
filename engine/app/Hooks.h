/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

namespace aria2 {

class Engine;

// Runs the on-download-* commands with aria2's arguments: GID, number of
// files and the first file path.
void installHooks(Engine& engine);

} // namespace aria2
