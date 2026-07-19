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
#include "ConsoleStatCalc.h"

#ifdef HAVE_TERMIOS_H
#  include <termios.h>
#endif // HAVE_TERMIOS_H
#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif // HAVE_SYS_IOCTL_H
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iterator>

#include "a2time.h"
#include "DownloadEngine.h"
#include "RequestGroupMan.h"
#include "RequestGroup.h"
#include "FileAllocationMan.h"
#include "FileAllocationEntry.h"
#include "CheckIntegrityMan.h"
#include "CheckIntegrityEntry.h"
#include "util.h"
#include "DownloadContext.h"
#include "wallclock.h"
#include "FileEntry.h"
#include "console.h"
#include "ColorizedStream.h"
#include "Option.h"

#ifdef ENABLE_BITTORRENT
#  include "bittorrent_helper.h"
#  include "PeerStorage.h"
#  include "BtRegistry.h"
#endif // ENABLE_BITTORRENT

namespace aria2 {

std::string SizeFormatter::operator()(int64_t size) const
{
  return format(size);
}

namespace {
class AbbrevSizeFormatter : public SizeFormatter {
protected:
  virtual std::string format(int64_t size) const override
  {
    return util::abbrevSize(size);
  }
};
} // namespace

namespace {
class PlainSizeFormatter : public SizeFormatter {
protected:
  virtual std::string format(int64_t size) const override
  {
    return util::itos(size);
  }
};
} // namespace

namespace {

// True when the console locale advertises UTF-8, so the readout can use
// block-element characters for the progress bar.
bool utf8Console()
{
#ifdef __MINGW32__
  return false;
#else  // !__MINGW32__
  static const bool utf8 = []() {
    for (const char* name : {"LC_ALL", "LC_CTYPE", "LANG"}) {
      const char* value = getenv(name);
      if (value && *value) {
        std::string v = util::toLower(value);
        return v.find("utf-8") != std::string::npos ||
               v.find("utf8") != std::string::npos;
      }
    }
    return false;
  }();
  return utf8;
#endif // !__MINGW32__
}

// Appends a progress bar of `width` cells. UTF-8 terminals get eighth-block
// sub-cell resolution; everything else gets a plain ASCII bar.
void appendProgressBar(ColorizedStream& o, int64_t completed, int64_t total,
                       size_t width)
{
  const double frac =
      total > 0 ? static_cast<double>(completed) / static_cast<double>(total)
                : 0.0;
  if (utf8Console()) {
    // U+2588 FULL BLOCK, U+2589..U+258F partial left blocks, U+2591 LIGHT
    // SHADE for the unfilled remainder.
    static const char* const partial[] = {
        "",             "\xE2\x96\x8F", "\xE2\x96\x8E", "\xE2\x96\x8D",
        "\xE2\x96\x8C", "\xE2\x96\x8B", "\xE2\x96\x8A", "\xE2\x96\x89"};
    const double cells = frac * width;
    size_t full = static_cast<size_t>(cells);
    const int eighth =
        static_cast<int>((cells - static_cast<double>(full)) * 8);
    std::string bar;
    for (size_t i = 0; i < full; ++i) {
      bar += "\xE2\x96\x88";
    }
    if (full < width && eighth > 0) {
      bar += partial[eighth];
      ++full;
    }
    o << colors::green << bar << colors::clear;
    std::string rest;
    for (size_t i = full; i < width; ++i) {
      rest += "\xE2\x96\x91";
    }
    o << rest;
    return;
  }
  size_t full = static_cast<size_t>(frac * width);
  if (full > width) {
    full = width;
  }
  std::string bar(full, '=');
  if (full < width) {
    bar += '>';
  }
  o << "[" << colors::green << bar << colors::clear;
  if (full < width) {
    o << std::string(width - full - 1, ' ');
  }
  o << "]";
}

} // namespace

namespace {
void printSizeProgress(ColorizedStream& o,
                       const std::shared_ptr<RequestGroup>& rg,
                       const TransferStat& stat,
                       const SizeFormatter& sizeFormatter, bool withPercent)
{
#ifdef ENABLE_BITTORRENT
  if (rg->isSeeder()) {
    o << "SEED(";
    if (rg->getCompletedLength() > 0) {
      std::streamsize oldprec = o.precision();
      o << std::fixed << std::setprecision(1)
        << ((stat.allTimeUploadLength * 10) / rg->getCompletedLength()) / 10.0
        << std::setprecision(oldprec) << std::resetiosflags(std::ios::fixed);
    }
    else {
      o << "--";
    }
    o << ")";
  }
  else
#endif // ENABLE_BITTORRENT
  {
    o << sizeFormatter(rg->getCompletedLength()) << "B/"
      << sizeFormatter(rg->getTotalLength()) << "B";
    if (withPercent && rg->getTotalLength() > 0) {
      o << colors::cyan << "("
        << 100 * rg->getCompletedLength() / rg->getTotalLength() << "%)";
      o << colors::clear;
    }
  }
}
} // namespace

namespace {
void printProgressCompact(ColorizedStream& o, const DownloadEngine* e,
                          const SizeFormatter& sizeFormatter)
{
  if (!e->getRequestGroupMan()->downloadFinished()) {
    NetStat& netstat = e->getRequestGroupMan()->getNetStat();
    int dl = netstat.calculateDownloadSpeed();
    int ul = netstat.calculateUploadSpeed();
    o << colors::magenta << "[" << colors::clear << "DL:" << colors::green
      << sizeFormatter(dl) << "B" << colors::clear;
    if (ul) {
      o << " UL:" << colors::cyan << sizeFormatter(ul) << "B" << colors::clear;
    }
    o << colors::magenta << "]" << colors::clear;
  }

  const RequestGroupList& groups = e->getRequestGroupMan()->getRequestGroups();
  size_t cnt = 0;
  const size_t MAX_ITEM = 5;
  for (auto i = groups.begin(), eoi = groups.end(); i != eoi && cnt < MAX_ITEM;
       ++i, ++cnt) {
    const std::shared_ptr<RequestGroup>& rg = *i;
    TransferStat stat = rg->calculateStat();
    o << colors::magenta << "[" << colors::clear << "#"
      << GroupId::toAbbrevHex(rg->getGID()) << " ";
    printSizeProgress(o, rg, stat, sizeFormatter, true);
    o << colors::magenta << "]" << colors::clear;
  }
  if (cnt < groups.size()) {
    o << "(+" << groups.size() - cnt << ")";
  }
}
} // namespace

namespace {
void printProgress(ColorizedStream& o, const std::shared_ptr<RequestGroup>& rg,
                   const DownloadEngine* e, const SizeFormatter& sizeFormatter,
                   size_t barWidth)
{
  TransferStat stat = rg->calculateStat();
  int eta = 0;
  if (rg->getTotalLength() > 0 && stat.downloadSpeed > 0) {
    eta =
        (rg->getTotalLength() - rg->getCompletedLength()) / stat.downloadSpeed;
  }
  o << colors::magenta << "[" << colors::clear << "#"
    << GroupId::toAbbrevHex(rg->getGID()) << " ";
  const bool showBar =
      barWidth > 0 && rg->getTotalLength() > 0 && !rg->isSeeder();
  if (showBar) {
    appendProgressBar(o, rg->getCompletedLength(), rg->getTotalLength(),
                      barWidth);
    std::string pct =
        util::itos(100 * rg->getCompletedLength() / rg->getTotalLength());
    if (pct.size() < 3) {
      pct.insert(0, 3 - pct.size(), ' ');
    }
    o << " " << colors::cyan << pct << "%" << colors::clear << " ";
  }
  printSizeProgress(o, rg, stat, sizeFormatter, !showBar);
  o << " CN:" << rg->getNumConnection();
#ifdef ENABLE_BITTORRENT
  auto btObj = e->getBtRegistry()->get(rg->getGID());
  if (btObj) {
    const PeerSet& peers = btObj->peerStorage->getUsedPeers();
    o << " SD:" << countSeeder(peers.begin(), peers.end());
  }
#endif // ENABLE_BITTORRENT

  if (!rg->downloadFinished()) {
    o << " DL:" << colors::green << sizeFormatter(stat.downloadSpeed) << "B"
      << colors::clear;
  }
  if (stat.sessionUploadLength > 0) {
    o << " UL:" << colors::cyan << sizeFormatter(stat.uploadSpeed) << "B"
      << colors::clear;
    o << "(" << sizeFormatter(stat.allTimeUploadLength) << "B)";
  }
  if (eta > 0) {
    o << " ETA:" << colors::yellow << util::secfmt(eta) << colors::clear;
  }
  o << colors::magenta << "]" << colors::clear;
}
} // namespace

namespace {
class PrintSummary {
private:
  size_t cols_;
  const DownloadEngine* e_;
  const SizeFormatter& sizeFormatter_;

public:
  PrintSummary(size_t cols, const DownloadEngine* e,
               const SizeFormatter& sizeFormatter)
      : cols_(cols), e_(e), sizeFormatter_(sizeFormatter)
  {
  }

