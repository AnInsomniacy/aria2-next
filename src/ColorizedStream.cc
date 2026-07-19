/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2013 Nils Maier
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

#include "ColorizedStream.h"

#include <cstdint>
#include <utility>

namespace aria2 {
namespace colors {

const Color black("30");
const Color red("31");
const Color green("32");
const Color yellow("33");
const Color blue("34");
const Color magenta("35");
const Color cyan("36");
const Color white("37");

const Color lightred("1;31");
const Color lightgreen("1;32");
const Color lightyellow("1;33");
const Color lightblue("1;34");
const Color lightmagenta("1;35");
const Color lightcyan("1;36");
const Color lightwhite("1;37");

const Color clear("0");

} // namespace colors

namespace {

// Decodes the UTF-8 sequence starting at s[i] (length n). Stores the code
// point in cp and returns the number of bytes consumed. Malformed input is
// consumed one byte at a time and reported as U+FFFD.
size_t decodeUtf8(const std::string& s, size_t i, size_t n, uint32_t& cp)
{
  const auto b0 = static_cast<unsigned char>(s[i]);
  size_t len;
  if (b0 < 0x80) {
    cp = b0;
    return 1;
  }
  else if ((b0 & 0xe0) == 0xc0) {
    cp = b0 & 0x1f;
    len = 2;
  }
  else if ((b0 & 0xf0) == 0xe0) {
    cp = b0 & 0x0f;
    len = 3;
  }
  else if ((b0 & 0xf8) == 0xf0) {
    cp = b0 & 0x07;
    len = 4;
  }
  else {
    cp = 0xfffd;
    return 1;
  }
  if (i + len > n) {
    cp = 0xfffd;
    return 1;
  }
  for (size_t k = 1; k < len; ++k) {
    const auto b = static_cast<unsigned char>(s[i + k]);
    if ((b & 0xc0) != 0x80) {
      cp = 0xfffd;
      return 1;
    }
    cp = (cp << 6) | (b & 0x3f);
  }
  return len;
}

// Terminal column width of a code point: East Asian wide/fullwidth code
// points occupy two columns, everything else one. Over-counting is the safe
// direction for truncation (a line can only end up shorter, never wrap).
size_t displayWidth(uint32_t cp)
{
  static const std::pair<uint32_t, uint32_t> wide[] = {
      {0x1100, 0x115f},   {0x2e80, 0x303e},   {0x3041, 0x33ff},
      {0x3400, 0x4dbf},   {0x4e00, 0x9fff},   {0xa000, 0xa4cf},
      {0xa960, 0xa97f},   {0xac00, 0xd7a3},   {0xf900, 0xfaff},
      {0xfe10, 0xfe19},   {0xfe30, 0xfe52},   {0xfe54, 0xfe66},
      {0xfe68, 0xfe6b},   {0xff00, 0xff60},   {0xffe0, 0xffe6},
      {0x1f300, 0x1f64f}, {0x1f900, 0x1f9ff}, {0x20000, 0x2fffd},
      {0x30000, 0x3fffd},
  };
  for (const auto& r : wide) {
    if (cp >= r.first && cp <= r.second) {
      return 2;
    }
  }
  return 1;
}

} // namespace

std::string ColorizedStreamBuf::str(bool color) const
{
  std::stringstream rv;
  for (const auto& e : elems) {
    if (color || e.first != eColor) {
      rv << e.second;
    }
  }
  if (color) {
    rv << colors::clear.str();
  }
  return rv.str();
}

std::string ColorizedStreamBuf::str(bool color, size_t max) const
{
  // max is a budget of terminal columns, not bytes: UTF-8 sequences are
  // never split and East Asian wide characters count as two columns.
  std::stringstream rv;
  size_t remaining = max;
  bool full = false;
  for (const auto& e : elems) {
    if (e.first == eColor) {
      if (color) {
        rv << e.second;
      }
      continue;
    }
    if (full) {
      continue;
    }
    const auto& text = e.second;
    const size_t n = text.size();
    size_t i = 0;
    while (i < n) {
      uint32_t cp;
      const size_t len = decodeUtf8(text, i, n, cp);
      const size_t width = displayWidth(cp);
      if (width > remaining) {
        full = true;
        break;
      }
      rv.write(text.c_str() + i, len);
      remaining -= width;
      i += len;
    }
    if (remaining == 0) {
      full = true;
    }
  }
  if (color) {
    rv << colors::clear.str();
  }
  return rv.str();
}

} // namespace aria2
