/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "core/Version.h"

#include <curl/curl.h>
#include <openssl/opensslv.h>
#include <sqlite3.h>
#include <spdlog/fmt/fmt.h>

#include <boost/version.hpp>

namespace aria2 {

std::string engineVersion() { return ARIA2_VERSION; }

std::vector<const char*> enabledFeatures()
{
  return {"BitTorrent", "ED2K", "GZip", "HTTPS", "HLS/DASH", "Message Digest",
          "Metalink", "SFTP"};
}

std::string versionText()
{
  std::string features;
  for (const auto* feature : enabledFeatures()) {
    features += features.empty() ? "" : ", ";
    features += feature;
  }
  return fmt::format(
      "aria2-next version {}\n"
      "Copyright (C) 2006, 2019 Tatsuhiro Tsujikawa\n"
      "Copyright (C) 2026 aria2-next contributors\n\n"
      "This program is free software; you can redistribute it and/or modify\n"
      "it under the terms of the GNU General Public License as published by\n"
      "the Free Software Foundation; either version 2 of the License, or\n"
      "(at your option) any later version.\n\n"
      "Enabled Features: {}\n"
      "Libraries: libcurl/{} {} SQLite/{} Boost/{}.{}.{}\n",
      engineVersion(), features, curl_version_info(CURLVERSION_NOW)->version,
      OPENSSL_VERSION_TEXT, sqlite3_libversion(), BOOST_VERSION / 100000,
      BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);
}

} // namespace aria2