  void operator()(const RequestGroupList::value_type& rg)
  {
    const char SEP_CHAR = '-';
    ColorizedStream o;
    printProgress(o, rg, e_, sizeFormatter_, 0);
    const std::vector<std::shared_ptr<FileEntry>>& fileEntries =
        rg->getDownloadContext()->getFileEntries();
    o << "\nFILE: ";
    writeFilePath(fileEntries.begin(), fileEntries.end(), o,
                  rg->inMemoryDownload());
    o << "\n" << std::setfill(SEP_CHAR) << std::setw(cols_) << SEP_CHAR << "\n";
    auto str = o.str(false);
    global::cout()->write(str.c_str());
  }
};
} // namespace

namespace {
void printProgressSummary(const RequestGroupList& groups, size_t cols,
                          const DownloadEngine* e,
                          const SizeFormatter& sizeFormatter)
{
  const char SEP_CHAR = '=';
  std::stringstream o;
  o << " *** Download Progress Summary";
  {
    time_t now = time(nullptr);
    struct tm tmbuf;
    char buf[24];
    if (localtime_r(&now, &tmbuf) != nullptr &&
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmbuf) != 0) {
      o << " as of " << buf;
    }
  }
  o << " *** \n"
    << std::setfill(SEP_CHAR) << std::setw(cols) << SEP_CHAR << "\n";
  global::cout()->write(o.str().c_str());
  std::for_each(groups.begin(), groups.end(),
                PrintSummary(cols, e, sizeFormatter));
}
} // namespace

