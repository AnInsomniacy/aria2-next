/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "app/Console.h"
#include "app/Hooks.h"
#include "base/Error.h"
#include "base/Log.h"
#include "base/Path.h"
#include "base/Process.h"
#include "core/Engine.h"
#include "core/Factory.h"
#include "core/Version.h"
#include "options/CommandLine.h"
#include "rpc/Dispatcher.h"
#include "rpc/Server.h"
#include "state/Session.h"

#include <boost/asio/signal_set.hpp>
#include <curl/curl.h>

#include <cstdio>
#include <iostream>

#ifdef _WIN32
#  include <shellapi.h>
#  include <windows.h>
#endif

using namespace aria2;

namespace {

std::vector<std::string> arguments(int argc, char** argv)
{
  std::vector<std::string> result;
#ifdef _WIN32
  int count = 0;
  auto** wide = CommandLineToArgvW(GetCommandLineW(), &count);
  for (int i = 1; i < count; ++i) {
    result.push_back(toUtf8(fs::path(wide[i])));
  }
  LocalFree(wide);
  (void)argc;
  (void)argv;
#else
  result.assign(argv + 1, argv + argc);
#endif
  return result;
}

void configureLog(const Options& options)
{
  log::Config config;
  config.file = options.get("log");
  log::parseLevel(options.get("log-level"), config.fileLevel);
  log::parseLevel(options.get("console-log-level"), config.consoleLevel);
  if (options.flag("quiet")) {
    config.consoleLevel = log::Level::Error;
  }
  config.maxFileSize = static_cast<size_t>(options.integer("log-max-size"));
  config.maxFiles = static_cast<size_t>(options.integer("log-max-files"));
  config.color = options.flag("enable-color");
  log::configure(config);
}

std::shared_ptr<Options> globalOptions(const CommandLine& line)
{
  auto global = std::make_shared<Options>();
  bool noConf = false;
  std::string confPath;
  for (const auto& [name, value] : line.options) {
    if (name == "no-conf") {
      noConf = value != "false";
    }
    else if (name == "conf-path") {
      confPath = value;
    }
  }
  if (!noConf) {
    const auto path = confPath.empty() ? defaultConfigPath() : confPath;
    if (const auto content = readFile(path)) {
      applyOptions(*global, parseConfigFile(*content, path));
    }
    else if (!confPath.empty()) {
      fail(ErrorCode::Option, "Cannot read configuration file {}", confPath);
    }
  }
  applyOptions(*global, line.options);
  if (global->blank("dir")) {
    global->set("dir", toUtf8(fs::current_path()));
  }
  return global;
}

void addInput(Engine& engine, const CommandLine& line)
{
  auto& global = engine.global();
  if (!global.blank("input-file")) {
    const auto path = global.get("input-file");
    std::optional<std::string> content;
    if (path == "-") {
      content = std::string(std::istreambuf_iterator<char>(std::cin), {});
    }
    else {
      content = readFile(path);
    }
    if (!content) {
      fail(ErrorCode::Option, "Cannot read input file {}", path);
    }
    for (auto& entry : parseSession(*content)) {
      try {
        for (auto& task : createTasks(engine, std::move(entry.uris),
                                      taskOptions(engine, entry.options))) {
          engine.add(task);
        }
      }
      catch (const Error& error) {
        log::error("Input file entry skipped: {}", error.what());
      }
    }
  }
  if (!global.blank("torrent-file")) {
    const auto torrent = readFile(global.get("torrent-file"));
    if (!torrent) {
      fail(ErrorCode::FileOpen, "Cannot read {}", global.get("torrent-file"));
    }
    engine.add(createTorrentTask(engine, *torrent, line.uris,
                                 taskOptions(engine, {})));
    return;
  }
  if (!global.blank("metalink-file")) {
    const auto metalink = readFile(global.get("metalink-file"));
    if (!metalink) {
      fail(ErrorCode::FileOpen, "Cannot read {}", global.get("metalink-file"));
    }
    for (auto& task : createMetalinkTasks(engine, *metalink, taskOptions(engine, {}))) {
      engine.add(task);
    }
  }
  if (line.uris.empty()) {
    return;
  }
  // Command-line URIs are mirrors of one file unless -Z asks for one task each.
  if (global.flag("force-sequential")) {
    for (const auto& uri : line.uris) {
      for (auto& task : createTasks(engine, {uri}, taskOptions(engine, {}))) {
        engine.add(task);
      }
    }
  }
  else {
    for (auto& task : createTasks(engine, line.uris, taskOptions(engine, {}))) {
      engine.add(task);
    }
  }
}

int run(const std::vector<std::string>& args)
{
  const auto line = parseCommandLine(args);
  auto global = globalOptions(line);
  if (global->flag("version")) {
    std::fputs(versionText().c_str(), stdout);
    return 0;
  }
  if (global->defined("help")) {
    std::fputs(helpText(global->get("help")).c_str(), stdout);
    return 0;
  }
  if (global->flag("daemon") && !daemonize()) {
    fail(ErrorCode::Unknown, "Cannot run as a daemon");
  }
  configureLog(*global);
  const bool rpc = global->flag("enable-rpc");
  if (line.uris.empty() && global->blank("input-file") && global->blank("torrent-file") &&
      global->blank("metalink-file") && !rpc) {
    std::fputs(helpText("#basic").c_str(), stdout);
    return 1;
  }

  Engine engine(global);
  installHooks(engine);
  std::unique_ptr<rpc::Dispatcher> dispatcher;
  std::unique_ptr<rpc::Server> server;
  if (rpc) {
    dispatcher = std::make_unique<rpc::Dispatcher>(engine);
    rpc::registerMethods(*dispatcher, engine);
    server = std::make_unique<rpc::Server>(engine, *dispatcher);
    server->start();
    engine.listen([&server](Event event, const Task& task) {
      server->notify(std::string(eventMethod(event)), gidText(task.gid()));
    });
  }
  addInput(engine, line);

  boost::asio::signal_set signals(engine.io(), SIGINT, SIGTERM);
  int interrupts = 0;
  std::function<void(const boost::system::error_code&, int)> onSignal =
      [&](const boost::system::error_code& error, int) {
        if (error) {
          return;
        }
        engine.shutdown(++interrupts > 1);
        signals.async_wait(onSignal);
      };
  signals.async_wait(onSignal);

  Console console(engine);
  console.start();
  const auto status = engine.run();
  console.stop();
  if (server) {
    server->stop();
  }
  return status;
}

} // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  curl_global_init(CURL_GLOBAL_ALL);
  int status = 1;
  try {
    status = run(arguments(argc, argv));
  }
  catch (const Error& error) {
    log::error("{}", error.what());
    std::fprintf(stderr, "Exception caught: %s\n", error.what());
    status = static_cast<int>(error.code());
  }
  catch (const std::exception& error) {
    log::error("{}", error.what());
    std::fprintf(stderr, "Exception caught: %s\n", error.what());
  }
  log::logger()->flush();
  curl_global_cleanup();
  return status;
}
