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
#ifndef D_COMMON_H
#define D_COMMON_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef __MINGW32__
#  ifdef malloc
#    undef malloc
#  endif
#  ifdef realloc
#    undef realloc
#  endif
#endif // __MINGW32__

#ifdef __MINGW32__
#  define WIN32_LEAN_AND_MEAN
#  ifndef ARIA2_WINDOWS_TARGET_VERSION
#    define ARIA2_WINDOWS_TARGET_VERSION 0x0A00
#  endif // ARIA2_WINDOWS_TARGET_VERSION
#  ifndef WINVER
#    define WINVER ARIA2_WINDOWS_TARGET_VERSION
#  endif // !WINVER
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT ARIA2_WINDOWS_TARGET_VERSION
#  endif // _WIN32_WINNT
#  if _WIN32_WINNT < 0x0601
#    error "_WIN32_WINNT must be at least 0x0601 for Boost.Asio Windows ABI consistency"
#  endif // _WIN32_WINNT < 0x0601
#  ifdef HAVE_WINSOCK2_H
#    ifndef FD_SETSIZE
#      define FD_SETSIZE 32768
#    endif // !FD_SETSIZE
#    include <winsock2.h>
#    undef ERROR
#  endif // HAVE_WINSOCK2_H
#  include <windows.h>
#endif // __MINGW32__

#define _(String) String

// use C99 limit macros
#define __STDC_LIMIT_MACROS
// included here for compatibility issues with old compiler/libraries.
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif // HAVE_STDINT_H

// For PRId64
#define __STDC_FORMAT_MACROS
#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#endif // HAVE_INTTYPES_H

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#endif // D_COMMON_H