ConsoleStatCalc::ConsoleStatCalc(std::chrono::seconds summaryInterval,
                                 bool colorOutput, bool humanReadable)
    : summaryInterval_(std::move(summaryInterval)),
      readoutVisibility_(true),
      truncate_(true),
#ifdef __MINGW32__
      isTTY_(true),
#else  // !__MINGW32__
      isTTY_(isatty(STDOUT_FILENO) == 1),
#endif // !__MINGW32__
      colorOutput_(colorOutput)
{
  if (humanReadable) {
    sizeFormatter_ = make_unique<AbbrevSizeFormatter>();
  }
  else {
    sizeFormatter_ = make_unique<PlainSizeFormatter>();
  }
}

void ConsoleStatCalc::calculateStat(const DownloadEngine* e)
{
  if (cp_.difference(global::wallclock()) + A2_DELTA_MILLIS <
      std::chrono::milliseconds(1000)) {
    return;
  }
  cp_ = global::wallclock();
  const SizeFormatter& sizeFormatter = *sizeFormatter_.get();

  // Some terminals (e.g., Windows terminal) prints next line when the
  // character reached at the last column.
  unsigned short int cols = 79;

  if (isTTY_) {
#ifndef __MINGW32__
#  ifdef HAVE_TERMIOS_H
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
      cols = size.ws_col - 1;
    }
#  endif // HAVE_TERMIOS_H
#else    // __MINGW32__
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (::GetConsoleScreenBufferInfo(::GetStdHandle(STD_OUTPUT_HANDLE),
                                     &info)) {
      cols = std::max(0, info.dwSize.X - 2);
    }
#endif   // !__MINGW32__
  }

  // The readout line is repainted with one write() so the console never
  // shows a half-cleared line. POSIX ANSI terminals erase with CSI K;
  // Windows consoles (whose ANSI filter only handles colors) and dumb
  // terminals overwrite with blanks instead.
  const bool color = isTTY_ && global::cout()->supportsColor() && colorOutput_;
