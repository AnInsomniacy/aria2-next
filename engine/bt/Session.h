/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <string>
#include <vector>

namespace aria2 {
class Engine;
}

namespace aria2::bt {

// The libtorrent session shared by all BitTorrent tasks.
class Session {
public:
  explicit Session(Engine& engine);
  ~Session();
  void optionsChanged(const std::vector<std::string>& names);
  void close();

private:
  Engine& engine_;
};

} // namespace aria2::bt
