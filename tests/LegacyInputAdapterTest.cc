#include "aria2/aria2.h"
#include "LegacyInputAdapter.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "a2doctest.h"

#include "Option.h"
#include "OptionParser.h"
#include "prefs.h"

namespace aria2 {

namespace {

std::string valueFor(const KeyVals& options, const std::string& name)
{
  const auto item = std::find_if(options.begin(), options.end(),
                                 [&name](const KeyVals::value_type& option) {
                                   return option.first == name;
                                 });
  return item == options.end() ? std::string() : item->second;
}

} // namespace

TEST_CASE("LegacyInputAdapter normalizes exact and approximate options")
{
  const KeyVals input{{"auto-save-interval", "15"},
                      {"split", "8"},
                      {"max-connection-per-server", "4"},
                      {"ftp-user", "anonymous"},
                      {"dir", "/tmp/downloads"}};
  const auto output =
      normalizeLegacyInput(input, LegacyInputSource::Configuration);

  CHECK_EQ("15", valueFor(output, "state-save-interval"));
  CHECK_EQ("4", valueFor(output, "stream-max-connections"));
  CHECK_EQ("/tmp/downloads", valueFor(output, "dir"));
  CHECK(valueFor(output, "ftp-user").empty());

  const auto clamped = normalizeLegacyInput({{"split", "512"}},
                                            LegacyInputSource::Configuration);
  CHECK_EQ("256", valueFor(clamped, "stream-max-connections"));
}

TEST_CASE("LegacyInputAdapter gives canonical options precedence")
{
  const KeyVals input{
      {"split", "4"}, {"stream-max-connections", "7"}, {"log-level", "notice"}};
  const auto output = normalizeLegacyInput(input, LegacyInputSource::Rpc);

  CHECK_EQ("7", valueFor(output, "stream-max-connections"));
  CHECK_EQ("info", valueFor(output, "log-level"));
  CHECK_EQ(1, std::count_if(output.begin(), output.end(), [](const auto& item) {
             return item.first == "stream-max-connections";
           }));
}

TEST_CASE("LegacyInputAdapter normalizes retired port and Metalink values")
{
  const auto output =
      normalizeLegacyInput({{"listen-port", "6881-6999"},
                            {"dht-listen-port", "7000-7010"},
                            {"metalink-preferred-protocol", "ftp"},
                            {"metalink-preferred-protocol", "https"}},
                           LegacyInputSource::Configuration);

  CHECK_EQ("6881", valueFor(output, "listen-port"));
  CHECK_EQ("https", valueFor(output, "metalink-preferred-protocol"));
  CHECK_THROWS(normalizeLegacyInput({{"listen-port", "80-90"}},
                                    LegacyInputSource::Configuration));
}

TEST_CASE("LegacyInputAdapter accepts retired DNS controls")
{
  const auto output =
      normalizeLegacyInput({{"async-dns", "false"},
                            {"async-dns-server", "1.1.1.1,8.8.8.8"},
                            {"enable-async-dns6", "true"},
                            {"dir", "/tmp/downloads"}},
                           LegacyInputSource::Configuration);

  CHECK_EQ(1, output.size());
  CHECK_EQ("/tmp/downloads", valueFor(output, "dir"));
  CHECK_THROWS(normalizeLegacyInput({{"async-dns", "invalid"}},
                                    LegacyInputSource::Configuration));
}

TEST_CASE("LegacyInputAdapter rewrites legacy command-line forms")
{
  char executable[] = "aria2-next";
  char split[] = "-s8";
  char perServer[] = "-x";
  char perServerValue[] = "4";
  char retired[] = "--enable-http-pipelining=true";
  char resolver[] = "--async-dns=false";
  char password[] = "--ftp-passwd=secret";
  char current[] = "--dir=/tmp/downloads";
  char uri[] = "https://example.com/file";
  char* argv[]{executable,     split,   perServer,
               perServerValue, retired, resolver,
               password,       current, uri};

  const auto output = normalizeLegacyCommandLine(
      static_cast<int>(std::size(argv)), argv, LegacyInputSource::CommandLine);

  CHECK(std::find(output.begin(), output.end(), "--stream-max-connections=4") !=
        output.end());
  CHECK(std::find(output.begin(), output.end(), current) != output.end());
  CHECK(std::find(output.begin(), output.end(), uri) != output.end());
  CHECK(std::none_of(output.begin(), output.end(), [](const auto& item) {
    return item.find("enable-http-pipelining") != std::string::npos ||
           item.find("async-dns") != std::string::npos;
  }));
  CHECK_EQ("--ftp-passwd=******", std::string(password));
}

TEST_CASE("LegacyInputAdapter projects only meaningful current values")
{
  Option option;
  option.put(PREF_STREAM_MAX_CONNECTIONS, "6");
  option.put(PREF_STATE_SAVE_INTERVAL, "30");
  option.put(PREF_BT_ENCRYPTION, V_REQUIRED);

  std::string value;
  CHECK(projectLegacyOption(&option, "split", value));
  CHECK_EQ("6", value);
  CHECK(projectLegacyOption(&option, "auto-save-interval", value));
  CHECK_EQ("30", value);
  CHECK(projectLegacyOption(&option, "bt-force-encryption", value));
  CHECK_EQ("true", value);
  CHECK_FALSE(projectLegacyOption(&option, "ftp-user", value));
  CHECK_FALSE(isLegacyInputOption("definitely-not-an-aria2-option"));
}

TEST_CASE("LegacyInputAdapter accepts yt-dlp resume input without changing file policy")
{
  char executable[] = "aria2-next";
  char resume[] = "--always-resume=false";
  char overwrite[] = "--allow-overwrite=true";
  char rename[] = "--auto-file-renaming=false";
  char uri[] = "https://example.com/video.mp4";
  char* argv[]{executable, resume, overwrite, rename, uri};
  const auto output = normalizeLegacyCommandLine(
      static_cast<int>(std::size(argv)), argv, LegacyInputSource::CommandLine);
  CHECK_EQ((std::vector<std::string>{executable, overwrite, rename, uri}), output);

  for (const auto source : {LegacyInputSource::Configuration,
                            LegacyInputSource::Rpc}) {
    for (const auto* value : {"true", "false"}) {
      const auto options = normalizeLegacyInput(
          {{"always-resume", value}, {"allow-overwrite", "false"}}, source);
      REQUIRE_EQ(1, options.size());
      CHECK_EQ("false", valueFor(options, "allow-overwrite"));
    }
    CHECK_THROWS(normalizeLegacyInput({{"always-resume", "invalid"}}, source));
  }
}

TEST_CASE("LegacyInputAdapter is shared by configuration and task input")
{
  auto parser = OptionParser::getInstance();
  Option defaults;
  parser->parseDefaultValues(defaults);
  CHECK_EQ("6", defaults.get(PREF_STREAM_MAX_CONNECTIONS));

  Option configuration;
  std::istringstream configurationInput(
      "split=8\nmax-connection-per-server=3\nftp-user=anonymous\n");
  parser->parse(configuration, configurationInput);
  CHECK_EQ("3", configuration.get(PREF_STREAM_MAX_CONNECTIONS));

  Option task;
  std::istringstream taskInput(
      "auto-save-interval=12\nenable-http-pipelining=true\n");
  parser->parseInternal(task, taskInput);
  CHECK_EQ("12", task.get(PREF_STATE_SAVE_INTERVAL));
}

} // namespace aria2
