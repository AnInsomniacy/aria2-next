/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Network.h"

namespace aria2::ed2k {

Network::Network(Engine& engine) : engine_(engine) {}
Network::~Network() = default;
void Network::close() {}

} // namespace aria2::ed2k
