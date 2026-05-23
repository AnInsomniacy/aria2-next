/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 aria2-next contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* copyright --> */
#include "LibtorrentProgressInfoFile.h"

#include "BufferedFile.h"
#include "DlAbortEx.h"
#include "DownloadContext.h"
#include "File.h"
#include "LibtorrentAttribute.h"
#include "LogFactory.h"
#include "SHA1IOFile.h"
#include "a2io.h"
#include "fmt.h"
#include "message.h"
#include "util.h"

#include <cstring>

namespace aria2 {

namespace {
constexpr unsigned char FORMAT_VERSION[] = {0x4cu, 0x54u, 0x00u, 0x01u};

std::string createFilename(const std::shared_ptr<DownloadContext>& dctx)
{
  return dctx->getBasePath() + ".aria2";
}

} // namespace

#define WRITE_CHECK(fp, ptr, count)                                            \
  if (fp.write((ptr), (count)) != (count)) {                                   \
    throw DL_ABORT_EX(fmt(EX_SEGMENT_FILE_WRITE, filename_.c_str()));          \
  }

#define READ_CHECK(fp, ptr, count)                                             \
  if (fp.read((ptr), (count)) != (count)) {                                    \
    throw DL_ABORT_EX(fmt(EX_SEGMENT_FILE_READ, filename_.c_str()));           \
  }

LibtorrentProgressInfoFile::LibtorrentProgressInfoFile(
    const std::shared_ptr<DownloadContext>& dctx)
    : dctx_(dctx), filename_(createFilename(dctx_))
{
}

LibtorrentProgressInfoFile::~LibtorrentProgressInfoFile() = default;

void LibtorrentProgressInfoFile::updateFilename()
{
  filename_ = createFilename(dctx_);
}

void LibtorrentProgressInfoFile::save(IOFile& fp)
{
  auto attrs = getLibtorrentAttrs(dctx_);
  WRITE_CHECK(fp, FORMAT_VERSION, sizeof(FORMAT_VERSION));
  auto data = attrs->getResumeData();
  uint64_t len = hton64(data.size());
  WRITE_CHECK(fp, &len, sizeof(len));
  if (!data.empty()) {
    WRITE_CHECK(fp, data.data(), data.size());
  }
  if (fp.close() == EOF) {
    throw DL_ABORT_EX(fmt(EX_SEGMENT_FILE_WRITE, filename_.c_str()));
  }
}

void LibtorrentProgressInfoFile::save()
{
  SHA1IOFile sha1io;
  save(sha1io);

  auto digest = sha1io.digest();
  if (digest == lastDigest_) {
    return;
  }
  lastDigest_ = std::move(digest);

  A2_LOG_INFO(fmt(MSG_SAVING_SEGMENT_FILE, filename_.c_str()));
  auto filenameTemp = filename_ + "__temp";
  {
    BufferedFile fp(filenameTemp.c_str(), BufferedFile::WRITE);
    if (!fp) {
      throw DL_ABORT_EX(fmt(EX_SEGMENT_FILE_WRITE, filename_.c_str()));
    }
    save(fp);
  }
  A2_LOG_INFO(MSG_SAVED_SEGMENT_FILE);
  if (!File(filenameTemp).renameTo(filename_)) {
    throw DL_ABORT_EX(fmt(EX_SEGMENT_FILE_WRITE, filename_.c_str()));
  }
}

void LibtorrentProgressInfoFile::load()
{
  A2_LOG_INFO(fmt(MSG_LOADING_SEGMENT_FILE, filename_.c_str()));
  BufferedFile fp(filename_.c_str(), BufferedFile::READ);
  if (!fp) {
    throw DL_ABORT_EX(fmt(EX_SEGMENT_FILE_READ, filename_.c_str()));
  }

  unsigned char version[sizeof(FORMAT_VERSION)];
  READ_CHECK(fp, version, sizeof(version));
  if (std::memcmp(version, FORMAT_VERSION, sizeof(FORMAT_VERSION)) != 0) {
    throw DL_ABORT_EX(
        fmt("Unsupported libtorrent control file version: %s",
            util::toHex(version, sizeof(version)).c_str()));
  }

  uint64_t len;
  READ_CHECK(fp, &len, sizeof(len));
  len = ntoh64(len);
  if (len > static_cast<uint64_t>(1_g)) {
    throw DL_ABORT_EX("libtorrent resume data is too large");
  }
  std::string data;
  data.resize(static_cast<size_t>(len));
  if (!data.empty()) {
    READ_CHECK(fp, &data[0], data.size());
  }
  getLibtorrentAttrs(dctx_)->setResumeData(std::move(data));
  A2_LOG_INFO(MSG_LOADED_SEGMENT_FILE);
}

void LibtorrentProgressInfoFile::removeFile()
{
  if (exists()) {
    File(filename_).remove();
  }
}

bool LibtorrentProgressInfoFile::exists()
{
  File f(filename_);
  if (f.isFile()) {
    A2_LOG_INFO(fmt(MSG_SEGMENT_FILE_EXISTS, filename_.c_str()));
    return true;
  }
  A2_LOG_INFO(fmt(MSG_SEGMENT_FILE_DOES_NOT_EXIST, filename_.c_str()));
  return false;
}

} // namespace aria2
