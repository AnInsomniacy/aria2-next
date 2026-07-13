#include "OptionParser.h"

#include <cstring>
#include <sstream>

#include "a2doctest.h"

#include "OptionHandlerImpl.h"
#include "Exception.h"
#include "util.h"
#include "Option.h"
#include "array_fun.h"
#include "prefs.h"
#include "help_tags.h"

namespace aria2 {

class OptionParserTest {


private:
  std::shared_ptr<OptionParser> oparser_;

public:
  void setUp()
  {
    oparser_.reset(new OptionParser());

    OptionHandler* timeout(
        new DefaultOptionHandler(PREF_TIMEOUT, NO_DESCRIPTION, "ALPHA", "",
                                 OptionHandler::REQ_ARG, 'A'));
    timeout->addTag(TAG_BASIC);
    timeout->setEraseAfterParse(true);
    oparser_->addOptionHandler(timeout);

    OptionHandler* dir(new DefaultOptionHandler(PREF_DIR));
    dir->addTag(TAG_BASIC);
    dir->addTag(TAG_HTTP);
    dir->addTag(TAG_FILE);
    oparser_->addOptionHandler(dir);

    DefaultOptionHandler* daemon(
        new DefaultOptionHandler(PREF_DAEMON, NO_DESCRIPTION, "CHARLIE", "",
                                 OptionHandler::REQ_ARG, 'C'));
    daemon->hide();
    daemon->addTag(TAG_FILE);
    oparser_->addOptionHandler(daemon);

    OptionHandler* out(new UnitNumberOptionHandler(PREF_OUT, NO_DESCRIPTION,
                                                   "1M", -1, -1, 'D'));
    out->addTag(TAG_FILE);
    oparser_->addOptionHandler(out);
  }

  void tearDown() {}

