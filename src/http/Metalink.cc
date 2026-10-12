/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/Metalink.h"

#include "base/Error.h"
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"

#include <expat.h>

#include <algorithm>
#include <array>
#include <memory>

namespace aria2::http {

namespace {

constexpr std::string_view metalink4 = "urn:ietf:params:xml:ns:metalink";
constexpr std::string_view metalink3 = "http://www.metalinker.org/";
// Strongest first; all are understood by verifyChecksum().
constexpr std::array hashTypes{"sha-512", "sha-384", "sha-256",
                               "sha-224", "sha-1",   "md5"};
constexpr size_t maxText = 64 * 1024;

struct Url {
  int64_t priority;
  std::string url;
};

class Reader {
public:
  XML_Parser parser = nullptr;
  std::vector<MetalinkFile> files;
  std::string error;

  void stop(std::string why)
  {
    error = std::move(why);
    XML_StopParser(parser, XML_FALSE);
  }

  // Expat may deliver events of the current token after stop().
  void start(std::string_view name, const XML_Char** attributes)
  {
    if (!error.empty()) {
      return;
    }
    const auto space = name.find(' ');
    const auto ns = space == std::string_view::npos ? std::string_view()
                                                    : name.substr(0, space);
    const auto local =
        space == std::string_view::npos ? name : name.substr(space + 1);
    if (stack_.empty() && ns == metalink3) {
      stop("Metalink 3 documents are not supported");
      return;
    }
    if (stack_.empty() && (ns != metalink4 || local != "metalink")) {
      stop("Not a Metalink 4 document");
      return;
    }
    stack_.push_back(ns == metalink4 ? std::string(local) : std::string());
    text_.clear();
    const auto attribute = [attributes](std::string_view key) -> std::string {
      for (auto* item = attributes; item && *item; item += 2) {
        if (key == item[0]) {
          return item[1];
        }
      }
      return {};
    };
    if (at({"metalink", "file"})) {
      file_ = MetalinkFile{attribute("name")};
      urls_.clear();
      hashes_.clear();
      pieceSets_.clear();
    }
    else if (at({"metalink", "file", "hash"})) {
      hashType_ = text::lower(attribute("type"));
    }
    else if (at({"metalink", "file", "url"})) {
      priority_ = text::toInt(attribute("priority")).value_or(999999);
    }
    else if (at({"metalink", "file", "pieces"})) {
      pieceSet_ = PieceHashes{text::lower(attribute("type")),
                              text::toInt(attribute("length")).value_or(0)};
    }
  }

  void end()
  {
    if (!error.empty()) {
      return;
    }
    const auto value = std::string(text::trim(text_));
    if (at({"metalink", "file", "size"})) {
      file_.size = text::toInt(value).value_or(-1);
    }
    else if (at({"metalink", "file", "hash"})) {
      hashes_.emplace_back(hashType_, text::lower(value));
    }
    else if (at({"metalink", "file", "pieces", "hash"})) {
      pieceSet_.hashes.push_back(text::lower(value));
    }
    else if (at({"metalink", "file", "pieces"})) {
      pieceSets_.push_back(std::move(pieceSet_));
    }
    else if (at({"metalink", "file", "url"})) {
      const auto parts = uri::parse(value);
      const auto scheme = parts ? parts->scheme : std::string();
      if (scheme == "http" || scheme == "https" || scheme == "sftp") {
        urls_.push_back({priority_, value});
      }
    }
    else if (at({"metalink", "file"})) {
      finishFile();
    }
    stack_.pop_back();
    text_.clear();
  }

  void characters(std::string_view data)
  {
    if (text_.size() + data.size() <= maxText) {
      text_.append(data);
    }
  }

private:
  bool at(std::initializer_list<std::string_view> path) const
  {
    return std::equal(stack_.begin(), stack_.end(), path.begin(), path.end());
  }

