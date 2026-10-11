/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once
#include "media/Control.h"
#include <memory>
#include <string>
#include <thread>
namespace aria2 {
class Options;
namespace media {
class Session {
public:
  Session(std::shared_ptr<const Options> option, std::string uri,
          std::string gid, std::string directory,
          std::shared_ptr<Control> control);
  ~Session();
  void join();

private:
  std::shared_ptr<Control> control_;
  std::thread worker_;
};
} // namespace media
} // namespace aria2