  void testFindAll();
  void testFindByNameSubstring();
  void testFindByTag();
  void testFind();
  void testFindByShortName();
  void testFindById();
  void testParseDefaultValues();
  void testParseDefaultValuesDoesNotInjectCompileTimeCABundle();
  void testLogRotationOptions();
  void testP2PSharingOptionsAreNotBtOnly();
  void testParseArg();
  void testParse();
  void testParseInternal();
  void testParseKeyVals();
};

A2_TEST(OptionParserTest, testFindAll)
A2_TEST(OptionParserTest, testFindByNameSubstring)
A2_TEST(OptionParserTest, testFindByTag)
A2_TEST(OptionParserTest, testFind)
A2_TEST(OptionParserTest, testFindByShortName)
A2_TEST(OptionParserTest, testFindById)
A2_TEST(OptionParserTest, testParseDefaultValues)
A2_TEST(OptionParserTest, testParseDefaultValuesDoesNotInjectCompileTimeCABundle)
A2_TEST(OptionParserTest, testLogRotationOptions)
A2_TEST(OptionParserTest, testP2PSharingOptionsAreNotBtOnly)
A2_TEST(OptionParserTest, testParseArg)
A2_TEST(OptionParserTest, testParse)
A2_TEST(OptionParserTest, testParseInternal)
A2_TEST(OptionParserTest, testParseKeyVals)

void OptionParserTest::testFindAll()
{
  std::vector<const OptionHandler*> res = oparser_->findAll();
  REQUIRE_EQ((size_t)3, res.size());
  REQUIRE_EQ(std::string("timeout"), std::string(res[0]->getName()));
  REQUIRE_EQ(std::string("dir"), std::string(res[1]->getName()));
  REQUIRE_EQ(std::string("out"), std::string(res[2]->getName()));
}

void OptionParserTest::testFindByNameSubstring()
{
  std::vector<const OptionHandler*> res = oparser_->findByNameSubstring("i");
  REQUIRE_EQ((size_t)2, res.size());
  REQUIRE_EQ(std::string("timeout"), std::string(res[0]->getName()));
  REQUIRE_EQ(std::string("dir"), std::string(res[1]->getName()));
}

void OptionParserTest::testFindByTag()
{
  std::vector<const OptionHandler*> res = oparser_->findByTag(TAG_FILE);
  REQUIRE_EQ((size_t)2, res.size());
  REQUIRE_EQ(std::string("dir"), std::string(res[0]->getName()));
  REQUIRE_EQ(std::string("out"), std::string(res[1]->getName()));
}

void OptionParserTest::testFind()
{
  const OptionHandler* dir = oparser_->find(PREF_DIR);
  REQUIRE(dir);
  REQUIRE_EQ(std::string("dir"), std::string(dir->getName()));

  const OptionHandler* daemon = oparser_->find(PREF_DAEMON);
  REQUIRE(!daemon);

  const OptionHandler* log = oparser_->find(PREF_LOG);
  REQUIRE(!log);
}

void OptionParserTest::testFindByShortName()
{
  const OptionHandler* timeout = oparser_->findByShortName('A');
  REQUIRE(timeout);
  REQUIRE_EQ(std::string("timeout"), std::string(timeout->getName()));

  REQUIRE(!oparser_->findByShortName('C'));
}

void OptionParserTest::testFindById()
{
  const OptionHandler* timeout = oparser_->findById(PREF_TIMEOUT->i);
  REQUIRE(timeout);
  REQUIRE_EQ(std::string("timeout"), std::string(timeout->getName()));

  REQUIRE(!oparser_->findById(9999));
}

void OptionParserTest::testParseDefaultValues()
{
  Option option;
  oparser_->parseDefaultValues(option);
  REQUIRE_EQ(std::string("ALPHA"), option.get(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("1048576"), option.get(PREF_OUT));
  REQUIRE_EQ(std::string("CHARLIE"), option.get(PREF_DAEMON));
  REQUIRE(!option.defined(PREF_DIR));
}

void OptionParserTest::testParseDefaultValuesDoesNotInjectCompileTimeCABundle()
{
  Option option;
  OptionParser::getInstance()->parseDefaultValues(option);

  REQUIRE(!option.defined(PREF_CA_CERTIFICATE));
}

void OptionParserTest::testLogRotationOptions()
{
  auto parser = OptionParser::getInstance();

  Option defaults;
  parser->parseDefaultValues(defaults);
  REQUIRE_EQ((int64_t)10_m,
                       defaults.getAsLLInt(PREF_LOG_MAX_SIZE));
  REQUIRE_EQ(4, defaults.getAsInt(PREF_LOG_MAX_FILES));
  REQUIRE_EQ(V_TRACE, defaults.get(PREF_LOG_LEVEL));
  REQUIRE_EQ(V_INFO, defaults.get(PREF_CONSOLE_LOG_LEVEL));

  Option configured;
  std::stringstream input;
  input << "log-max-size=20M\n";
  input << "log-max-files=6\n";
  parser->parse(configured, input);
  REQUIRE_EQ((int64_t)20_m,
                       configured.getAsLLInt(PREF_LOG_MAX_SIZE));
  REQUIRE_EQ(6, configured.getAsInt(PREF_LOG_MAX_FILES));

  try {
    parser->find(PREF_LOG_MAX_FILES)->parse(configured, "0");
    FAIL("zero log file count must be rejected");
  }
  catch (Exception&) {
  }

  try {
    parser->find(PREF_LOG_LEVEL)->parse(configured, "notice");
    FAIL("notice log level must be rejected");
  }
  catch (Exception&) {
  }
}

void OptionParserTest::testP2PSharingOptionsAreNotBtOnly()
{
  auto parser = OptionParser::getInstance();
  const auto seedRatio = parser->find(PREF_SEED_RATIO);
  const auto seedTime = parser->find(PREF_SEED_TIME);
  const auto detachShareOnly = parser->find(PREF_DETACH_SHARE_ONLY);
  const auto oldBtDetachSeedOnly = option::k2p("bt-detach-seed-only");

  REQUIRE(seedRatio);
  REQUIRE(seedRatio->hasTag(TAG_BITTORRENT));
  REQUIRE(seedRatio->hasTag(TAG_ED2K));
  REQUIRE(seedTime);
  REQUIRE(seedTime->hasTag(TAG_BITTORRENT));
  REQUIRE(seedTime->hasTag(TAG_ED2K));
  REQUIRE(detachShareOnly);
  REQUIRE(detachShareOnly->hasTag(TAG_BITTORRENT));
  REQUIRE(detachShareOnly->hasTag(TAG_ED2K));
  REQUIRE_EQ((size_t)0, oldBtDetachSeedOnly->i);
}

void OptionParserTest::testParseArg()
{
  Option option;
  char prog[7];
  strncpy(prog, "aria2c", sizeof(prog));

  char optionTimeout[3];
  strncpy(optionTimeout, "-A", sizeof(optionTimeout));
  char argTimeout[6];
  strncpy(argTimeout, "ALPHA", sizeof(argTimeout));
  char optionDir[8];
  strncpy(optionDir, "--dir", sizeof(optionDir));
  char argDir[6];
  strncpy(argDir, "BRAVO", sizeof(argDir));

  char nonopt1[8];
  strncpy(nonopt1, "nonopt1", sizeof(nonopt1));
  char nonopt2[8];
  strncpy(nonopt2, "nonopt2", sizeof(nonopt2));

  char* argv[] = {prog,   optionTimeout, argTimeout, optionDir,
                  argDir, nonopt1,       nonopt2};
  int argc = arraySize(argv);

  std::stringstream s;
  std::vector<std::string> nonopts;

  oparser_->parseArg(s, nonopts, argc, argv);

  REQUIRE_EQ(std::string("timeout=ALPHA\n"
                                   "dir=BRAVO\n"),
                       s.str());

  REQUIRE_EQ((size_t)2, nonopts.size());
  REQUIRE_EQ(std::string("nonopt1"), nonopts[0]);
  REQUIRE_EQ(std::string("nonopt2"), nonopts[1]);

  REQUIRE_EQ(std::string("*****"), std::string(argTimeout));
}

void OptionParserTest::testParse()
{
  Option option;
  std::istringstream in("timeout=Hello\n"
                        "UNKNOWN=x\n"
                        "\n"
                        "dir=World");
  oparser_->parse(option, in);
  REQUIRE_EQ(std::string("Hello"), option.get(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("World"), option.get(PREF_DIR));
}

void OptionParserTest::testParseInternal()
{
  Option option;
  std::istringstream in("daemon=true\n"
                        "timeout=Hello\n");
  oparser_->parseInternal(option, in);
  REQUIRE_EQ(std::string("true"), option.get(PREF_DAEMON));
  REQUIRE_EQ(std::string("Hello"), option.get(PREF_TIMEOUT));
}

void OptionParserTest::testParseKeyVals()
{
  Option option;
  KeyVals kv;
  kv.push_back(std::make_pair("timeout", "Hello"));
  kv.push_back(std::make_pair("UNKNOWN", "x"));
  kv.push_back(std::make_pair("dir", "World"));
  oparser_->parse(option, kv);
  REQUIRE_EQ(std::string("Hello"), option.get(PREF_TIMEOUT));
  REQUIRE_EQ(std::string("World"), option.get(PREF_DIR));
}

} // namespace aria2
