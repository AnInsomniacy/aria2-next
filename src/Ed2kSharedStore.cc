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
#include "Ed2kSharedStore.h"

#include <algorithm>
#include <sstream>

#include "DefaultDiskWriter.h"
#include "DlAbortEx.h"
#include "DiskWriter.h"
#include "DownloadResult.h"
#include "Ed2kAttribute.h"
#include "File.h"
#include "FileEntry.h"
#include "Option.h"
#include "ed2k_hash.h"
#include "error_code.h"
#include "message_digest_helper.h"
#include "prefs.h"
#include "util.h"

namespace aria2 {

namespace ed2k {

namespace {

const Ed2kAttribute* findEd2kAttrs(const DownloadResult& result)
{
  for (const auto& attrs : result.attrs) {
    auto ed2kAttrs = dynamic_cast<const Ed2kAttribute*>(attrs.get());
    if (ed2kAttrs) {
      return ed2kAttrs;
    }
  }
  return nullptr;
}

std::string sharedOriginToString(SharedOrigin origin)
{
  switch (origin) {
  case SharedOrigin::COMPLETED_DOWNLOAD:
    return "completed";
  case SharedOrigin::IMPORTED_FILE:
    return "imported";
  }
  return "completed";
}

bool parseSharedOrigin(SharedOrigin& origin, const std::string& value)
{
  if (value == "completed") {
    origin = SharedOrigin::COMPLETED_DOWNLOAD;
    return true;
  }
  if (value == "imported") {
    origin = SharedOrigin::IMPORTED_FILE;
    return true;
  }
  return false;
}

bool parseInt64(int64_t& result, const std::string& value)
{
  char* end = nullptr;
  errno = 0;
  auto parsed = strtoll(value.c_str(), &end, 10);
  if (errno != 0 || end != value.c_str() + value.size()) {
    return false;
  }
  result = parsed;
  return true;
}

bool parseBool(bool& result, const std::string& value)
{
  if (value == "1") {
    result = true;
    return true;
  }
  if (value == "0") {
    result = false;
    return true;
  }
  return false;
}

bool readRange(DiskWriter* writer, std::string& data, int64_t offset,
               size_t length)
{
  data.assign(length, '\0');
  auto read = writer->readData(reinterpret_cast<unsigned char*>(&data[0]),
                               length, offset);
  return read == static_cast<ssize_t>(length);
}

} // namespace

bool isValidSharedFile(const SharedFile& file)
{
  if (file.hash.size() != HASH_LENGTH || file.path.empty() ||
      file.name.empty() || file.size <= 0) {
    return false;
  }
  File diskFile(file.path);
  return diskFile.isFile() && diskFile.size() == file.size;
}

bool SharedStore::addOrReplace(SharedFile file)
{
  if (!isValidSharedFile(file)) {
    return false;
  }
  auto itr = std::find_if(std::begin(files_), std::end(files_),
                          [&](const SharedFile& item) {
                            return item.hash == file.hash;
                          });
  if (itr == std::end(files_)) {
    files_.push_back(std::move(file));
  }
  else {
    *itr = std::move(file);
  }
  return true;
}

bool SharedStore::addCompletedDownload(const DownloadResult& result,
                                       int64_t now)
{
  if (result.result != error_code::FINISHED || result.fileEntries.size() != 1 ||
      result.inMemoryDownload) {
    return false;
  }
  auto attrs = findEd2kAttrs(result);
  if (!attrs || attrs->link.hash.size() != HASH_LENGTH ||
      attrs->link.size != result.totalLength || attrs->link.size <= 0) {
    return false;
  }
  const auto& fileEntry = result.fileEntries[0];
  if (!fileEntry || fileEntry->getLength() != attrs->link.size) {
    return false;
  }

  SharedFile file;
  file.hash = attrs->link.hash;
  file.aichRootHash = attrs->aichRootHash;
  file.pieceHashes = attrs->pieceHashes;
  file.path = fileEntry->getPath();
  file.name = attrs->link.name.empty() ? fileEntry->getBasename()
                                       : attrs->link.name;
  file.size = attrs->link.size;
  file.lastHashTime = now;
  file.origin = SharedOrigin::COMPLETED_DOWNLOAD;
  file.completed = true;
  return addOrReplace(std::move(file));
}

size_t SharedStore::loadOptionState(const Option* option)
{
  if (!option || option->blank(PREF_ED2K_SHARED_FILE_STATE)) {
    return 0;
  }
  size_t count = 0;
  std::istringstream in(option->get(PREF_ED2K_SHARED_FILE_STATE));
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    SharedFile file;
    if (parseSharedFileStatePayload(file,
                                    util::fromHex(line.begin(), line.end())) &&
        addOrReplace(std::move(file))) {
      ++count;
    }
  }
  return count;
}

size_t SharedStore::importOptionFiles(const Option* option, int64_t now)
{
  if (!option || option->blank(PREF_ED2K_SHARE_FILE)) {
    return 0;
  }
  size_t count = 0;
  std::istringstream in(option->get(PREF_ED2K_SHARE_FILE));
  std::string path;
  while (std::getline(in, path)) {
    if (path.empty()) {
      continue;
    }
    SharedFile file;
    if (importSharedFile(file, path, now) && addOrReplace(std::move(file))) {
      ++count;
    }
  }
  return count;
}

const SharedFile* SharedStore::findByHash(const std::string& hash) const
{
  auto itr = std::find_if(std::begin(files_), std::end(files_),
                          [&](const SharedFile& item) {
                            return item.hash == hash;
                          });
  return itr == std::end(files_) ? nullptr : &*itr;
}

bool importSharedFile(SharedFile& result, const std::string& path, int64_t now)
{
  File file(path);
  if (!file.isFile()) {
    return false;
  }
  auto size = file.size();
  if (size <= 0) {
    return false;
  }

  std::shared_ptr<DiskWriter> writer(new DefaultDiskWriter(path));
  writer->enableReadOnly();
  writer->openExistingFile();

  std::vector<std::string> pieceHashes;
  std::vector<std::string> aichPieceHashes;
  std::string piece;
  for (int64_t offset = 0; offset < size; offset += PIECE_LENGTH) {
    const auto length = static_cast<size_t>(
        std::min<int64_t>(PIECE_LENGTH, size - offset));
    if (!readRange(writer.get(), piece, offset, length)) {
      return false;
    }
    pieceHashes.push_back(md4Digest(piece));
    aichPieceHashes.push_back(aichRootHash(piece.data(), piece.size()));
  }
  if (size >= PIECE_LENGTH && size % PIECE_LENGTH == 0) {
    pieceHashes.push_back(md4Digest(""));
  }

  SharedFile shared;
  shared.hash = pieceHashes.empty() ? md4Digest(piece) : rootHash(pieceHashes);
  shared.aichRootHash = aichRootHash(aichPieceHashes);
  shared.pieceHashes = std::move(pieceHashes);
  shared.path = path;
  shared.name = file.getBasename();
  shared.size = size;
  shared.lastHashTime = now;
  shared.origin = SharedOrigin::IMPORTED_FILE;
  shared.completed = true;
  result = std::move(shared);
  return true;
}

std::string createSharedFileStatePayload(const SharedFile& file)
{
  if (file.hash.size() != HASH_LENGTH || file.size <= 0 || file.path.empty() ||
      file.name.empty()) {
    throw DL_ABORT_EX("Bad ED2K shared file state.");
  }
  std::string payload = "v=1";
  payload += "|hash=" + util::toHex(file.hash);
  payload += "|size=" + util::itos(file.size);
  payload += "|path=" + util::percentEncode(file.path);
  payload += "|name=" + util::percentEncode(file.name);
  payload += "|origin=" + sharedOriginToString(file.origin);
  payload += "|completed=" + std::string(file.completed ? "1" : "0");
  payload += "|lastHashTime=" + util::itos(file.lastHashTime);
  if (!file.pieceHashes.empty()) {
    std::string pieceHashes;
    for (const auto& hash : file.pieceHashes) {
      if (hash.size() != HASH_LENGTH) {
        throw DL_ABORT_EX("Bad ED2K shared file piece hash.");
      }
      if (!pieceHashes.empty()) {
        pieceHashes += ":";
      }
      pieceHashes += util::toHex(hash);
    }
    payload += "|pieces=" + pieceHashes;
  }
  if (!file.aichRootHash.empty()) {
    if (file.aichRootHash.size() != AICH_HASH_LENGTH) {
      throw DL_ABORT_EX("Bad ED2K shared file AICH root.");
    }
    payload += "|aich=" + util::toHex(file.aichRootHash);
  }
  return payload;
}

bool parseSharedFileStatePayload(SharedFile& file, const std::string& payload)
{
  SharedFile parsed;
  std::vector<std::string> fields;
  util::split(payload.begin(), payload.end(), std::back_inserter(fields), '|',
              true);
  for (const auto& field : fields) {
    auto divided = util::divide(field.begin(), field.end(), '=');
    const std::string key(divided.first.first, divided.first.second);
    const std::string value(divided.second.first, divided.second.second);
    if (key == "v" && value != "1") {
      return false;
    }
    if (key == "hash") {
      parsed.hash = util::fromHex(value.begin(), value.end());
    }
    else if (key == "size") {
      if (!parseInt64(parsed.size, value)) {
        return false;
      }
    }
    else if (key == "path") {
      parsed.path = util::percentDecode(value.begin(), value.end());
    }
    else if (key == "name") {
      parsed.name = util::percentDecode(value.begin(), value.end());
    }
    else if (key == "origin") {
      if (!parseSharedOrigin(parsed.origin, value)) {
        return false;
      }
    }
    else if (key == "completed") {
      if (!parseBool(parsed.completed, value)) {
        return false;
      }
    }
    else if (key == "lastHashTime") {
      if (!parseInt64(parsed.lastHashTime, value)) {
        return false;
      }
    }
    else if (key == "pieces") {
      std::vector<std::string> hashes;
      util::split(value.begin(), value.end(), std::back_inserter(hashes), ':',
                  true);
      for (const auto& hash : hashes) {
        auto binary = util::fromHex(hash.begin(), hash.end());
        if (binary.size() != HASH_LENGTH) {
          return false;
        }
        parsed.pieceHashes.push_back(std::move(binary));
      }
    }
    else if (key == "aich") {
      parsed.aichRootHash = util::fromHex(value.begin(), value.end());
      if (parsed.aichRootHash.size() != AICH_HASH_LENGTH) {
        return false;
      }
    }
  }
  if (!isValidSharedFile(parsed)) {
    return false;
  }
  file = std::move(parsed);
  return true;
}

} // namespace ed2k

} // namespace aria2