#ifdef __MINGW32__
  const bool ansiErase = false;
#else  // !__MINGW32__
  const bool ansiErase = isTTY_ && global::cout()->supportsColor();
#endif // !__MINGW32__
  std::string clearSeq("\r");
  if (isTTY_) {
    if (ansiErase) {
      clearSeq += "\x1b[K";
    }
    else {
      clearSeq.append(cols, ' ');
      clearSeq += '\r';
    }
  }

  if (e->getRequestGroupMan()->countRequestGroup() > 0) {
    if ((summaryInterval_ > 0_s) &&
        lastSummaryNotified_.difference(global::wallclock()) +
                A2_DELTA_MILLIS >=
            summaryInterval_) {
      lastSummaryNotified_ = global::wallclock();
      if (isTTY_) {
        global::cout()->write(clearSeq.c_str());
      }
      printProgressSummary(e->getRequestGroupMan()->getRequestGroups(), cols, e,
                           sizeFormatter);
      global::cout()->write("\n");
      global::cout()->flush();
    }
  }
  if (!readoutVisibility_) {
    if (isTTY_) {
      global::cout()->write(clearSeq.c_str());
      global::cout()->flush();
    }
    return;
  }
  ColorizedStream o;
  size_t numGroup = e->getRequestGroupMan()->countRequestGroup();
  if (numGroup == 1) {
    const std::shared_ptr<RequestGroup>& rg =
        *e->getRequestGroupMan()->getRequestGroups().begin();
    const size_t barWidth = isTTY_ && cols >= 74 ? 20 : 0;
    printProgress(o, rg, e, sizeFormatter, barWidth);
  }
  else if (numGroup > 1) {
    // For more than 2 RequestGroups, use compact readout form
    printProgressCompact(o, e, sizeFormatter);
  }

  {
    auto& entry = e->getFileAllocationMan()->getPickedEntry();
    if (entry) {
      o << " [FileAlloc:#"
        << GroupId::toAbbrevHex(entry->getRequestGroup()->getGID()) << " "
        << sizeFormatter(entry->getCurrentLength()) << "B/"
        << sizeFormatter(entry->getTotalLength()) << "B(";
      if (entry->getTotalLength() > 0) {
        o << 100LL * entry->getCurrentLength() / entry->getTotalLength();
      }
      else {
        o << "--";
      }
      o << "%)]";
      if (e->getFileAllocationMan()->hasNext()) {
        o << "(+" << e->getFileAllocationMan()->countEntryInQueue() << ")";
      }
    }
  }
  {
    auto& entry = e->getCheckIntegrityMan()->getPickedEntry();
    if (entry) {
      o << " [Checksum:#"
        << GroupId::toAbbrevHex(entry->getRequestGroup()->getGID()) << " "
        << sizeFormatter(entry->getCurrentLength()) << "B/"
        << sizeFormatter(entry->getTotalLength()) << "B(";
      if (entry->getTotalLength() > 0) {
        o << 100LL * entry->getCurrentLength() / entry->getTotalLength();
      }
      else {
        o << "--";
      }
      o << "%)]";
      if (e->getCheckIntegrityMan()->hasNext()) {
        o << "(+" << e->getCheckIntegrityMan()->countEntryInQueue() << ")";
      }
    }
  }
  if (isTTY_) {
    std::string out(clearSeq);
    out += truncate_ ? o.str(color, cols) : o.str(color);
    global::cout()->write(out.c_str());
    global::cout()->flush();
  }
  else {
    auto str = o.str(false);
    global::cout()->write(str.c_str());
    global::cout()->write("\n");
  }
}

} // namespace aria2
