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
#include "Log.h"
#include "FileEntry.h"

#include <cassert>
#include <algorithm>

#include "util.h"
#include "a2algo.h"
#include "uri.h"
#include "fmt.h"

namespace aria2 {

FileEntry::FileEntry(std::string path, int64_t length, int64_t offset,
                     const std::vector<std::string>& uris)
    : length_(length),
      offset_(offset),
      uris_(uris.begin(), uris.end()),
      path_(std::move(path)),
      maxConnectionPerServer_(1),
      requested_(true)
{
}

FileEntry::FileEntry()
    : length_(0),
      offset_(0),
      maxConnectionPerServer_(1),
      requested_(false)
{
}

FileEntry::~FileEntry() = default;

bool FileEntry::operator<(const FileEntry& fileEntry) const
{
  return offset_ < fileEntry.offset_;
}

bool FileEntry::exists() const { return File(getPath()).exists(); }

int64_t FileEntry::gtoloff(int64_t goff) const
{
  assert(offset_ <= goff);
  return goff - offset_;
}

std::vector<std::string> FileEntry::getUris() const
{
  std::vector<std::string> uris(std::begin(spentUris_), std::end(spentUris_));
  uris.insert(std::end(uris), std::begin(uris_), std::end(uris_));
  return uris;
}

namespace {
size_t countInFlightHost(const FileEntry::InFlightRequestSet& requests,
                         const std::string& host)
{
  size_t count = 0;
  for (const auto& request : requests) {
    if (request->getHost() == host) {
      ++count;
    }
  }
  return count;
}

} // namespace

std::shared_ptr<Request> FileEntry::getRequest(
    const std::string& referer, const std::string& method)
{
  std::deque<std::string> deferred;
  while (!uris_.empty()) {
    auto uri = std::move(uris_.front());
    uris_.pop_front();

    auto req = std::make_shared<Request>();
    if (!req->setUri(uri)) {
      continue;
    }
    if (countInFlightHost(inFlightRequests_, req->getHost()) >=
        static_cast<size_t>(maxConnectionPerServer_)) {
      deferred.push_back(std::move(uri));
      continue;
    }
    if (referer == "*") {
      req->setReferer(uri);
    }
    else {
      req->setReferer(util::percentEncodeMini(referer));
    }
    req->setMethod(method);
    spentUris_.push_back(uri);
    inFlightRequests_.insert(req);
    uris_.insert(uris_.begin(), deferred.begin(), deferred.end());
    return req;
  }

  uris_.insert(uris_.begin(), deferred.begin(), deferred.end());
  return nullptr;
}

void FileEntry::poolRequest(const std::shared_ptr<Request>& request)
{
  removeRequest(request);
}

bool FileEntry::removeRequest(const std::shared_ptr<Request>& request)
{
  return inFlightRequests_.erase(request) == 1;
}

void FileEntry::removeURIWhoseHostnameIs(const std::string& hostname)
{
  std::deque<std::string> newURIs;
  for (std::deque<std::string>::const_iterator itr = uris_.begin(),
                                               eoi = uris_.end();
       itr != eoi; ++itr) {
    uri_split_result us;
    if (uri_split(&us, (*itr).c_str()) == -1) {
      continue;
    }
    if (us.fields[USR_HOST].len != hostname.size() ||
        memcmp((*itr).c_str() + us.fields[USR_HOST].off, hostname.c_str(),
               hostname.size()) != 0) {
      newURIs.push_back(*itr);
    }
  }
  ARIA2_LOG_DEBUG(fmt("Removed %lu duplicate hostname URIs for path=%s",
                   static_cast<unsigned long>(uris_.size() - newURIs.size()),
                   getPath().c_str()));
  uris_.swap(newURIs);
}

void FileEntry::removeIdenticalURI(const std::string& uri)
{
  uris_.erase(std::remove(uris_.begin(), uris_.end(), uri), uris_.end());
}

void FileEntry::addURIResult(std::string uri, error_code::Value result)
{
  uriResults_.push_back(URIResult(uri, result));
}

namespace {
class FindURIResultByResult {
private:
  error_code::Value r_;

public:
  FindURIResultByResult(error_code::Value r) : r_(r) {}

