/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include <string>
#include <vector>

namespace aria2::http {

// libcurl reads Netscape cookie files itself. Browser stores (Firefox
// moz_cookies, Chromium cookies with plaintext values) are SQLite databases
// and are converted to Netscape lines here.
bool isSqliteCookieFile(const std::string& path);
// Throws Error(FileOpen) when the store cannot be read.
std::vector<std::string> sqliteCookieLines(const std::string& path);
// Identifies a file version so a store is loaded into the jar once.
std::string fileRevision(const std::string& path);

} // namespace aria2::http
