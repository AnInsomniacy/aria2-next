/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "base/Process.h"

#include "base/Path.h"

#ifdef _WIN32
#  include <windows.h>
#else
#  include <cerrno>
#  include <csignal>
#  include <fcntl.h>
#  include <spawn.h>
#  include <unistd.h>
extern char** environ;
#endif

namespace aria2 {

#ifdef _WIN32

bool processAlive(int64_t pid)
{
  const auto handle =
      OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
  if (!handle) {
    return GetLastError() == ERROR_ACCESS_DENIED;
  }
  const bool alive = WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
  CloseHandle(handle);
  return alive;
}

namespace {
// Quotes one argument for CommandLineToArgvW-compatible parsing.
std::wstring quote(const std::wstring& argument)
{
  if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos) {
    return argument;
  }
  std::wstring result = L"\"";
  size_t backslashes = 0;
  for (const auto ch : argument) {
    if (ch == L'\\') {
      ++backslashes;
      continue;
    }
    result.append(ch == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
    backslashes = 0;
    result += ch;
  }
  result.append(backslashes * 2, L'\\');
  return result + L"\"";
}
} // namespace

bool spawnDetached(const std::string& program,
                   const std::vector<std::string>& arguments)
{
  std::wstring command = quote(toPath(program).native());
  for (const auto& argument : arguments) {
    command += L" " + quote(toPath(argument).native());
  }
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr,
                      &startup, &process)) {
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

bool daemonize() { return false; }

#else

bool processAlive(int64_t pid)
{
  return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}

bool spawnDetached(const std::string& program,
                   const std::vector<std::string>& arguments)
{
  // Finished hooks are reaped automatically.
  signal(SIGCHLD, SIG_IGN);
  std::vector<char*> argv;
  argv.push_back(const_cast<char*>(program.c_str()));
  for (const auto& argument : arguments) {
    argv.push_back(const_cast<char*>(argument.c_str()));
  }
  argv.push_back(nullptr);
#  if defined(__ANDROID__) && __ANDROID_API__ < 28
  // Bionic has posix_spawn only from API 28; the release targets API 24.
  // Between fork and exec only async-signal-safe calls are made.
  const auto child = fork();
  if (child == 0) {
    if (const auto null = open("/dev/null", O_WRONLY); null >= 0) {
      dup2(null, STDOUT_FILENO);
    }
    execvp(program.c_str(), argv.data());
    _exit(127);
  }
  return child > 0;
#  else
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                   O_WRONLY, 0);
  pid_t child = 0;
  const auto result = posix_spawnp(&child, program.c_str(), &actions, nullptr,
                                   argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  return result == 0;
#  endif
}

bool daemonize()
{
  if (const auto child = fork(); child < 0) {
    return false;
  }
  else if (child > 0) {
    _exit(0);
  }
  if (setsid() < 0) {
    return false;
  }
  const auto null = open("/dev/null", O_RDWR);
  if (null >= 0) {
    dup2(null, STDIN_FILENO);
    dup2(null, STDOUT_FILENO);
    dup2(null, STDERR_FILENO);
    if (null > STDERR_FILENO) {
      close(null);
    }
  }
  return true;
}

#endif

} // namespace aria2
