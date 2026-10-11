/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "bt/Session.h"

namespace aria2::bt {

Session::Session(Engine& engine) : engine_(engine) {}
Session::~Session() = default;
void Session::optionsChanged(const std::vector<std::string>&) {}
void Session::close() {}

} // namespace aria2::bt
