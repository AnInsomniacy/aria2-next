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

#include <algorithm>
#include <cstring>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/read_resume_data.hpp>

namespace aria2 {

namespace {
constexpr unsigned char FORMAT_VERSION[] = {0x4cu, 0x54u, 0x00u, 0x01u};

std::string createFilename(const std::shared_ptr<DownloadContext>& dctx)
{
  auto attrs = getLibtorrentAttrs(dctx);
  return attrs->controlFilePath;
}

std::string getV1InfoHash(const libtorrent::add_torrent_params& params)
{
  if (params.info_hashes.has_v1()) {
    return params.info_hashes.v1.to_string();
  }
  if (params.ti) {
    auto hashes = params.ti->info_hashes();
    if (hashes.has_v1()) {
      return hashes.v1.to_string();
    }
  }
  return {};
}

std::string bitfieldBytes(
    const libtorrent::typed_bitfield<libtorrent::piece_index_t>& pieces)
{
  std::string bitfield;
  if (pieces.empty()) {
    return bitfield;
  }
  bitfield.resize(pieces.num_bytes());
  std::memcpy(&bitfield[0], pieces.data(), bitfield.size());
  return bitfield;
}

int64_t calculateWantedLength(const libtorrent::add_torrent_params& params)
{
  if (!params.ti) {
    return 0;
  }
  const auto& files = params.ti->files();
  int64_t total = 0;
  for (auto i = 0; i < files.num_files(); ++i) {
    if (!params.file_priorities.empty() &&
        static_cast<size_t>(i) < params.file_priorities.size() &&
        params.file_priorities[static_cast<size_t>(i)] ==
            libtorrent::dont_download) {
      continue;
    }
    total += files.file_size(libtorrent::file_index_t(i));
  }
  return total;
}

int64_t calculateCompletedLength(const libtorrent::add_torrent_params& params)
{
  if (!params.ti || params.have_pieces.empty()) {
    return 0;
  }
  const auto& files = params.ti->files();
  const auto pieceLength = params.ti->piece_length();
  int64_t total = 0;
  for (auto i = 0; i < files.num_files(); ++i) {
    if (!params.file_priorities.empty() &&
        static_cast<size_t>(i) < params.file_priorities.size() &&
        params.file_priorities[static_cast<size_t>(i)] ==
            libtorrent::dont_download) {
      continue;
    }
    const auto file = libtorrent::file_index_t(i);
    const auto fileBegin = files.file_offset(file);
    const auto fileEnd = fileBegin + files.file_size(file);
    if (fileEnd <= fileBegin || pieceLength <= 0) {
      continue;
    }
    const auto firstPiece = static_cast<int>(fileBegin / pieceLength);
    const auto lastPiece = static_cast<int>((fileEnd - 1) / pieceLength);
    for (auto pieceIndex = firstPiece; pieceIndex <= lastPiece; ++pieceIndex) {
      auto piece = libtorrent::piece_index_t(pieceIndex);
      if (params.have_pieces[piece]) {
        const auto pieceBegin = static_cast<int64_t>(pieceIndex) * pieceLength;
        const auto pieceEnd = pieceBegin + params.ti->piece_size(piece);
        total += std::max<int64_t>(
            0, std::min(fileEnd, pieceEnd) - std::max(fileBegin, pieceBegin));
      }
    }
  }
  auto wanted = calculateWantedLength(params);
  return std::min(total, wanted);
}

void storeResumeStatus(LibtorrentAttribute* attrs, const std::string& data)
{
  libtorrent::error_code ec;
  auto params = libtorrent::read_resume_data(
      libtorrent::span<char const>(data.data(), static_cast<int>(data.size())),
      ec);
  if (ec) {
    return;
  }
  if (!attrs->infoHash.empty() &&
      getV1InfoHash(params) != attrs->infoHash) {
    return;
  }
  auto& status = attrs->resumeStatus;
  status.hasStatus = true;
  status.hasMetadata = bool(params.ti);
  status.totalLength = calculateWantedLength(params);
  status.completedLength = calculateCompletedLength(params);
  status.bitfield = bitfieldBytes(params.have_pieces);
  status.infoHash = getV1InfoHash(params);
  if (params.ti) {
    status.name = params.ti->name();
  }
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
  auto attrs = getLibtorrentAttrs(dctx_);
  if (!attrs->hasResumeData()) {
    A2_LOG_INFO(fmt("Skipping empty libtorrent control file: %s",
                    filename_.c_str()));
    return;
  }

  SHA1IOFile sha1io;
  save(sha1io);

  auto digest = sha1io.digest();
  if (digest == lastDigest_) {
    return;
  }
  lastDigest_ = std::move(digest);

  A2_LOG_INFO(fmt(MSG_SAVING_SEGMENT_FILE, filename_.c_str()));
  File(File(filename_).getDirname()).mkdirs();
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
  auto attrs = getLibtorrentAttrs(dctx_);
  storeResumeStatus(attrs, data);
  if (attrs->sourceType == LibtorrentAttribute::SourceType::MAGNET &&
      attrs->pauseAfterMetadata && attrs->resumeStatus.hasMetadata) {
    attrs->metadataPauseApplied = true;
  }
  attrs->setResumeData(std::move(data));
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