  bool operator()(const URIResult& uriResult) const
  {
    return uriResult.getResult() == r_;
  }
};
} // namespace

void FileEntry::extractURIResult(std::deque<URIResult>& res,
                                 error_code::Value r)
{
  auto i = std::stable_partition(uriResults_.begin(), uriResults_.end(),
                                 FindURIResultByResult(r));
  std::copy(uriResults_.begin(), i, std::back_inserter(res));
  uriResults_.erase(uriResults_.begin(), i);
}

void FileEntry::releaseRuntimeResource()
{
  inFlightRequests_.clear();
}

namespace {
template <typename InputIterator>
void putBackUri(std::deque<std::string>& uris, InputIterator first,
                InputIterator last)
{
  for (; first != last; ++first) {
    uris.push_front((*first)->getUri());
  }
}
} // namespace

void FileEntry::putBackRequest()
{
  putBackUri(uris_, inFlightRequests_.begin(), inFlightRequests_.end());
}

namespace {
template <typename InputIterator, typename T>
InputIterator findRequestByUri(InputIterator first, InputIterator last,
                               const T& uri)
{
  for (; first != last; ++first) {
    if (!(*first)->removalRequested() && (*first)->getUri() == uri) {
      return first;
    }
  }
  return last;
}
} // namespace

bool FileEntry::removeUri(const std::string& uri)
{
  auto itr = std::find(spentUris_.begin(), spentUris_.end(), uri);
  if (itr == spentUris_.end()) {
    itr = std::find(uris_.begin(), uris_.end(), uri);
    if (itr == uris_.end()) {
      return false;
    }
    uris_.erase(itr);
    return true;
  }
  spentUris_.erase(itr);
  std::shared_ptr<Request> req;
  auto riter =
      findRequestByUri(inFlightRequests_.begin(), inFlightRequests_.end(), uri);
  if (riter != inFlightRequests_.end()) {
    req = *riter;
    req->requestRemoval();
  }
  return true;
}

std::string FileEntry::getBasename() const { return File(path_).getBasename(); }

std::string FileEntry::getDirname() const { return File(path_).getDirname(); }

size_t FileEntry::setUris(const std::vector<std::string>& uris)
{
  uris_.clear();
  return addUris(uris.begin(), uris.end());
}

bool FileEntry::addUri(const std::string& uri)
{
  std::string peUri = util::percentEncodeMini(uri);
  if (uri_split(nullptr, peUri.c_str()) == 0) {
    uris_.push_back(peUri);
    return true;
  }
  else {
    return false;
  }
}

bool FileEntry::insertUri(const std::string& uri, size_t pos)
{
  std::string peUri = util::percentEncodeMini(uri);
  if (uri_split(nullptr, peUri.c_str()) != 0) {
    return false;
  }
  pos = std::min(pos, uris_.size());
  uris_.insert(uris_.begin() + pos, peUri);
  return true;
}

void FileEntry::setPath(std::string path) { path_ = std::move(path); }

void FileEntry::setContentType(std::string contentType)
{
  contentType_ = std::move(contentType);
}

size_t FileEntry::countInFlightRequest() const
{
  return inFlightRequests_.size();
}

void FileEntry::setOriginalName(std::string originalName)
{
  originalName_ = std::move(originalName);
}

void FileEntry::setSuffixPath(std::string suffixPath)
{
  suffixPath_ = std::move(suffixPath);
}

bool FileEntry::emptyRequestUri() const
{
  return uris_.empty() && inFlightRequests_.empty();
}

void writeFilePath(std::ostream& o, const std::shared_ptr<FileEntry>& entry,
                   bool memory)
{
  if (entry->getPath().empty()) {
    auto uris = entry->getUris();
    if (uris.empty()) {
      o << "n/a";
    }
    else {
      o << uris.front();
    }
    return;
  }

  if (memory) {
    o << "[MEMORY]" << File(entry->getPath()).getBasename();
  }
  else {
    o << entry->getPath();
  }
}

} // namespace aria2
