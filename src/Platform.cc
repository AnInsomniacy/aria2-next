/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "Platform.h"

#include <stdlib.h> /* _fmode */
#include <fcntl.h>  /*  _O_BINARY */

#include <iostream>

#ifdef HAVE_LIBCURL
#  include <curl/curl.h>
#endif // HAVE_LIBCURL
#ifdef HAVE_OPENSSL
#  include <openssl/err.h>
#  include <openssl/ssl.h>
#endif // HAVE_OPENSSL


#include "a2netcompat.h"
#include "DlAbortEx.h"
#include "message.h"
#include "fmt.h"
#include "console.h"
#include "OptionParser.h"
#include "prefs.h"
#include "LogFactory.h"
#include "util.h"
#include "SocketCore.h"

namespace aria2 {

bool Platform::initialized_ = false;

#ifdef HAVE_OPENSSL
OSSL_PROVIDER* Platform::legacy_provider_ = nullptr;
OSSL_PROVIDER* Platform::default_provider_ = nullptr;
#endif // HAVE_OPENSSL

Platform::Platform() { setUp(); }

Platform::~Platform() { tearDown(); }

bool Platform::setUp()
{
  if (initialized_) {
    return false;
  }
  initialized_ = true;
#ifdef HAVE_LIBCURL
  CURLcode curlCode = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (curlCode != CURLE_OK) {
    initialized_ = false;
    throw DL_ABORT_EX(
        fmt("curl_global_init failed: %s", curl_easy_strerror(curlCode)));
  }
#endif // HAVE_LIBCURL
#ifdef HAVE_OPENSSL
  // RC4 is in the legacy provider.
  legacy_provider_ = OSSL_PROVIDER_load(nullptr, "legacy");
  if (!legacy_provider_) {
    throw DL_ABORT_EX("OSSL_PROVIDER_load 'legacy' failed.");
  }

  default_provider_ = OSSL_PROVIDER_load(nullptr, "default");
  if (!default_provider_) {
    throw DL_ABORT_EX("OSSL_PROVIDER_load 'default' failed.");
  }
#endif   // HAVE_OPENSSL


#ifdef HAVE_WINSOCK2_H
  WSADATA wsaData;
  memset(reinterpret_cast<char*>(&wsaData), 0, sizeof(wsaData));
  if (WSAStartup(MAKEWORD(1, 1), &wsaData)) {
    throw DL_ABORT_EX(MSG_WINSOCK_INIT_FAILD);
  }
#endif // HAVE_WINSOCK2_H

#ifdef __MINGW32__
  (void)_setmode(_fileno(stdin), _O_BINARY);
  (void)_setmode(_fileno(stdout), _O_BINARY);
  (void)_setmode(_fileno(stderr), _O_BINARY);
#endif // __MINGW32__

  return true;
}

bool Platform::tearDown()
{
  if (!initialized_) {
    return false;
  }
  initialized_ = false;

#ifdef ENABLE_SSL
  SocketCore::setClientTLSContext(nullptr);
  SocketCore::setServerTLSContext(nullptr);
#endif // ENABLE_SSL

#ifdef HAVE_LIBCURL
  curl_global_cleanup();
#endif // HAVE_LIBCURL

#ifdef HAVE_OPENSSL
  if (default_provider_) {
    OSSL_PROVIDER_unload(default_provider_);
    default_provider_ = nullptr;
  }

  if (legacy_provider_) {
    OSSL_PROVIDER_unload(legacy_provider_);
    legacy_provider_ = nullptr;
  }
#endif   // HAVE_OPENSSL

#ifdef HAVE_WINSOCK2_H
  WSACleanup();
#endif // HAVE_WINSOCK2_H
  // Deletes statically allocated resources. This is done to
  // distinguish memory leak from them. This is handy to use
  // valgrind.
  OptionParser::deleteInstance();
  option::deletePrefResource();
  return true;
}

bool Platform::isInitialized() { return initialized_; }

} // namespace aria2
