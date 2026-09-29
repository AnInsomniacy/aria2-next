#include "stream/AsyncDiskWriter.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "DiskWriter.h"
#include "File.h"
#include "a2doctest.h"

namespace aria2 {

namespace {

// Records every write it receives, and can be made slow to simulate a device
// that cannot keep up.
class RecordingWriter : public DiskWriter {
public:
  explicit RecordingWriter(std::chrono::milliseconds delay = {})
      : delay_(delay)
  {
  }

  void initAndOpenFile(int64_t) override {}
  void openFile(int64_t) override {}
  void closeFile() override { ++closes_; }
  void openExistingFile(int64_t) override {}
  void writeData(const unsigned char* data, size_t len, int64_t offset) override
  {
    if (delay_.count() > 0) {
      std::this_thread::sleep_for(delay_);
    }
    payload_.insert(payload_.end(), data, data + len);
    offsets_.push_back(offset);
    ++writes_;
  }
  ssize_t readData(unsigned char*, size_t, int64_t) override { return 0; }
  int64_t size() override { return 0; }
  void truncate(int64_t) override {}
  void allocate(int64_t, int64_t, bool) override {}
  bool isFileBacked() const override { return fileBacked_; }

  void setFileBacked(bool value) { fileBacked_ = value; }

  size_t writes() const { return writes_; }
  int closes() const { return closes_; }
  const std::vector<unsigned char>& payload() const { return payload_; }
  const std::vector<int64_t>& offsets() const { return offsets_; }

private:
  std::chrono::milliseconds delay_;
  bool fileBacked_ = true;
  std::atomic<size_t> writes_{0};
  std::atomic<int> closes_{0};
  std::vector<unsigned char> payload_;
  std::vector<int64_t> offsets_;
};

std::vector<unsigned char> bytes(const std::string& s)
{
  return std::vector<unsigned char>(s.begin(), s.end());
}

} // namespace

TEST_CASE("AsyncDiskWriterTest.writesInSubmissionOrderAndFlushDrains")
{
  auto inner = std::unique_ptr<RecordingWriter>(new RecordingWriter());
  auto* raw = inner.get();
  stream::AsyncDiskWriter writer(std::move(inner), 1 << 20);

  const auto first = bytes("alpha");
  const auto second = bytes("bravo");
  const auto third = bytes("charlie");
  writer.writeData(first.data(), first.size(), 0);
  writer.writeData(second.data(), second.size(), 100);
  writer.writeData(third.data(), third.size(), 200);
  writer.flush();

  REQUIRE(raw->writes() == 3);
  // Ordering matters: overlapping ranges must keep last-writer-wins semantics.
  REQUIRE(raw->offsets() == std::vector<int64_t>({0, 100, 200}));
  const std::string expected = "alphabravocharlie";
  REQUIRE(raw->payload() == bytes(expected));
}

TEST_CASE("AsyncDiskWriterTest.closeFileDrainsBeforeClosingInner")
{
  auto inner = std::unique_ptr<RecordingWriter>(new RecordingWriter());
  auto* raw = inner.get();
  stream::AsyncDiskWriter writer(std::move(inner), 1 << 20);

  const auto data = bytes("payload");
  writer.writeData(data.data(), data.size(), 0);
  writer.closeFile();

  // A close must never discard accepted bytes, and must not happen first.
  REQUIRE(raw->writes() == 1);
  REQUIRE(raw->closes() == 1);
  REQUIRE(raw->payload() == data);
}

TEST_CASE("AsyncDiskWriterTest.flushReportsDeviceFailure")
{
  class FailingWriter : public DiskWriter {
  public:
    void initAndOpenFile(int64_t) override {}
    void openFile(int64_t) override {}
    void closeFile() override {}
    void openExistingFile(int64_t) override {}
    void writeData(const unsigned char*, size_t, int64_t) override
    {
      throw std::runtime_error("device write failed");
    }
    ssize_t readData(unsigned char*, size_t, int64_t) override { return 0; }
    int64_t size() override { return 0; }
    bool isFileBacked() const override { return true; }
  };

  auto inner = std::unique_ptr<DiskWriter>(new FailingWriter());
  stream::AsyncDiskWriter writer(std::move(inner), 1 << 20);
  const auto data = bytes("payload");
  writer.writeData(data.data(), data.size(), 0);

  // The worker cannot surface the error through the write call, so it is
  // reported by the next flush - which is what a checkpoint relies on.
  bool threw = false;
  try {
    writer.flush();
  }
  catch (const std::exception&) {
    threw = true;
  }
  REQUIRE(threw);
  REQUIRE(writer.hasWriteFailed());
  REQUIRE_FALSE(writer.hasPendingWrites());
}

TEST_CASE("AsyncDiskWriterTest.backlogIsBoundedAndObservable")
{
  // A slow device plus a small queue must become observable as backlogged
  // rather than growing without bound. writeData itself must never block the
  // caller, which is the whole point of the offload.
  auto inner = std::unique_ptr<RecordingWriter>(
      new RecordingWriter(std::chrono::milliseconds(40)));
  stream::AsyncDiskWriter writer(std::move(inner), 1024);

  const std::vector<unsigned char> chunk(512, 0x5a);
  bool sawBacklog = false;
  for (int i = 0; i < 8 && !sawBacklog; ++i) {
    writer.writeData(chunk.data(), chunk.size(), i * 512);
    sawBacklog = writer.isBacklogged();
  }
  REQUIRE(sawBacklog);
  REQUIRE(writer.pendingBytes() > 0);

  writer.flush();
  REQUIRE_FALSE(writer.hasPendingWrites());
}

TEST_CASE("AsyncDiskWriterTest.wrapSkipsNonFileBackedWriters")
{
  auto memory = std::unique_ptr<RecordingWriter>(new RecordingWriter());
  memory->setFileBacked(false);
  auto* raw = memory.get();
  auto wrapped = stream::wrapAsyncWriter(std::move(memory));

  // In-memory writers cannot stall on a device, so they stay unwrapped and
  // keep writing synchronously.
  REQUIRE(wrapped.get() == static_cast<DiskWriter*>(raw));
  REQUIRE_FALSE(wrapped->isWriteOffloaded());
}

TEST_CASE("AsyncDiskWriterTest.wrapIsIdempotent")
{
  auto wrapped =
      stream::wrapAsyncWriter(std::unique_ptr<DiskWriter>(new RecordingWriter()));
  REQUIRE(wrapped->isWriteOffloaded());
  auto* raw = wrapped.get();
  auto twice = stream::wrapAsyncWriter(std::move(wrapped));
  REQUIRE(twice.get() == raw);
}

} // namespace aria2
