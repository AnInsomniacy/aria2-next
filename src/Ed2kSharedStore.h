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
#ifndef D_ED2K_SHARED_STORE_H
#define D_ED2K_SHARED_STORE_H

#include "common.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria2 {

struct DownloadResult;
class Option;

namespace ed2k {

enum class SharedOrigin {
  COMPLETED_DOWNLOAD,
  IMPORTED_FILE,
};

struct SharedFile {
  std::string hash;
  std::string aichRootHash;
  std::vector<std::string> pieceHashes;
  std::string path;
  std::string name;
  int64_t size = 0;
  int64_t lastHashTime = 0;
  SharedOrigin origin = SharedOrigin::COMPLETED_DOWNLOAD;
  bool completed = true;
};

class SharedStore {
private:
  std::vector<SharedFile> files_;

public:
  bool addOrReplace(SharedFile file);
  bool addCompletedDownload(const DownloadResult& result, int64_t now);
  size_t loadOptionState(const Option* option);
  size_t importOptionFiles(const Option* option, int64_t now);

  const SharedFile* findByHash(const std::string& hash) const;
  std::vector<SharedFile> list() const { return files_; }
  size_t size() const { return files_.size(); }
};

bool isValidSharedFile(const SharedFile& file);
bool importSharedFile(SharedFile& result, const std::string& path,
                      int64_t now);
std::string createSharedFileStatePayload(const SharedFile& file);
bool parseSharedFileStatePayload(SharedFile& file, const std::string& payload);

} // namespace ed2k

} // namespace aria2

#endif // D_ED2K_SHARED_STORE_H
