/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

namespace aria2 {
class Engine;
}

namespace aria2::ed2k {

// Server, Kad and client connections shared by all ED2K tasks.
class Network {
public:
  explicit Network(Engine& engine);
  ~Network();
  void close();

private:
  Engine& engine_;
};

} // namespace aria2::ed2k
