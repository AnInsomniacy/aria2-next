/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "media/MediaStore.h"
#include "media/MediaTransport.h"
#include <cstdint>
#include <gpac/dash.h>
#include <gpac/setup.h>
#include <gpac/tools.h>
#include <ios>
#include <optional>
#include <stdexcept>
#include <string>
#include "media/MediaJob.h"
#include "media/MediaFiles.h"
#include "media/MediaMuxer.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>

namespace aria2::media {

void MediaJob::remember(const Segment& segment)
{
  const SegmentKey key{segment.period, segment.track, segment.number};
  auto previous = retained.find(key);
  if (previous != retained.end()) {
    coverage[segment.period][segment.track] -= previous->second.duration;
    auto& resource = retainedPaths[previous->second.path];
    if (--resource.first == 0)
      retainedBytes -= resource.second;
  }
  retained[key] = segment;
  coverage[segment.period][segment.track] += segment.duration;
  auto& resource = retainedPaths[segment.path];
  if (resource.first++ == 0) {
    resource.second = segment.bytes;
    retainedBytes += segment.bytes;
  }
  auto& last = lastSegments[{segment.period, segment.track}];
  if (last.track.empty() || last.number <= segment.number)
    last = segment;
}

std::string MediaJob::digest(const std::string& path)
{
  if (path.empty())
    return {};
  auto found = digests.find(path);
  if (found == digests.end()) {
    syncFile(path);
    syncParent(path);
    found = digests.emplace(path, Transport::digest(path)).first;
  }
  return found->second;
}

void MediaJob::commit(Segment segment)
{
  auto last = lastSegments.find({segment.period, segment.track});
  if (live && segment.hls && segment.type != "subtitle" &&
      last == lastSegments.end()) {
    std::optional<int64_t> reference;
    for (const auto& entry : lastSegments)
      if (entry.first.first == segment.period &&
          entry.second.type != "subtitle") {
        reference = entry.second.start;
        break;
      }
    segment.start = Muxer::startTime(segment, control, reference);
  }
  const auto previous =
      retained.find({segment.period, segment.track, segment.number});
  if (last != lastSegments.end()) {
    if ((segment.hls && segment.number > last->second.number + 1) ||
        (!segment.hls &&
         segment.start > last->second.start + last->second.duration + 1))
      throw std::runtime_error("Media segments are missing; refusing to save "
                               "an incomplete presentation");
    if (segment.hls) {
      if (previous != retained.end())
        segment.start = previous->second.start;
      else if (segment.number == last->second.number + 1)
        segment.start = last->second.start + last->second.duration;
      else
        return; // The refreshed live window can include earlier segments.
    }
  }
  segment.bytes = std::filesystem::file_size(nativePath(segment.path));
  segment.digest = digest(segment.path);
  segment.initDigest = digest(segment.init);
  if (live && previous != retained.end() &&
      previous->second.digest != segment.digest)
    throw std::runtime_error(
        "A committed live media segment changed while resuming");
  store.commit(segment);
  remember(segment);
  transport.retain(segment.path);
  transport.retain(segment.init);
}

Segment MediaJob::describe(int group, u32 number, const GF_Fraction64& start,
                           u32 duration, const std::string& path,
                           u32 discontinuity)
{
  auto& selectedGroup = groups.at(group);
  Segment segment;
  segment.period = gf_dash_get_period_start(dash);
  segment.track = selectedGroup.identity;
  segment.number = number;
  segment.start =
      start.den ? gf_timestamp_rescale_signed(start.num, start.den, 1000) : 0;
  segment.duration = duration;
  segment.path = path;
  segment.init = selectedGroup.init;
  segment.type = selectedGroup.type;
  segment.discontinuity = discontinuity;
  segment.timeOffset = selectedGroup.timeOffset;
  segment.hls = gf_dash_is_m3u8(dash);
  if (!segment.hls)
    segment.number = segment.start;
  // GPAC segment positions already include PTO; encoded packet timestamps
  // still need the separate offset when passed to the muxer.
  return segment;
}

std::string MediaJob::localResource(const char* url, int64_t begin, int64_t end)
{
  if (!url || !*url)
    return {};
  std::string value(url);
  if (value.rfind("http://", 0) == 0 || value.rfind("https://", 0) == 0)
    return transport.get(value, begin, end, !live).path;
  const auto path = std::filesystem::weakly_canonical(nativePath(value));
  const auto root =
      std::filesystem::weakly_canonical(nativePath(taskDirectory));
  if (path.parent_path() != root || !std::filesystem::is_regular_file(path))
    throw std::runtime_error(
        "Manifest requested a file outside its media cache");
  if (end >= 0 && std::filesystem::file_size(path) !=
                      static_cast<uint64_t>(end - begin + 1)) {
    if (begin < 0 || end < begin ||
        static_cast<uint64_t>(end) >= std::filesystem::file_size(path))
      throw std::runtime_error(
          "Cached media resource does not contain the required byte range");
    auto slice =
        root / Transport::fingerprint(utf8(path) + ":" + std::to_string(begin) +
                                      ":" + std::to_string(end));
    std::ifstream input(path, std::ios::binary);
    input.seekg(begin);
    std::ofstream output(slice, std::ios::binary | std::ios::trunc);
    std::array<char, 65536> buffer{};
    int64_t remaining = end - begin + 1;
    while (remaining > 0) {
      auto count = static_cast<std::streamsize>(
          std::min<int64_t>(remaining, buffer.size()));
      input.read(buffer.data(), count);
      output.write(buffer.data(), input.gcount());
      if (input.gcount() != count || !output)
        throw std::runtime_error("Cannot materialize media byte range");
      remaining -= count;
    }
    output.close();
    return utf8(slice);
  }
  return utf8(path);
}

void MediaJob::progress()
{
  auto value = snapshot();
  value.downloadedLength = retainedBytes;
  value.completedDuration = 0;
  for (const auto& period : coverage) {
    int64_t minimum = INT64_MAX;
    int64_t beginning = INT64_MIN, end = INT64_MAX;
    for (const auto& track : period.second) {
      minimum = std::min(minimum, track.second);
      const auto last = lastSegments.find({period.first, track.first});
      if (live && value.protocol == "hls" && last != lastSegments.end() &&
          last->second.type != "subtitle") {
        const auto stop = last->second.start + last->second.duration;
        beginning = std::max(beginning, stop - track.second);
        end = std::min(end, stop);
      }
    }
    if (minimum > 0 && beginning != INT64_MIN)
      value.completedDuration += std::max<int64_t>(0, end - beginning);
    else if (minimum != INT64_MAX)
      value.completedDuration += minimum;
  }
  publish(value);
}

} // namespace aria2::media