  void finishFile()
  {
    if (!isContainedRelativePath(file_.name)) {
      stop("Unsafe Metalink file name " + file_.name);
      return;
    }
    if (urls_.empty()) {
      return; // metaurl-only files (torrents) are not followed
    }
    std::stable_sort(
        urls_.begin(), urls_.end(),
        [](const Url& a, const Url& b) { return a.priority < b.priority; });
    for (auto& url : urls_) {
      file_.urls.push_back(std::move(url.url));
    }
    for (const auto* type : hashTypes) {
      const auto found =
          std::find_if(hashes_.begin(), hashes_.end(),
                       [type](const auto& hash) { return hash.first == type; });
      if (found != hashes_.end()) {
        file_.checksum = found->first + "=" + found->second;
        break;
      }
    }
    // Pieces must cover exactly the declared size to be of any use.
    for (const auto* type : hashTypes) {
      const auto found = std::find_if(
          pieceSets_.begin(), pieceSets_.end(), [&](const PieceHashes& set) {
            return set.type == type && set.length > 0 && file_.size > 0 &&
                   static_cast<int64_t>(set.hashes.size()) ==
                       (file_.size + set.length - 1) / set.length;
          });
      if (found != pieceSets_.end()) {
        file_.pieces = std::move(*found);
        break;
      }
    }
    files.push_back(std::move(file_));
  }

  std::vector<std::string> stack_; // local names; empty for foreign elements
  std::string text_;
  MetalinkFile file_;
  std::vector<Url> urls_;
  std::vector<std::pair<std::string, std::string>> hashes_;
  std::string hashType_;
  int64_t priority_ = 999999;
  PieceHashes pieceSet_;
  std::vector<PieceHashes> pieceSets_;
};

} // namespace

std::vector<MetalinkFile> parseMetalink(std::string_view document)
{
  std::unique_ptr<XML_ParserStruct, decltype(&XML_ParserFree)> parser(
      XML_ParserCreateNS(nullptr, ' '), XML_ParserFree);
  if (!parser) {
    throw std::bad_alloc();
  }
  Reader reader;
  reader.parser = parser.get();
  XML_SetUserData(parser.get(), &reader);
  XML_SetElementHandler(
      parser.get(),
      [](void* self, const XML_Char* name, const XML_Char** attributes) {
        static_cast<Reader*>(self)->start(name, attributes);
      },
      [](void* self, const XML_Char*) { static_cast<Reader*>(self)->end(); });
  XML_SetCharacterDataHandler(
      parser.get(), [](void* self, const XML_Char* data, int size) {
        static_cast<Reader*>(self)->characters(
            std::string_view(data, static_cast<size_t>(size)));
      });
  // Entity declarations serve no purpose in Metalink and enable expansion
  // attacks; reject documents that declare any.
  XML_SetEntityDeclHandler(parser.get(), [](void* self, const XML_Char*, int,
                                            const XML_Char*, int,
                                            const XML_Char*, const XML_Char*,
                                            const XML_Char*, const XML_Char*) {
    static_cast<Reader*>(self)->stop(
        "Metalink documents must not declare entities");
  });
  if (document.size() > 64 * 1024 * 1024) {
    fail(ErrorCode::MetalinkParse, "The Metalink document is too large");
  }
  const auto parsed = XML_Parse(parser.get(), document.data(),
                                static_cast<int>(document.size()), XML_TRUE);
  if (!reader.error.empty()) {
    fail(ErrorCode::MetalinkParse, "{}", reader.error);
  }
  if (parsed != XML_STATUS_OK) {
    fail(ErrorCode::MetalinkParse, "Invalid Metalink document: {}",
         XML_ErrorString(XML_GetErrorCode(parser.get())));
  }
  if (reader.files.empty()) {
    fail(ErrorCode::MetalinkParse,
         "The Metalink document has no downloadable file");
  }
  return std::move(reader.files);
}

bool isMetalink(std::string_view path, std::string_view contentType)
{
  const auto extension = toPath(path).extension().string();
  return text::iequals(extension, ".meta4") ||
         text::iequals(extension, ".metalink") ||
         text::istartsWith(contentType, "application/metalink4+xml");
}

} // namespace aria2::http
