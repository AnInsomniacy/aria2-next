/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "media/DashFileIo.h"
#include "media/MediaTransport.h"
#include <cstdint>
#include <curl/curl.h>
#include <exception>
#include <gpac/dash.h>
#include <gpac/setup.h>
#include <gpac/tools.h>
#include <string>
#include "media/MediaJob.h"
#include "media/MediaFiles.h"
#include <algorithm>

namespace aria2::media {

namespace {

struct Io {
  MediaJob* job;
  std::string url;
  int group;
  int64_t begin = 0, end = -1;
  bool fetched = false;
  Resource resource;
  GF_Err result = GF_OK;
};
} // namespace

DashFileIo::DashFileIo(MediaJob& job)
{
  native_.udta = &job;
  native_.create = [](GF_DASHFileIO* io, Bool, const char* url,
                      s32 group) -> void* {
    try {
      return new Io{static_cast<MediaJob*>(io->udta), url ? url : "", group};
    }
    catch (...) {
      return nullptr;
    }
  };
  native_.del = [](GF_DASHFileIO*, void* handle) {
    delete static_cast<Io*>(handle);
  };
  native_.abort = [](GF_DASHFileIO*, void*) {};
  native_.delete_cache_file = [](GF_DASHFileIO*, void*, const char*) {};
  native_.setup_from_url = [](GF_DASHFileIO*, void* handle, const char* url,
                              s32 group) -> GF_Err {
    auto& h = *static_cast<Io*>(handle);
    try {
      h.url = url ? url : "";
      h.group = group;
      h.fetched = false;
      h.begin = 0;
      h.end = -1;
      h.result = GF_OK;
      h.resource = {};
      return GF_OK;
    }
    catch (...) {
      return GF_OUT_OF_MEM;
    }
  };
  native_.set_range = [](GF_DASHFileIO*, void* handle, u64 first, u64 last,
                         Bool) -> GF_Err {
    if (first > static_cast<u64>(INT64_MAX) ||
        last > static_cast<u64>(INT64_MAX) || last < first)
      return GF_BAD_PARAM;
    auto& h = *static_cast<Io*>(handle);
    h.begin = first;
    h.end = last;
    h.fetched = false;
    return GF_OK;
  };
  native_.init = [](GF_DASHFileIO*, void* handle) -> GF_Err {
    auto& h = *static_cast<Io*>(handle);
    if (h.job->awaiting || h.job->control->cancel)
      return GF_SERVICE_ERROR;
    if (h.fetched)
      return h.result;
    try {
      h.resource = h.job->transport.get(h.url, h.begin, h.end,
                                        h.group >= 0 && !h.job->live);
      h.fetched = true;
      h.result = GF_OK;
    }
    catch (const std::exception& e) {
      const auto http = dynamic_cast<const HttpError*>(&e);
      if (http && http->status == 404 && h.job->live && h.group >= 0) {
        h.result = GF_URL_ERROR;
        return h.result;
      }
      h.job->failure = failureMessage(e);
      h.job->failureKind = failureCode(e);
      h.result = GF_IO_ERR;
    }
    return h.result;
  };
  native_.run = native_.init;
  native_.get_status = [](GF_DASHFileIO*, void* handle) -> GF_Err {
    return static_cast<Io*>(handle)->result;
  };
  native_.get_url = [](GF_DASHFileIO*, void* handle) -> const char* {
    auto& h = *static_cast<Io*>(handle);
    return h.resource.url.empty() ? h.url.c_str() : h.resource.url.c_str();
  };
  native_.get_cache_name = [](GF_DASHFileIO*, void* handle) -> const char* {
    auto& h = *static_cast<Io*>(handle);
    return h.resource.path.empty() ? nullptr : h.resource.path.c_str();
  };
  native_.get_mime = [](GF_DASHFileIO*, void* handle) -> const char* {
    return static_cast<Io*>(handle)->resource.mime.c_str();
  };
  native_.get_header_value = [](GF_DASHFileIO*, void* handle,
                                const char* name) -> const char* {
    for (const auto& header : static_cast<Io*>(handle)->resource.headers)
      if (curl_strequal(name, header.first.c_str()))
        return header.second.c_str();
    return nullptr;
  };
  native_.manifest_updated = [](GF_DASHFileIO* io, const char* name,
                                const char* path, s32 group) {
    auto& job = *static_cast<MediaJob*>(io->udta);
    try {
      job.manifestUpdated(name, path, group);
    }
    catch (const std::exception& error) {
      job.failure = failureMessage(error);
      job.failureKind = failureCode(error);
    }
  };
  native_.get_utc_start_time = [](GF_DASHFileIO*, void* handle) -> u64 {
    return static_cast<Io*>(handle)->resource.utcStart;
  };
  native_.get_total_size = [](GF_DASHFileIO*, void* handle) -> u32 {
    return static_cast<u32>(
        std::min<int64_t>(UINT32_MAX, static_cast<Io*>(handle)->resource.size));
  };
  native_.get_bytes_done = native_.get_total_size;
  native_.get_bytes_per_sec = [](GF_DASHFileIO*, void*) -> u32 { return 0; };
  native_.on_dash_event = [](GF_DASHFileIO* io, GF_DASHEventType event,
                             s32 group, GF_Err error) -> GF_Err {
    auto& job = *static_cast<MediaJob*>(io->udta);
    try {
      return job.event(event, group, error);
    }
    catch (const std::exception& e) {
      job.failure = failureMessage(e);
      job.failureKind = failureCode(e);
      return GF_IO_ERR;
    }
    catch (...) {
      job.failure = "Media client callback failed";
      return GF_IO_ERR;
    }
  };
}

} // namespace aria2::media
