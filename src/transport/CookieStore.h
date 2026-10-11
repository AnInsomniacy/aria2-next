/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#ifndef ARIA2_TRANSPORT_COOKIE_STORE_H
#define ARIA2_TRANSPORT_COOKIE_STORE_H

#include "common.h"

#include <memory>
#include <string>
#include <vector>

namespace aria2::http {

// libcurl reads Netscape cookie files itself. Browser SQLite stores (Firefox
// moz_cookies, Chromium cookies with plaintext values) are converted here.
bool isSqliteCookieFile(const std::string& path);

struct SqliteCookies {
  // Changes whenever the store file changes; callers load a revision once
  // into their shared libcurl cookie jar.
  std::string revision;
  std::shared_ptr<const std::vector<std::string>> lines;
};

// Returns the store's cookies as Netscape lines. Conversions are cached per
// file revision; throws when the store is unreadable so the task fails with
// the real cause.
SqliteCookies sqliteCookies(const std::string& path);

} // namespace aria2::http
#endif
