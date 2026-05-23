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
#ifndef D_LIBTORRENT_PROGRESS_INFO_FILE_H
#define D_LIBTORRENT_PROGRESS_INFO_FILE_H

#include "ProgressInfoFile.h"

#include <memory>
#include <string>

namespace aria2 {

class DownloadContext;
class IOFile;

class LibtorrentProgressInfoFile : public ProgressInfoFile {
private:
  std::shared_ptr<DownloadContext> dctx_;
  std::string filename_;
  std::string lastDigest_;

  void save(IOFile& fp);

public:
  explicit LibtorrentProgressInfoFile(
      const std::shared_ptr<DownloadContext>& dctx);
  ~LibtorrentProgressInfoFile();

  std::string getFilename() CXX11_OVERRIDE { return filename_; }
  bool exists() CXX11_OVERRIDE;
  void save() CXX11_OVERRIDE;
  void load() CXX11_OVERRIDE;
  void removeFile() CXX11_OVERRIDE;
  void updateFilename() CXX11_OVERRIDE;
};

} // namespace aria2

#endif // D_LIBTORRENT_PROGRESS_INFO_FILE_H
