/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "app/Hooks.h"

#include "base/Log.h"
#include "base/Process.h"
#include "core/Engine.h"

namespace aria2 {

void installHooks(Engine& engine)
{
  engine.listen([&engine](Event event, const Task& task) {
    const char* option = nullptr;
    switch (event) {
    case Event::Start:
      option = "on-download-start";
      break;
    case Event::Pause:
      option = "on-download-pause";
      break;
    case Event::Complete:
      option = "on-download-complete";
      break;
    case Event::Error:
      option = "on-download-error";
      break;
    case Event::BtComplete:
      option = "on-bt-download-complete";
      break;
    case Event::Stop:
      break;
    }
    const auto run = [&](const char* name) {
      const auto command = engine.global().get(name);
      if (command.empty()) {
        return;
      }
      const auto files = task.files();
      const auto first = files.empty() ? std::string() : files.front().path;
      if (!spawnDetached(command, {gidText(task.gid()), std::to_string(files.size()), first})) {
        log::warn("Could not run {} command {}", name, command);
      }
    };
    if (option) {
      run(option);
    }
    // aria2 runs on-download-stop for every stopped download.
    if (event == Event::Stop || event == Event::Complete || event == Event::Error) {
      run("on-download-stop");
    }
  });
}

} // namespace aria2
