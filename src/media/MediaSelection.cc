/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "options/Options.h"
#include "media/Control.h"
#include "media/MediaTransport.h"
#include <cstdint>
#include <gpac/dash.h>
#include <gpac/setup.h>
#include <gpac/tools.h>
#include <map>
#include <stdexcept>
#include <string>
#include "media/MediaJob.h"
#include "base/Log.h"
#include <gpac/constants.h>
#include <sstream>

namespace aria2::media {

namespace {
std::string trackType(const GF_DASHQualityInfo& info, bool hls)
{
  bool video = false, audio = false, text = false;
  std::istringstream codecs(info.codec ? info.codec : "");
  std::string codec;
  while (std::getline(codecs, codec, ',')) {
    const auto first = codec.find_first_not_of(" \t");
    if (first == std::string::npos)
      continue;
    codec = codec.substr(first, codec.find_first_of(". \t", first) - first);
    auto id = gf_codecid_parse(codec.c_str());
    if (id == GF_CODECID_NONE && codec.size() == 4)
      id = gf_codec_id_from_isobmf(gf_4cc_parse(codec.c_str()));
    const auto type = gf_codecid_type(id);
    video |= type == GF_STREAM_VISUAL;
    audio |= type == GF_STREAM_AUDIO;
    text |= type == GF_STREAM_TEXT;
  }
  if (video || audio || text)
    return video && audio ? "muxed"
           : video        ? "video"
           : audio        ? "audio"
                          : "subtitle";
  const std::string mime = info.mime ? info.mime : "";
  if (info.sample_rate || info.nb_channels || mime.rfind("audio", 0) == 0)
    return "audio";
  if (info.width || mime.rfind("video", 0) == 0)
    return hls && (!info.codec || !*info.codec) ? "muxed" : "video";
  return "subtitle";
}

bool hasAudioRendition(GF_DashClient* dash)
{
  for (u32 group = 0; group < gf_dash_get_group_count(dash); ++group) {
    if (!gf_dash_is_group_selectable(dash, group))
      continue;
    for (u32 quality = 0;
         quality < gf_dash_group_get_num_qualities(dash, group); ++quality) {
      GF_DASHQualityInfo info{};
      if (gf_dash_group_get_quality_info(dash, group, quality, &info) ==
              GF_OK &&
          !info.disabled && trackType(info, true) == "audio")
        return true;
    }
  }
  return false;
}
} // namespace

std::map<std::string, Track> MediaJob::chooseTracks(Snapshot& value)
{
  std::map<std::string, Track> chosen;
  const bool hls = gf_dash_is_m3u8(dash);
  const bool separateAudio = hls && hasAudioRendition(dash);
  trackLocations.clear();
  for (u32 group = 0; group < gf_dash_get_group_count(dash); ++group) {
    gf_dash_group_select(dash, group, GF_FALSE);
    if (!gf_dash_is_group_selectable(dash, group))
      continue;
    const auto language = gf_dash_group_get_language(dash, group);
    for (u32 quality = 0;
         quality < gf_dash_group_get_num_qualities(dash, group); ++quality) {
      GF_DASHQualityInfo info{};
      if (gf_dash_group_get_quality_info(dash, group, quality, &info) !=
              GF_OK ||
          info.disabled)
        continue;
      const std::string mime = info.mime ? info.mime : "";
      auto type = trackType(info, hls);
      // HLS CODECS covers the playable rendition set, including external audio.
      // When GPAC exposes that audio separately, take only video from this
      // playlist; the selected audio rendition supplies the soundtrack.
      if (type == "muxed" && separateAudio)
        type = "video";
      // Native representation metadata survives group/quality reordering.
      // HLS variant URLs identify renditions independently of generated IDs.
      const std::string representation =
          hls ? (info.hls_variant_url ? info.hls_variant_url : uri)
              : std::to_string(gf_dash_group_get_as_id(dash, group)) + ":" +
                    (info.ID ? info.ID : "");
      const auto identity =
          representation + "\n" + type + "\n" + (language ? language : "") +
          "\n" + (info.codec ? info.codec : "") + "\n" +
          std::to_string(info.width) + "x" + std::to_string(info.height) +
          "\n" + std::to_string(info.bandwidth) + "\n" +
          std::to_string(info.sample_rate) + ":" +
          std::to_string(info.nb_channels);
      const auto id = "track-" + Transport::fingerprint(identity);
      if (!trackLocations.emplace(id, std::make_pair(group, quality)).second)
        throw Failure(FailureCode::UnsupportedSource,
                      "Media representations have ambiguous identities");
      Track track{
          id,
          type,
          language ? language : "",
          info.codec ? info.codec : "",
          static_cast<int>(info.width),
          static_cast<int>(info.height),
          info.bandwidth,
          false,
          info.fps_den ? static_cast<double>(info.fps_num) / info.fps_den : 0};
      value.tracks.push_back(track);
      log::debug(
          "media: representation id={} type={} language={} mime={} codec={}",
          track.id, track.type, track.language, mime, track.codec);
      auto pref = type == "video" || (type == "muxed" &&
                                      option->get("media-video") != "none")
                      ? "media-video"
                  : type == "audio" || type == "muxed" ? "media-audio"
                                                       : "media-subtitles";
      const auto& selection = option->get(pref);
      if (selection == "none")
        continue;
      if (selection != "best" && selection != track.id &&
          selection != track.language)
        continue;
      if (!chosen.count(type) || chosen[type].bandwidth < track.bandwidth)
        chosen[type] = track;
    }
  }
  for (const auto& type :
       {std::string("video"), std::string("audio"), std::string("subtitle")}) {
    const auto pref = type == "video"   ? "media-video"
                      : type == "audio" ? "media-audio"
                                        : "media-subtitles";
    const auto& selection = option->get(pref);
    const auto muxed = chosen.find("muxed");
    const bool selectedMuxed = type != "subtitle" && muxed != chosen.end() &&
                               muxed->second.id == selection;
    if (selection != "best" && selection != "none" && !chosen.count(type) &&
        !selectedMuxed)
      throw Failure(FailureCode::UnsupportedSelection,
                    "Requested media track or language is unavailable: " +
                        selection);
  }
  if (chosen.count("video") && chosen.count("muxed"))
    chosen.erase(chosen.at("video").bandwidth >= chosen.at("muxed").bandwidth
                     ? "muxed"
                     : "video");
  if (chosen.count("muxed") && chosen.count("audio")) {
    if (option->get("media-video") == "none" &&
        chosen.at("audio").bandwidth >= chosen.at("muxed").bandwidth)
      chosen.erase("muxed");
    else if (option->get("media-audio") == "best" ||
             option->get("media-audio") == chosen.at("muxed").id)
      chosen.erase("audio");
    else
      throw Failure(FailureCode::UnsupportedSelection,
                    "A multiplexed representation cannot be combined with "
                    "another audio track");
  }
  return chosen;
}

void MediaJob::selectTrack(const Track& track)
{
  const auto [group, quality] = trackLocations.at(track.id);
  const char* descriptor = nullptr;
  if (gf_dash_group_enum_descriptor(dash, group, GF_MPD_DESC_CONTENT_PROTECTION,
                                    0, nullptr, &descriptor, nullptr))
    throw Failure(FailureCode::ProtectedMedia,
                  "DRM-protected media is not supported");
  gf_dash_group_select(dash, group, GF_TRUE);
  if (gf_dash_group_select_quality(dash, group, nullptr, quality) != GF_OK)
    throw Failure(FailureCode::UnsupportedSelection,
                  "Cannot select the requested media quality");
  const auto start = option->integer("media-start-time");
  const auto end = option->integer("media-end-time");
  if (end && end <= start)
    throw Failure(FailureCode::UnsupportedSelection,
                  "Media range must end after its start");
  if (!live && start > 0 && !option->flag("media-pause-after-probe"))
    gf_dash_group_seek(dash, group, start);
  selected.push_back(group);
  const auto& identity = track.id;
  u64 offset = 0;
  u32 timescale = 0;
  if (gf_dash_group_get_presentation_time_offset(dash, group, &offset,
                                                 &timescale) < 0)
    throw std::runtime_error("Cannot read the representation timestamp offset");
  groups[group] = {
      track.type, identity, "",
      static_cast<int64_t>(
          timescale ? gf_timestamp_rescale(offset, timescale, 1000000) : 0)};
  if (!option->flag("media-pause-after-probe")) {
    auto previous =
        lastSegments.find({gf_dash_get_period_start(dash), identity});
    if (live && gf_dash_is_m3u8(dash) && previous != lastSegments.end() &&
        gf_dash_group_resume_sequence(dash, group, previous->second.number) !=
            GF_OK)
      throw std::runtime_error("Live media left the server's retention window; "
                               "the recording has a gap");
    if (live && !gf_dash_is_m3u8(dash) && previous != lastSegments.end() &&
        gf_dash_group_resume_time(
            dash, group, previous->second.start + previous->second.duration) !=
            GF_OK)
      throw std::runtime_error("Cannot restore the DASH recording position");
    store.select(gf_dash_get_period_start(dash), track.type, identity);
    coverage[gf_dash_get_period_start(dash)].try_emplace(identity, 0);
  }
}

GF_Err MediaJob::selectGroups()
{
  selected.clear();
  groups.clear();
  auto value = snapshot();
  value.tracks.clear();
  value.protocol = gf_dash_is_m3u8(dash) ? "hls" : "dash";
  value.live = live || gf_dash_is_dynamic_mpd(dash);
  live = value.live;
  value.duration = static_cast<int64_t>(gf_dash_get_duration(dash) * 1000);
  if (gf_dash_is_smooth_streaming(dash))
    throw std::runtime_error("Smooth Streaming is not supported");
  auto chosen = chooseTracks(value);
  for (auto& track : value.tracks) {
    auto choice = chosen.find(track.type);
    track.selected = choice != chosen.end() && choice->second.id == track.id;
    if (!track.selected)
      continue;
    selectTrack(track);
  }
  if (!chosen.count("video") && !chosen.count("audio") &&
      !chosen.count("muxed") && !chosen.count("subtitle"))
    throw Failure(FailureCode::UnsupportedSelection,
                  "Select at least one audio or video track");
  store.saveTracks(value.tracks);
  if (option->flag("media-pause-after-probe")) {
    for (int group : selected)
      gf_dash_group_select(dash, group, GF_FALSE);
    value.state = "awaiting-selection";
    awaiting = true;
    publish(value);
    return GF_SERVICE_ERROR;
  }
  value.state = value.live ? "recording" : "downloading";
  publish(value);
  return GF_OK;
}

} // namespace aria2::media
