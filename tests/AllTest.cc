#include "common.h"

#include <signal.h>

#include <iostream>
#include <string>

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include "Platform.h"
#include "SocketCore.h"
#include "util.h"
#include "console.h"
#include "LogFactory.h"
#include "prefs.h"

namespace {
void listTests(CppUnit::Test* test, const std::string& prefix = "")
{
  const auto name = test->getName();
  const auto path = prefix.empty() ? name : prefix + "/" + name;
  if (test->getChildTestCount() == 0) {
    std::cout << path << "\n";
    return;
  }
  for (int i = 0; i < test->getChildTestCount(); ++i) {
    listTests(test->getChildTestAt(i), path);
  }
}
} // namespace

int main(int argc, char* argv[])
{
  aria2::global::initConsole(false);
  aria2::Platform platform;

#ifdef SIGPIPE
  sigset_t signalMask;
#  ifdef HAVE_SIGACTION
  sigemptyset(&signalMask);
#  else  // !HAVE_SIGACTION
  signalMask = 0;
#  endif // !HAVE_SIGACTION
  aria2::util::setGlobalSignalHandler(SIGPIPE, &signalMask, SIG_IGN, 0);
#endif // SIGPIPE

  // By default, SocketCore uses AF_UNSPEC for getaddrinfo hints to
  // resolve address. Sometime SocketCore::bind() and
  // SocketCore::establishConnection() use difference protocol family
  // and latter cannot connect to former. To avoid this situation, we
  // limit protocol family to AF_INET for unit tests.
  aria2::SocketCore::setProtocolFamily(AF_INET);
  // If AI_ADDRCONFIG is set, tests fail if IPv4 address is not
  // configured.
  aria2::setDefaultAIFlags(0);
  // Create output directory
  aria2::util::mkdirs(A2_TEST_OUT_DIR);

  aria2::LogFactory::setConsoleLogLevel(aria2::V_DEBUG);
  aria2::LogFactory::reconfigure();

  CppUnit::Test* suite = CppUnit::TestFactoryRegistry::getRegistry().makeTest();
  if (argc > 1 && std::string(argv[1]) == "--list") {
    listTests(suite);
    delete suite;
    return 0;
  }
  CppUnit::TextUi::TestRunner runner;
  runner.addTest(suite);

  runner.setOutputter(
      new CppUnit::CompilerOutputter(&runner.result(), std::cerr));

  const std::string testPath = argc > 1 ? argv[1] : "";
  // Run all tests by default, or a single CppUnit test path when supplied.
  bool successfull = runner.run(testPath);

  return successfull ? 0 : 1;
}
