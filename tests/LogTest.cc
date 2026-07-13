#include "Log.h"

#include "a2doctest.h"

#include "BufferedFile.h"
#include "File.h"

namespace aria2 {

class LogTest {

public:
  void setUp();
  void tearDown();

  void testRotationKeepsStrictBounds();
  void testStartupConvergesLegacyLogs();
  void testOversizedRecordIsBounded();

private:
  std::string path_;
  logging::Settings originalSettings_;

  logging::Settings settings(size_t maxSize, size_t maxFiles) const;
  void removeLogs();
  void writeFile(const std::string& path, size_t size);
};

A2_TEST(LogTest, testRotationKeepsStrictBounds)
A2_TEST(LogTest, testStartupConvergesLegacyLogs)
A2_TEST(LogTest, testOversizedRecordIsBounded)

void LogTest::setUp()
{
  path_ = A2_TEST_OUT_DIR "/aria2_LogTest.log";
  originalSettings_ = logging::getSettings();
  logging::shutdown();
  removeLogs();
}

void LogTest::tearDown()
{
  logging::shutdown();
  removeLogs();
  logging::configure(originalSettings_);
}

logging::Settings LogTest::settings(size_t maxSize, size_t maxFiles) const
{
  logging::Settings settings;
  settings.file = path_;
  settings.maxFileSize = maxSize;
  settings.maxFiles = maxFiles;
  settings.fileLevel = spdlog::level::trace;
  settings.consoleOutput = false;
  return settings;
}

void LogTest::removeLogs()
{
  File(path_).remove();
  for (size_t i = 1; i <= logging::MAX_FILES; ++i) {
    File(path_ + "." + std::to_string(i)).remove();
    File(A2_TEST_OUT_DIR "/aria2_LogTest." + std::to_string(i) + ".log")
        .remove();
  }
}

void LogTest::writeFile(const std::string& path, size_t size)
{
  BufferedFile file(path.c_str(), BufferedFile::WRITE);
  REQUIRE(file);
  const std::string data(size, 'x');
  REQUIRE_EQ(size, file.write(data.data(), data.size()));
}

void LogTest::testRotationKeepsStrictBounds()
{
  logging::configure(settings(128, 2));
  for (size_t i = 0; i < 12; ++i) {
    A2_LOG_TRACE(std::string(48, static_cast<char>('a' + i)));
  }
  logging::flush();

  const std::string history = A2_TEST_OUT_DIR "/aria2_LogTest.1.log";
  REQUIRE(File(path_).exists());
  REQUIRE(File(history).exists());
  REQUIRE(!File(A2_TEST_OUT_DIR "/aria2_LogTest.2.log").exists());
  REQUIRE(File(path_).size() <= 128);
  REQUIRE(File(history).size() <= 128);
  REQUIRE(File(path_).size() + File(history).size() <= 256);
}

void LogTest::testStartupConvergesLegacyLogs()
{
  const std::string nativeHistory =
      A2_TEST_OUT_DIR "/aria2_LogTest.1.log";
  writeFile(path_, 256);
  writeFile(path_ + ".1", 256);
  writeFile(nativeHistory, 256);
  writeFile(A2_TEST_OUT_DIR "/aria2_LogTest.2.log", 8);

  logging::configure(settings(64, 2));
  logging::flush();

  REQUIRE(File(path_).exists());
  REQUIRE_EQ((int64_t)0, File(path_).size());
  REQUIRE(!File(path_ + ".1").exists());
  REQUIRE(!File(nativeHistory).exists());
  REQUIRE(!File(A2_TEST_OUT_DIR "/aria2_LogTest.2.log").exists());
}

void LogTest::testOversizedRecordIsBounded()
{
  logging::configure(settings(96, 1));
  A2_LOG_ERROR(std::string(4096, 'x'));
  logging::flush();

  REQUIRE(File(path_).exists());
  REQUIRE(File(path_).size() <= 96);
  REQUIRE(!File(A2_TEST_OUT_DIR "/aria2_LogTest.1.log").exists());
}

} // namespace aria2
