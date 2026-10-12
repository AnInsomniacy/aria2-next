/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/HttpTask.h"

#include "base/ContentDisposition.h"
#include "base/Log.h"
#include "base/Path.h"
#include "base/Text.h"
#include "base/Uri.h"
#include "core/Engine.h"
#include "disk/Output.h"
#include "http/Checksum.h"
#include "http/Client.h"
#include "http/Request.h"
#include "http/Worker.h"
#include "media/MediaTask.h"
#include "state/StateStore.h"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/post.hpp>
#include <boost/json/array.hpp>

#include <algorithm>
#include <bit>
#include <random>

namespace aria2::http {

namespace json = boost::json;
using namespace std::chrono_literals;

namespace {

constexpr auto tickInterval = 500ms;
constexpr double tickSeconds =
    std::chrono::duration<double>(tickInterval).count();
// Data reaches the disk in chunks of this size per connection.
constexpr int64_t minFlushSize = 64 << 10;
constexpr int64_t maxFlushSize = 1 << 20;
// Slow connections reach the disk at least this often, so a crash loses
// little more than this (and the last state-save-interval).
constexpr auto flushAfter = 1s;
// Unwritten bytes a task may hold before its connections pause.
constexpr int64_t writeBudget = 64 << 20;
// A connection that is up but delivers nothing for this long is stalled,
// whether or not it delivered anything before. Connecting is bounded by
// connect-timeout instead.
constexpr auto stallAfter = 8s;
// A probe without an answer after this long (a server that accepts and stays
// silent, a lost SYN) gets a second copy; the first answer wins.
constexpr auto probeHedge = 2s;
// Overload feedback is relaxed one connection at a time after a calm period.
constexpr auto calmPeriod = 30s;
// A download starts with at most this many connections (aria2's per-server
// limit) and goes beyond only while more connections bring more speed: a step
// is judged growthPeriod after it was taken and must add growthGain.
constexpr int initialConnections = 16;
constexpr auto growthPeriod = 2s;
constexpr double growthGain = 1.1;
constexpr int maxRestarts = 3;
// Rounds of downloading pieces again that failed their hash.
constexpr int maxRepairs = 3;

// Weak entity tags cannot validate byte ranges (RFC 9110 13.1.5). Servers
// that omit the quotes get them, as If-Range requires an entity-tag.
std::string strongTag(const std::string& etag)
{
  if (etag.empty() || etag.starts_with("W/")) {
    return {};
  }
  return etag.front() == '"' ? etag : '"' + etag + '"';
}

// A client may only use a modification date as a range validator when the
// response was dated at least a minute later (RFC 9110 8.8.2.2).
std::string strongDate(const std::string& modified, const std::string& date)
{
  if (modified.empty() || date.empty()) {
    return {};
  }
  const auto at = curl_getdate(modified.c_str(), nullptr);
  const auto now = curl_getdate(date.c_str(), nullptr);
  return at >= 0 && now - at >= 60 ? modified : std::string();
}

Clock::duration backoff(int failures, int64_t retryWait)
{
  if (retryWait > 0) {
    return std::chrono::seconds(retryWait);
  }
  static std::mt19937 random{std::random_device{}()};
  const auto base = std::min<int64_t>(30000, 500LL << std::min(failures, 6));
  return std::chrono::milliseconds(
      std::uniform_int_distribution<int64_t>(base / 2, base)(random));
}

ErrorCode curlError(int code)
{
  switch (code) {
  case CURLE_COULDNT_RESOLVE_HOST:
  case CURLE_COULDNT_RESOLVE_PROXY:
    return ErrorCode::NameResolve;
  case CURLE_OPERATION_TIMEDOUT:
    return ErrorCode::TimedOut;
  case CURLE_TOO_MANY_REDIRECTS:
    return ErrorCode::TooManyRedirects;
  case CURLE_LOGIN_DENIED:
  case CURLE_REMOTE_ACCESS_DENIED:
    return ErrorCode::HttpAuth;
  case CURLE_REMOTE_FILE_NOT_FOUND:
    return ErrorCode::ResourceNotFound;
  default:
    return ErrorCode::Network;
  }
}

// TLS and protocol errors that retrying cannot fix.
bool permanent(int code)
{
  switch (code) {
  case CURLE_UNSUPPORTED_PROTOCOL:
  case CURLE_URL_MALFORMAT:
  case CURLE_PEER_FAILED_VERIFICATION:
  case CURLE_SSL_CERTPROBLEM:
  case CURLE_SSL_CACERT_BADFILE:
  case CURLE_SSL_ISSUER_ERROR:
  case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
  case CURLE_TOO_MANY_REDIRECTS:
  case CURLE_LOGIN_DENIED:
  case CURLE_REMOTE_ACCESS_DENIED:
  case CURLE_REMOTE_FILE_NOT_FOUND:
    return true;
  default:
    return false;
  }
}

} // namespace

// ------------------------------------------------------------------ Worker --

Worker::Worker(HttpTask& task, int id, size_t slot, size_t source,
               int64_t begin, int64_t requestEnd)
    : Transfer(task.engine().http()),
      id(id),
      slot(slot),
      source(source),
      begin(begin),
      requestEnd(requestEnd),
      bufferOffset(begin),
      task_(task)
{
}

void Worker::onHeader(std::string_view line)
{
  if (line.starts_with("HTTP/")) {
    const auto space = line.find(' ');
    status = text::toInt(text::trim(line.substr(space + 1, 3))).value_or(0);
    etag.clear();
    lastModified.clear();
    date.clear();
    contentRange.clear();
    disposition.clear();
    contentType.clear();
    contentEncoding.clear();
    acceptRanges.clear();
    retryAfter.clear();
    contentLength = -1;
    block_ = true;
    return;
  }
  const auto value = text::trim(line);
  if (value.empty()) {
    if (block_ && status >= 200 && status < 300) {
      task_.accept(*this);
    }
    block_ = false;
    return;
  }
  const auto colon = value.find(':');
  if (colon == std::string_view::npos) {
    return;
  }
  const auto name = text::lower(value.substr(0, colon));
  const std::string field(text::trim(value.substr(colon + 1)));
  if (name == "etag") {
    etag = field;
  }
  else if (name == "last-modified") {
    lastModified = field;
  }
  else if (name == "date") {
    date = field;
  }
  else if (name == "content-range") {
    contentRange = field;
  }
  else if (name == "content-disposition") {
    disposition = field;
  }
  else if (name == "content-type") {
    contentType = field;
  }
  else if (name == "content-encoding") {
    contentEncoding = field;
  }
  else if (name == "content-length") {
    contentLength = text::toInt(field).value_or(-1);
  }
  else if (name == "accept-ranges") {
    acceptRanges = field;
  }
  else if (name == "retry-after") {
    retryAfter = field;
  }
}

size_t Worker::onData(const char* data, size_t size)
{
  if (!accepted) {
    // SFTP has no header block; its first bytes stand for the response.
    task_.accept(*this);
    if (!accepted) {
      return 0;
    }
  }
  return task_.receive(*this, data, size);
}

void Worker::onDone(CURLcode result) { task_.finished(*this, result); }

// ---------------------------------------------------------------- HttpTask --

HttpTask::HttpTask(Engine& engine, Gid gid, std::shared_ptr<Options> options,
                   std::vector<std::string> uris)
    : Task(engine, gid, std::move(options)),
      planner_(this->options().integer("min-split-size")),
      ticker_(engine.io())
{
  for (auto& uri : uris) {
    const auto parts = uri::parse(uri);
    sources_.push_back(
        Source{.uri = std::move(uri), .host = parts ? parts->host : ""});
  }
}

HttpTask::~HttpTask()
{
  for (auto& [id, worker] : workers_) {
    worker->cancel();
  }
}

bool HttpTask::parallel() const
{
  return ranged_ && !single_ && planner_.length() > 0;
}

int HttpTask::connectionTarget() const
{
  if (!parallel()) {
    return 1;
  }
  const auto perHost = options().integer("max-connection-per-server");
  int64_t hosts = 0;
  for (const auto& source : sources_) {
    hosts += source.dead ? 0 : perHost;
  }
  return static_cast<int>(std::clamp<int64_t>(
      std::min({options().integer("split"), hosts, int64_t{cap_}}), 1, 256));
}

void HttpTask::reset()
{
  for (const auto& [id, worker] : workers_) {
    worker->cancel();
  }
  workers_.clear();
  planner_.reset();
  persisted_.clear();
  std::fill(pieceSources_.begin(), pieceSources_.end(), 0);
  output_.reset();
  held_.clear();
  pendingWrites_ = 0;
  unflushed_ = 0;
  cap_ = static_cast<int>(
      std::min({options().integer("split"),
                options().integer("max-connection-per-server"),
                int64_t{initialConnections}}));
  growing_ = true;
  overloaded_ = false;
  rate_ = rateAtStep_ = 0;
  stepAt_ = Clock::now();
  slots_.assign(static_cast<size_t>(std::max(1, cap_)), Slot{});
  for (auto& source : sources_) {
    source.active = 0;
    source.failures = 0;
    source.readyAt = {};
    source.etag.clear();
    source.lastModified.clear();
  }
}

void HttpTask::start()
{
  reset();
  phase_ = Phase::Probing;
  limit_.setRate(options().integer("max-download-limit"));
  scheduleTick();

  // A stored record resumes the file this task created before; a missing
  // file means the partial download was deleted and starts over.
  if (!options().flag("remove-control-file") && !options().flag("dry-run")) {
    if (const auto record = engine().state().loadTransfer(gid());
        record && record->totalLength > 0 && fileExists(record->path)) {
      path_ = record->path;
      owned_ = true;
      etag_ = record->etag;
      lastModified_ = record->lastModified;
      planner_.setLength(record->totalLength);
      planner_.restore(record->completed);
      for (const auto& [begin, end] : record->completed) {
        persisted_.add(begin, end);
      }
      log::info("{}: resuming {} at {}/{} bytes", gidText(gid()), path_,
                persisted_.total(), record->totalLength);
      phase_ = Phase::Opening;
      openOutput(false);
      return;
    }
  }
  launch();
}

// The mirror with the fewest recent failures, then the fewest connections.
// Mirrors that are backing off (Retry-After, failures) are skipped.
std::optional<size_t> HttpTask::pickSource()
{
  const auto perHost = options().integer("max-connection-per-server");
  const auto now = Clock::now();
  std::optional<size_t> best;
  for (size_t i = 0; i < sources_.size(); ++i) {
    const auto& source = sources_[i];
    if (source.dead || source.active >= perHost || now < source.readyAt) {
      continue;
    }
    if (source.refreshing &&
        std::any_of(workers_.begin(), workers_.end(), [i](const auto& entry) {
          const auto& worker = *entry.second;
          return worker.source == i && !worker.redirected && !worker.accepted;
        })) {
      continue; // its entry point is being asked already
    }
    if (!best ||
        std::pair(source.failures, source.active) <
            std::pair(sources_[*best].failures, sources_[*best].active)) {
      best = i;
    }
  }
  return best;
}

namespace {
bool ipLiteral(const std::string& url)
{
  const auto parts = uri::parse(url);
  boost::system::error_code error;
  auto host = parts ? parts->host : std::string();
  if (host.size() > 2 && host.front() == '[') {
    host = host.substr(1, host.size() - 2);
  }
  boost::asio::ip::make_address(host, error);
  return !error;
}
} // namespace

int HttpTask::familyFor(const Source& source, int avoid) const
{
  const auto& url = source.redirect.empty() ? source.uri : source.redirect;
  if (options().flag("disable-ipv6") || ipLiteral(url)) {
    return 0;
  }
  const auto& v4 = source.families[0];
  const auto& v6 = source.families[1];
  if (v4.failed != v6.failed) {
    return v4.failed ? 6 : 4;
  }
  if (v4.failed) {
    return 0;
  }
  // Only a family that has carried a response is ever forced: networks with
  // global IPv6 addresses but no route to the server would otherwise hold
  // every forced connection until connect-timeout. Until one has, libcurl's
  // Happy Eyeballs picks the family that connects first.
  // A duplicate of a slow connection tries the other route if it works.
  if (avoid) {
    const auto& other = source.families[avoid == 4];
    return other.worked ? (avoid == 4 ? 6 : 4) : 0;
  }
  // The family that answered keeps the next connections, which then skip
  // Happy Eyeballs' head start for the other one (200 ms per connection
  // where IPv6 leads nowhere).
  if (v4.worked != v6.worked) {
    return v4.worked ? 4 : 6;
  }
  // A family several times slower per connection gets no new connections.
  if (v4.worked && v4.speed > 0 && v6.speed > 0) {
    if (v4.speed > 4 * v6.speed) {
      return 4;
    }
    if (v6.speed > 4 * v4.speed) {
      return 6;
    }
  }
  return 0;
}

// A failure on one address family while the other is untried or works is
// the route's fault rather than the server's: the family is avoided and the
// connection retries at once without spending a try.
bool HttpTask::blameFamily(Worker& worker, Source& source, int code)
{
  const auto status = worker.responseCode();
  const bool statusError =
      code == CURLE_HTTP_RETURNED_ERROR &&
      (status == 401 || status == 403 || status == 404 || status == 421);
  const bool pathError =
      code == CURLE_COULDNT_CONNECT || code == CURLE_COULDNT_RESOLVE_HOST ||
      code == CURLE_OPERATION_TIMEDOUT || code == CURLE_RECV_ERROR ||
      code == CURLE_SEND_ERROR || code == CURLE_SSL_CONNECT_ERROR ||
      statusError;
  const auto effective = worker.effectiveUrl();
  const int family =
      worker.connectedFamily() ? worker.connectedFamily() : worker.family;
  if (!pathError || !family || options().flag("disable-ipv6") ||
      ipLiteral(effective.empty() ? source.uri : effective)) {
    return false;
  }
  auto& mine = source.families[family == 6];
  const auto& other = source.families[family == 4];
  if (other.failed) {
    // Both routes failed: back to Happy Eyeballs, and the failure counts.
    mine.failed = true;
    return false;
  }
  // A status is the server's answer unless the other route has served this
  // source; a host without addresses of that family must not lose its real
  // answer to an endless detour.
  if (mine.worked || (statusError && !other.worked)) {
    return false;
  }
  if (mine.failed) {
    return true; // started before the family was blamed
  }
  mine.failed = true;
  // The next request goes straight to the destination that failed here, so
  // that it can be pinned to the other family.
  if (!effective.empty() && effective != source.uri) {
    source.redirect = effective;
  }
  log::info("{}: {} over IPv{} failed ({}); using IPv{}", gidText(gid()),
            uri::redact(effective), family,
            status >= 400
                ? fmt::format("HTTP {}", status)
                : std::string(curl_easy_strerror(static_cast<CURLcode>(code))),
            family == 4 ? 6 : 4);
  return true;
}

void HttpTask::measureFamilies()
{
  for (size_t index = 0; index < sources_.size(); ++index) {
    std::array<double, 2> sum{};
    std::array<int, 2> count{};
    for (const auto& [id, worker] : workers_) {
      const auto family = worker->connectedFamily();
      const auto* planned = planner_.worker(id);
      if (worker->source != index || !family || !worker->receiving ||
          !planned || planned->age < 2) {
        continue;
      }
      sum[family == 6] += planned->speed;
      ++count[family == 6];
    }
    for (size_t f = 0; f < 2; ++f) {
      if (count[f]) {
        sources_[index].families[f].speed = sum[f] / count[f];
      }
    }
  }
}

void HttpTask::launch()
{
  if (phase_ != Phase::Probing && phase_ != Phase::Running) {
    return;
  }
  const auto now = Clock::now();
  if (planner_.complete()) {
    return;
  }
  if (phase_ == Phase::Probing) {
    // One request learns the length, validators and the name. Its attempts
    // and retry waits are kept on slot 0, also for the hedged copy.
    const bool hedge = workers_.size() == 1 &&
                       now - workers_.begin()->second->started >= probeHedge;
    const auto maxTries = options().integer("max-tries");
    if ((!workers_.empty() && !hedge) || now < slots_[0].readyAt ||
        (maxTries > 0 && slots_[0].failures >= maxTries &&
         sources_.size() == 1)) {
      return;
    }
    if (slots_.size() < 2) {
      slots_.resize(2);
    }
    const size_t slot = slots_[0].worker ? 1 : 0;
    if (const auto source = pickSource(); source && !slots_[slot].worker) {
      const auto avoid =
          hedge ? workers_.begin()->second->connectedFamily() : 0;
      const auto id = nextWorkerId_++;
      planner_.place(id, 0, unbounded);
      startWorker(id, slot, *source, 0, unbounded, avoid);
    }
    return;
  }
  const auto target = static_cast<size_t>(connectionTarget());
  if (slots_.size() < target) {
    slots_.resize(target);
  }
  const auto maxTries = options().integer("max-tries");
  for (size_t slot = 0; slot < target; ++slot) {
    auto& entry = slots_[slot];
    // With mirrors, max-tries is spent per mirror instead.
    if (entry.worker || now < entry.readyAt ||
        (maxTries > 0 && entry.failures >= maxTries && sources_.size() == 1)) {
      continue;
    }
    const auto source = pickSource();
    if (!source) {
      break;
    }
    const auto id = nextWorkerId_;
    if (!ranged_) {
      // Without ranges the only possible request is the whole file.
      if (!workers_.empty()) {
        break;
      }
      planner_.place(id, 0,
                     planner_.length() >= 0 ? planner_.length() : unbounded);
      ++nextWorkerId_;
      startWorker(id, slot, *source, 0, unbounded);
      break;
    }
    const auto range = planner_.assign(id);
    if (!range) {
      break;
    }
    // A raced range shares its end with the copy it duplicates.
    int avoid = 0;
    for (const auto& [other, worker] : workers_) {
      const auto* planned = planner_.worker(other);
      if (planned && planned->end == range->second &&
          worker->source == *source) {
        avoid = worker->connectedFamily();
      }
    }
    ++nextWorkerId_;
    startWorker(id, slot, *source, range->first, range->second, avoid);
  }
}

void HttpTask::startWorker(int id, size_t slot, size_t sourceIndex,
                           int64_t begin, int64_t requestEnd, int avoid)
{
  auto& source = sources_[sourceIndex];
  auto worker =
      std::make_shared<Worker>(*this, id, slot, sourceIndex, begin, requestEnd);
  const auto& url = source.redirect.empty() ? source.uri : source.redirect;
  worker->redirected = !source.redirect.empty();
  try {
    engine().http().configure(*worker, options(), url, source.uri);
  }
  catch (const Error& error) {
    planner_.release(id);
    abort(error.code(), error.what());
    return;
  }
  auto* easy = worker->easy();
  worker->family = familyFor(source, avoid);
  if (worker->family) {
    curl_easy_setopt(easy, CURLOPT_IPRESOLVE,
                     worker->family == 4 ? CURL_IPRESOLVE_V4
                                         : CURL_IPRESOLVE_V6);
  }
  // Parallel ranges use separate TCP connections rather than one HTTP/2
  // connection whose flow control would cap them all.
  curl_easy_setopt(easy, CURLOPT_HTTP_VERSION,
                   connectionTarget() > 1 ? CURL_HTTP_VERSION_1_1
                                          : CURL_HTTP_VERSION_2TLS);
  const bool bounded = requestEnd != unbounded;
  const auto range = std::to_string(begin) + '-' +
                     (bounded ? std::to_string(requestEnd - 1) : std::string());
  if (ranged_) {
    curl_easy_setopt(easy, CURLOPT_RANGE, range.c_str());
  }
  // Ranges address the stored representation; decoding is for one stream.
  const bool decode = !ranged_ && options().flag("http-accept-gzip");
  curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, decode ? "" : "identity");
  curl_easy_setopt(easy, CURLOPT_HTTP_CONTENT_DECODING, decode ? 1L : 0L);
  const auto [etag, modified] = validators(source);
  const auto validator = etag.empty() ? modified : etag;
  if (ranged_ && !validator.empty()) {
    worker->header("If-Range: " + validator);
  }
  if (options().flag("dry-run")) {
    curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
  }
  slots_[slot].worker = worker;
  ++source.active;
  source.used = true;
  workers_[id] = worker;
  worker->start();
  log::debug("{}: connection {} requests {} from {}{}", gidText(gid()), id,
             ranged_ ? range : std::string("everything"), uri::redact(url),
             worker->family ? fmt::format(" over IPv{}", worker->family)
                            : std::string());
}

// A 200 answer to a ranged request. With a validator attached it means the
// file changed; otherwise the server does not serve ranges.
// A 200 to a ranged request with If-Range means the representation changed,
// unless it carries the validators we sent: then the server ignores ranges,
// and the connection that started at byte 0 carries on alone.
bool HttpTask::rangesIgnored(Worker& worker)
{
  const auto [etag, modified] = validators(sources_[worker.source]);
  const bool same = !etag.empty()       ? strongTag(worker.etag) == etag
                    : !modified.empty() ? worker.lastModified == modified
                                        : true;
  if (dropMirror(worker, same ? "it ignores byte ranges" : "its file changed")) {
    return true;
  }
  if (!same) {
    changed("validator mismatch");
    return true;
  }
  // Without ranges the only way forward rewrites from byte 0, which would
  // destroy a prefix this task does not own.
  if (!owned_ && !path_.empty()) {
    abort(ErrorCode::CannotResume,
          fmt::format("The server ignores byte ranges; {} was left unchanged",
                      path_));
    return true;
  }
  log::notice("{}: server ignores byte ranges; using one connection",
              gidText(gid()));
  ranged_ = false;
  cancelWorker(worker.id);
  return true;
}

bool HttpTask::compressedBody(const Worker& worker) const
{
  if (worker.contentEncoding.empty() ||
      text::iequals(worker.contentEncoding, "identity") ||
      !options().flag("http-accept-gzip") || planner_.length() >= 0) {
    return false;
  }
  auto name = safeFileName(contentDispositionFileName(worker.disposition));
  if (name.empty()) {
    name = uri::fileName(worker.effectiveUrl());
  }
  const auto extension = text::lower(toPath(name).extension().string());
  return extension != ".gz" && extension != ".tgz" && extension != ".br" &&
         extension != ".zst" && extension != ".z";
}

void HttpTask::accept(Worker& worker)
{
  if (worker.accepted || worker.cancelled()) {
    return;
  }
  auto& source = sources_[worker.source];
  int64_t total = -1;
  int64_t start = 0;
  if (worker.status == 206) {
    const auto range = parseContentRange(worker.contentRange);
    start = range ? range->first : -1;
    total = range ? range->total : -1;
    if (start != worker.begin) {
      log::warn("{}: response range starts at {} instead of {}", gidText(gid()),
                start, worker.begin);
      const auto slot = worker.slot;
      cancelWorker(worker.id);
      slotFailed(slot, ErrorCode::HttpProtocol, "Mismatched Content-Range");
      return;
    }
  }
  else if (worker.status == 0) {
    // SFTP: the size of the remaining file, ranges always honoured.
    curl_off_t length = -1;
    curl_easy_getinfo(worker.easy(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                      &length);
    total = length >= 0 ? worker.begin + length : -1;
  }
  else {
    total = worker.contentLength;
    if (worker.begin > 0 && rangesIgnored(worker)) {
      return;
    }
  }
  if (declared_ >= 0 && total >= 0 && total != declared_) {
    // A mirror with another length than the Metalink states serves another
    // file, such as an outdated copy.
    if (!dropMirror(worker,
                    fmt::format("{} bytes instead of {}", total, declared_))) {
      abort(ErrorCode::HttpProtocol,
            fmt::format("No mirror has the stated length {}", declared_));
    }
    return;
  }
  if (compressedBody(worker)) {
    // The server compressed although identity was asked for. With
    // http-accept-gzip the file is fetched again as one decoded stream, as a
    // browser would save it; an archive's own name keeps its bytes as sent.
    if (ranged_) {
      ranged_ = false;
      cancelWorker(worker.id);
      return;
    }
    total = -1; // Content-Length counts the compressed bytes
  }
  const auto tag = strongTag(worker.etag);
  const bool lengthChanged =
      planner_.length() >= 0 && total >= 0 && total != planner_.length();
  // Validators are compared within one origin; mirrors legitimately differ.
  const bool tagChanged = sources_.size() == 1 && !single_ && !etag_.empty() &&
                          !tag.empty() && tag != etag_;
  const bool timeChanged = sources_.size() == 1 && !single_ && etag_.empty() &&
                           !lastModified_.empty() &&
                           !worker.lastModified.empty() &&
                           worker.lastModified != lastModified_;
  if (lengthChanged && dropMirror(worker, "another length")) {
    return;
  }
  if (lengthChanged || tagChanged || timeChanged) {
    changed(lengthChanged ? "length changed" : "validator changed");
    return;
  }
  worker.accepted = true;
  source.failures = 0;
  if (source.etag.empty() && source.lastModified.empty()) {
    source.etag = tag;
    source.lastModified = strongDate(worker.lastModified, worker.date);
  }
  if (const auto family = worker.connectedFamily()) {
    source.families[family == 6].worked = true;
  }
  if (!worker.redirected) {
    source.refreshing = false;
    if (auto effective = worker.effectiveUrl();
        !effective.empty() && effective != source.uri) {
      source.redirect = std::move(effective);
    }
  }
  if (phase_ != Phase::Probing) {
    return;
  }
  for (const auto& [id, other] : std::map(workers_)) {
    if (id != worker.id) {
      cancelWorker(id); // the hedged probe that lost
    }
  }
  // The first response defines the download.
  if (total >= 0) {
    planner_.setLength(total);
  }
  if (worker.status == 200 && (worker.contentLength < 0 ||
                               !text::iequals(worker.acceptRanges, "bytes"))) {
    ranged_ = false;
  }
  if (options().get("media") == "auto" &&
      media::MediaTask::manifestType(worker.contentType) &&
      !options().flag("dry-run")) {
    handOverToMedia();
    return;
  }
  etag_ = tag;
  lastModified_ = strongDate(worker.lastModified, worker.date);
  contentType_ = worker.contentType;
  if (!worker.lastModified.empty()) {
    remoteTime_ = curl_getdate(worker.lastModified.c_str(), nullptr);
  }
  if (options().flag("dry-run")) {
    phase_ = Phase::Done;
    cancelWorker(worker.id);
    complete();
    return;
  }
  phase_ = Phase::Opening;
  if (path_.empty()) {
    decidePath(worker);
  }
  else {
    // Restart of a task that already chose its file.
    openOutput(owned_ && persisted_.empty());
  }
}

// A mirror that disagrees with the download (another length, ignored
// ranges, a changed file) is dropped while others remain.
bool HttpTask::dropMirror(Worker& worker, const std::string& why)
{
  auto& source = sources_[worker.source];
  if (!std::any_of(sources_.begin(), sources_.end(), [&](const Source& s) {
        return &s != &source && !s.dead;
      })) {
    return false;
  }
  log::warn("{}: not using {}: {}", gidText(gid()), uri::redact(source.uri),
            why);
  source.dead = true;
  const auto slot = worker.slot;
  cancelWorker(worker.id);
  slotFailed(slot, ErrorCode::HttpProtocol, why);
  return true;
}

// If-Range validators: a single source has the download's; a mirror only
// its own, once it has answered.
std::pair<std::string, std::string>
HttpTask::validators(const Source& source) const
{
  if (sources_.size() > 1) {
    return {source.etag, source.lastModified};
  }
  return {strongTag(etag_), lastModified_};
}

bool HttpTask::decidePath(Worker& worker)
{
  auto name = options().get("out");
  if (name.empty()) {
    const auto hint = safeFileName(options().get("filename-hint"));
    const auto disposition =
        safeFileName(contentDispositionFileName(worker.disposition));
    if (!hint.empty() && options().get("filename-hint-source") == "browser") {
      name = hint;
    }
    else if (!disposition.empty()) {
      name = disposition;
    }
    else if (!hint.empty()) {
      name = hint;
    }
    else {
      name = uri::fileName(sources_[worker.source].uri);
      if (name.empty()) {
        name = uri::fileName(worker.effectiveUrl());
      }
    }
    if (name.empty()) {
      name = "index.html";
    }
  }
  auto path = joinPath(options().get("dir"), name);
  const auto taken = [this](const std::string& candidate) {
    return engine().writing(candidate, gid());
  };
  // Another task's file is never adopted, overwritten or shared.
  const bool shared = taken(path);
  if (shared || fileExists(path)) {
    if (!shared && options().flag("continue")) {
      // An existing file is adopted as a prefix and never truncated.
      const auto size = fileSize(path);
      const auto length = planner_.length();
      if (length >= 0 && size > length) {
        abort(ErrorCode::CannotResume,
              fmt::format("{} is larger than the remote file", path));
        return false;
      }
      path_ = path;
      owned_ = false;
      if (size > 0 && ranged_) {
        planner_.restore({{0, size}});
        persisted_.add(0, size);
        cancelWorker(worker.id); // ranges after the prefix follow
      }
      openOutput(false);
      return true;
    }
    if (shared || !options().flag("allow-overwrite")) {
      if (!options().flag("auto-file-renaming")) {
        abort(ErrorCode::FileExists,
              shared ? fmt::format("{} belongs to another download", path)
                     : fmt::format("{} already exists", path));
        return false;
      }
      auto numbered = numberedPath(path, taken);
      if (!numbered) {
        abort(ErrorCode::FileExists, fmt::format("{} already exists", path));
        return false;
      }
      path = std::move(*numbered);
    }
  }
  path_ = path;
  owned_ = true;
  openOutput(true);
  return true;
}

void HttpTask::openOutput(bool truncate)
{
  output_ = std::make_shared<Output>(engine().disk(), path_);
  const auto allocation = parseAllocation(options().get("file-allocation"));
  output_->open(truncate, allocation, planner_.length(),
                [this, self = shared_from_this()](std::error_code error) {
                  outputReady(error);
                });
}

void HttpTask::outputReady(std::error_code error)
{
  if (phase_ != Phase::Opening) {
    quiesce();
    return;
  }
  if (error) {
    abort(ErrorCode::FileCreate,
          fmt::format("Cannot open {}: {}", path_, error.message()));
    return;
  }
  phase_ = Phase::Running;
  checkpoint();
  for (auto& [offset, data] : std::exchange(held_, {})) {
    write(offset, std::move(data));
  }
  for (const auto& [id, worker] : std::map(workers_)) {
    flush(*worker);
  }
  tryFinish();
  launch();
}

size_t HttpTask::receive(Worker& worker, const char* data, size_t size)
{
  if (phase_ == Phase::Stopping || phase_ == Phase::Done ||
      phase_ == Phase::Restarting || worker.done) {
    return 0;
  }
  auto& global = engine().http().downloadLimit();
  if (unflushed_ > writeBudget || !limit_.available() || !global.available()) {
    if (unflushed_ > writeBudget) {
      // Buffered bytes count against the budget too; only writing frees it.
      for (const auto& [id, other] : workers_) {
        flush(*other);
      }
    }
    return CURL_WRITEFUNC_PAUSE; // tick() resumes it
  }
  size_t consumed = 0;
  while (consumed < size) {
    const auto before = planner_.worker(worker.id)->position;
    const auto usable = static_cast<size_t>(
        planner_.receive(worker.id, static_cast<int64_t>(size - consumed)));
    if (usable > 0) {
      if (!pieceSources_.empty() && worker.source < 64) {
        const auto last = std::min<int64_t>(
            (before + usable - 1) / pieces_.length,
            static_cast<int64_t>(pieceSources_.size()) - 1);
        for (auto piece = before / pieces_.length; piece <= last; ++piece) {
          pieceSources_[static_cast<size_t>(piece)] |= uint64_t{1}
                                                       << worker.source;
        }
      }
      if (worker.buffer.empty()) {
        worker.bufferOffset = before;
        worker.bufferedSince = Clock::now();
      }
      worker.buffer.append(data + consumed, usable);
      consumed += usable;
      continue;
    }
    // The assignment ended inside this chunk: take over what follows, or stop.
    std::optional<int> taken;
    if (!planner_.extend(worker.id, taken)) {
      // Bytes past the end of the file: the announced length is wrong, and
      // so is whatever the server said about ranges with it.
      if (planner_.length() >= 0 &&
          planner_.worker(worker.id)->position >= planner_.length()) {
        rangesDistrusted_ = true;
        changed("the server sent more than the length it announced");
        return 0;
      }
      worker.done = true;
      break;
    }
    if (taken) {
      cancelWorker(*taken);
    }
  }
  const auto now = Clock::now();
  const auto amount = static_cast<int64_t>(consumed);
  if (!worker.receiving) {
    planner_.noteStartup(
        std::chrono::duration<double>(now - worker.started).count());
  }
  worker.lastData = now;
  worker.receiving = true;
  worker.tickBytes += amount;
  worker.received += amount;
  slots_[worker.slot].failures = 0;
  speed_.add(amount, now);
  limit_.charge(amount, now);
  global.charge(amount, now);
  unflushed_ += amount;
  // A raced range finished elsewhere: the slower copy gives way.
  for (const auto id : planner_.redundant()) {
    if (id == worker.id) {
      worker.done = true;
    }
    else {
      cancelWorker(id);
    }
  }
  // Many connections share the budget, so each buffers less.
  const auto threshold = std::clamp<int64_t>(
      writeBudget /
          (4 * static_cast<int64_t>(std::max<size_t>(workers_.size(), 1))),
      minFlushSize, maxFlushSize);
  if (static_cast<int64_t>(worker.buffer.size()) >= threshold || worker.done) {
    flush(worker);
  }
  if (worker.done) {
    // Decided before tryFinish(), which may complete the download and
    // release every worker from the planner, this one included.
    const auto* planned = planner_.worker(worker.id);
    const bool ends = consumed == size && planned &&
                      planned->position >= worker.requestEnd;
    tryFinish();
    return ends ? size // the response ends here by itself
                : 0;   // abort; finished() treats it as success
  }
  return consumed;
}

void HttpTask::flush(Worker& worker)
{
  if (worker.buffer.empty()) {
    return;
  }
  if (phase_ == Phase::Restarting) {
    unflushed_ -= static_cast<int64_t>(worker.buffer.size());
    worker.buffer.clear();
    return;
  }
  const auto offset = worker.bufferOffset;
  const auto size = worker.buffer.size();
  worker.bufferOffset = offset + static_cast<int64_t>(size);
  if (output_ && phase_ != Phase::Probing && phase_ != Phase::Opening) {
    write(offset, std::exchange(worker.buffer, {}));
  }
  else {
    // The task keeps it: the worker may finish before the file is open.
    held_.emplace_back(offset, std::exchange(worker.buffer, {}));
  }
}

void HttpTask::write(int64_t offset, std::string data)
{
  const auto size = data.size();
  ++pendingWrites_;
  output_->write(offset, std::move(data),
                 [this, self = shared_from_this(), offset, size](
                     std::error_code error) { written(offset, size, error); });
}

void HttpTask::written(int64_t offset, size_t size, std::error_code error)
{
  --pendingWrites_;
  unflushed_ -= static_cast<int64_t>(size);
  if (phase_ == Phase::Restarting) {
    restartWhenQuiet();
    return;
  }
  if (error) {
    abort(ErrorCode::FileIo,
          fmt::format("Cannot write {}: {}", path_, error.message()));
    return;
  }
  persisted_.add(offset, offset + static_cast<int64_t>(size));
  if (phase_ == Phase::Stopping) {
    quiesce();
    return;
  }
  tryFinish();
}

void HttpTask::finished(Worker& worker, int code)
{
  flush(worker);
  const auto* planned = planner_.worker(worker.id);
  if (phase_ == Phase::Stopping || phase_ == Phase::Done ||
      phase_ == Phase::Restarting) {
    retireWorker(worker);
    quiesce();
    return;
  }
  const bool ok =
      code == CURLE_OK || (worker.done && code == CURLE_WRITE_ERROR);
  if (!ok) {
    failedResponse(worker, code);
    return;
  }
  if (planned && planner_.length() < 0) {
    // A stream without a declared length ends where the server stops.
    planner_.setLength(planned->position);
  }
  const bool short_ = code == CURLE_OK && planned && !worker.done &&
                      planned->position < planned->end;
  const auto slot = worker.slot;
  retireWorker(worker);
  if (short_) {
    slotFailed(slot, ErrorCode::Network,
               "The connection closed before the range was complete");
    return;
  }
  tryFinish();
  launch();
}

void HttpTask::failedResponse(Worker& worker, int code)
{
  const auto status = worker.responseCode();
  const auto slot = worker.slot;
  auto& source = sources_[worker.source];
  const bool redirected = worker.redirected;
  const auto retryAfter = worker.retryAfter;
  if (blameFamily(worker, source, code)) {
    retireWorker(worker);
    slots_[slot].readyAt = {};
    launch();
    return;
  }
  retireWorker(worker);
  if (code == CURLE_HTTP_RETURNED_ERROR && redirected &&
      (status == 401 || status == 403 || status == 404 || status == 410)) {
    // An expired signed destination: go back through the original URI, one
    // request at a time until it hands out a new destination.
    if (!source.redirect.empty()) {
      log::info("{}: {} expired (HTTP {}); resolving {} again", gidText(gid()),
                uri::redact(source.redirect), status, uri::redact(source.uri));
      source.redirect.clear();
      source.refreshing = true;
    }
    launch();
    return;
  }
  // A mirror that keeps failing is given up for the others (max-tries per
  // mirror); a single source is governed by the connection slots.
  const bool mirrors = sources_.size() > 1;
  const auto maxTries = options().integer("max-tries");
  ++source.failures;
  if (mirrors) {
    source.readyAt = Clock::now() +
                     backoff(source.failures, options().integer("retry-wait"));
    if (maxTries > 0 && source.failures >= maxTries) {
      source.dead = true;
    }
  }
  const auto allDead = [this] {
    return std::all_of(sources_.begin(), sources_.end(),
                       [](const Source& s) { return s.dead; });
  };
  if (code == CURLE_HTTP_RETURNED_ERROR) {
    if (status == 429 || status == 503) {
      int transferring = 0;
      for (const auto& [id, other] : workers_) {
        transferring += other->receiving ? 1 : 0;
      }
      // An entry point handing out destinations is not the server that
      // carries the transfers, and one mirror's overload is not the
      // others': only that source waits then.
      if (!mirrors && (!source.refreshing || redirected)) {
        cap_ = std::max(1, transferring);
        growing_ = false;
        overloaded_ = true;
        calmSince_ = Clock::now();
      }
      auto wait =
          backoff(slots_[slot].failures, options().integer("retry-wait"));
      if (const auto seconds = text::toInt(retryAfter)) {
        // Retry-After raises the configured wait, never shortens it.
        wait = std::max<Clock::duration>(
            wait, std::chrono::seconds(std::min<int64_t>(*seconds, 600)));
      }
      // The server asked for a pause: no new request reaches it before the
      // wait; running transfers continue.
      source.readyAt = Clock::now() + wait;
      log::info(
          "{}: server overloaded (HTTP {}); {} connection(s), retry in {} ms",
          gidText(gid()), status, cap_,
          std::chrono::duration_cast<std::chrono::milliseconds>(wait).count());
      if (allDead()) {
        abort(ErrorCode::ServiceUnavailable, fmt::format("HTTP {}", status));
        return;
      }
      if (transferring == 0) {
        // Nothing gets through: this counts as a failed attempt.
        slotFailed(slot, ErrorCode::ServiceUnavailable,
                   fmt::format("HTTP {}", status), wait);
      }
      else if (!mirrors) {
        slots_[slot].readyAt = Clock::now() + wait;
      }
      return;
    }
    if (status == 416 && planner_.length() >= 0) {
      changed("range not satisfiable");
      return;
    }
    if (status == 412 && planner_.length() >= 0) {
      changed("precondition failed");
      return;
    }
    // Client errors on a source are its answer: like aria2's 404 handling,
    // max-file-not-found of them end the source even with unlimited tries.
    if (status == 403 || status == 404 || status == 410 || status == 451) {
      if (++source.notFound >=
              std::max<int64_t>(options().integer("max-file-not-found"), 1) ||
          sources_.size() > 1) {
        source.dead = true;
      }
      const auto error = status == 403 ? ErrorCode::HttpProtocol
                                       : ErrorCode::ResourceNotFound;
      if (allDead()) {
        abort(error, fmt::format("HTTP {}", status));
        return;
      }
      slotFailed(slot, error, fmt::format("HTTP {}", status));
      return;
    }
    if (status == 401 || status == 407) {
      abort(ErrorCode::HttpAuth,
            fmt::format("HTTP {}: authorization failed", status));
      return;
    }
    slotFailed(slot,
               status >= 500 ? ErrorCode::Network : ErrorCode::HttpProtocol,
               fmt::format("HTTP {}", status));
    return;
  }
  std::string message = worker.errorText();
  if (message.empty()) {
    message = curl_easy_strerror(static_cast<CURLcode>(code));
  }
  if (permanent(code)) {
    source.dead = true;
    if (allDead()) {
      abort(curlError(code), message);
      return;
    }
  }
  slotFailed(slot, curlError(code), message);
}

void HttpTask::retireWorker(Worker& worker)
{
  if (worker.received > 0) {
    const auto family = worker.connectedFamily();
    log::debug("{}: connection {} received {} bytes over IPv{}", gidText(gid()),
               worker.id, worker.received, family);
    const auto seconds =
        std::chrono::duration<double>(Clock::now() - worker.started).count();
    if (seconds > 0) {
      const auto rate = static_cast<double>(worker.received) / seconds;
      planner_.noteSpeed(rate);
      if (family) {
        auto& measured = sources_[worker.source].families[family == 6].speed;
        measured = measured > 0 ? (measured + rate) / 2 : rate;
      }
    }
  }
  planner_.release(worker.id);
  auto& source = sources_[worker.source];
  source.active = std::max(0, source.active - 1);
  if (worker.slot < slots_.size() &&
      slots_[worker.slot].worker.get() == &worker) {
    slots_[worker.slot].worker.reset();
  }
  workers_.erase(worker.id);
}

void HttpTask::slotFailed(size_t slot, ErrorCode code, std::string message,
                          Clock::duration wait)
{
  lastError_ = code;
  lastMessage_ = std::move(message);
  auto& entry = slots_[phase_ == Phase::Probing ? 0 : slot];
  ++entry.failures;
  // Mirrors back off individually; the connection moves on to another.
  const auto delay =
      sources_.size() > 1 ? Clock::duration{}
      : wait.count()      ? wait
                          : backoff(entry.failures, options().integer("retry-wait"));
  entry.readyAt = Clock::now() + delay;
  log::info(
      "{}: connection failed ({}), attempt {}, retry in {} ms", gidText(gid()),
      lastMessage_, entry.failures,
      std::chrono::duration_cast<std::chrono::milliseconds>(delay).count());
  const auto maxTries = options().integer("max-tries");
  if (maxTries <= 0) {
    return;
  }
  if (sources_.size() > 1) {
    // Mirrors fail individually; the task fails with the last of them.
    if (workers_.empty() &&
        std::all_of(sources_.begin(), sources_.end(),
                    [](const Source& s) { return s.dead; })) {
      abort(lastError_, lastMessage_);
    }
    return;
  }
  // The task fails once no connection has tries left and none is running.
  const auto exhausted = [maxTries](const Slot& s) {
    return s.failures >= maxTries;
  };
  if (workers_.empty() &&
      (phase_ == Phase::Probing
           ? exhausted(slots_[0])
           : std::all_of(slots_.begin(), slots_.end(), exhausted))) {
    abort(lastError_, lastMessage_);
  }
}

void HttpTask::cancelWorker(int id)
{
  const auto found = workers_.find(id);
  if (found == workers_.end()) {
    planner_.release(id);
    return;
  }
  auto worker = found->second;
  worker->cancel();
  flush(*worker);
  retireWorker(*worker);
}

void HttpTask::changed(const std::string& why)
{
  // Stored bytes are kept only while they provably belong to the same file.
  if (!owned_ && !path_.empty()) {
    abort(ErrorCode::CannotResume,
          fmt::format("The remote file changed ({}); {} was left unchanged",
                      why, path_));
    return;
  }
  if (++restarts_ > maxRestarts) {
    abort(ErrorCode::CannotResume,
          fmt::format("The remote file keeps changing ({})", why));
    return;
  }
  log::notice("{}: {}; restarting from the beginning", gidText(gid()), why);
  // Sources that disagree with each other are served by one stream from now.
  single_ = single_ || restarts_ > 1 || rangesDistrusted_;
  phase_ = Phase::Restarting;
  for (const auto& [id, worker] : std::map(workers_)) {
    worker->cancel();
    retireWorker(*worker);
  }
  engine().state().removeTransfer(gid());
  restartWhenQuiet();
}

// Bytes of the old representation still queued for the disk must land before
// the file is truncated for the new one.
void HttpTask::restartWhenQuiet()
{
  if (phase_ != Phase::Restarting || pendingWrites_ > 0) {
    return;
  }
  auto restart = [this, self = shared_from_this()](std::error_code) {
    if (phase_ != Phase::Restarting) {
      quiesce();
      return;
    }
    const auto path = path_;
    reset();
    etag_.clear();
    lastModified_.clear();
    ranged_ = !rangesDistrusted_;
    path_ = path; // reopened with truncation by the next first response
    phase_ = Phase::Probing;
    launch();
  };
  if (output_) {
    output_->close(std::move(restart));
  }
  else {
    // Called from a response callback, where curl refuses new handles.
    boost::asio::post(engine().io(),
                      [restart = std::move(restart)] { restart({}); });
  }
}

void HttpTask::tryFinish()
{
  if (phase_ != Phase::Running || !planner_.complete()) {
    return;
  }
  // A ranged answer without Content-Length may carry more than its
  // Content-Range announced; it must end by itself so receive() sees that.
  for (const auto& [id, worker] : workers_) {
    if (worker->accepted && !worker->done && worker->status == 206 &&
        worker->contentLength < 0) {
      return; // finished() calls back
    }
  }
  for (const auto& [id, worker] : std::map(workers_)) {
    cancelWorker(id);
  }
  if (pendingWrites_ > 0 || unflushed_ > 0) {
    return; // written() calls back
  }
  phase_ = Phase::Finishing;
  output_->close([this, self = shared_from_this()](std::error_code) {
    output_.reset();
    verifyAndComplete();
  });
}

void HttpTask::verifyAndComplete()
{
  if (phase_ != Phase::Finishing) {
    quiesce();
    return;
  }
  if (remoteTime_ > 0 && options().flag("remote-time")) {
    setModificationTime(path_, remoteTime_);
  }
  const auto checksum = options().get("checksum");
  if (checksum.empty()) {
    if (pieces_.hashes.empty()) {
      succeed();
    }
    else {
      checkPieces({});
    }
    return;
  }
  verifyChecksum(engine().disk(), path_, checksum,
                 [this, self = shared_from_this()](
                     std::optional<std::string> failure) {
                   if (phase_ != Phase::Finishing) {
                     quiesce();
                   }
                   else if (!failure) {
                     succeed();
                   }
                   else if (!pieces_.hashes.empty()) {
                     checkPieces(std::move(*failure));
                   }
                   else {
                     phase_ = Phase::Running;
                     abort(ErrorCode::Checksum, *failure);
                   }
                 });
}

// Finds the pieces behind a failed (or absent) whole-file hash.
void HttpTask::checkPieces(std::string failure)
{
  auto strand = engine().disk().strand();
  engine().disk().run(
      strand,
      [path = path_, pieces = pieces_] { return badPieces(path, pieces); },
      [this, self = shared_from_this(),
       failure = std::move(failure)](std::optional<std::vector<size_t>> bad) {
        if (phase_ != Phase::Finishing) {
          quiesce();
          return;
        }
        if (bad && bad->empty() && failure.empty()) {
          succeed();
          return;
        }
        if (bad && !bad->empty() && repairs_ < maxRepairs) {
          repair(*bad);
          return;
        }
        phase_ = Phase::Running;
        abort(ErrorCode::Checksum,
              !bad          ? fmt::format("Cannot check the {} piece hashes",
                                          pieces_.type)
              : bad->empty() ? failure
                             : fmt::format("{} piece(s) failed their hash",
                                           bad->size()));
      });
}

// Bad pieces are downloaded again. A mirror that alone delivered one is not
// asked again while others remain.
void HttpTask::repair(const std::vector<size_t>& bad)
{
  ++repairs_;
  log::warn("{}: {} piece(s) failed their hash; downloading them again",
            gidText(gid()), bad.size());
  const auto length = planner_.length();
  for (const auto piece : bad) {
    const auto begin = static_cast<int64_t>(piece) * pieces_.length;
    const auto end = std::min(begin + pieces_.length, length);
    planner_.forget(begin, end);
    persisted_.remove(begin, end);
    const auto mask = std::exchange(pieceSources_[piece], 0);
    if (std::popcount(mask) != 1) {
      continue;
    }
    auto& source = sources_[static_cast<size_t>(std::countr_zero(mask))];
    const auto alive = std::count_if(sources_.begin(), sources_.end(),
                                     [](const Source& s) { return !s.dead; });
    if (!source.dead && alive > 1) {
      log::warn("{}: {} delivered a corrupt piece; not using it",
                gidText(gid()), uri::redact(source.uri));
      source.dead = true;
    }
  }
  phase_ = Phase::Opening;
  openOutput(false);
}

void HttpTask::succeed()
{
  phase_ = Phase::Done;
  engine().state().removeTransfer(gid());
  log::notice("Download complete: {}", path_);
  complete();
}

void HttpTask::setMetalink(int64_t size, PieceHashes pieces)
{
  declared_ = size;
  pieces_ = std::move(pieces);
  pieceSources_.assign(pieces_.hashes.size(), 0);
}

void HttpTask::abort(ErrorCode code, std::string message)
{
  if (phase_ == Phase::Done || phase_ == Phase::Stopping) {
    return;
  }
  phase_ = Phase::Done;
  for (const auto& [id, worker] : std::map(workers_)) {
    worker->cancel();
    flush(*worker);
    retireWorker(*worker);
  }
  checkpoint();
  if (output_) {
    output_->close([](std::error_code) {});
  }
  failed(code, std::move(message));
}

// The response is an HLS or DASH manifest: the presentation is downloaded
// and remuxed by a media task with the same GID instead.
void HttpTask::handOverToMedia()
{
  log::info("{}: {} is a media manifest", gidText(gid()),
            uri::redact(sources_.front().uri));
  phase_ = Phase::Done;
  ticker_.cancel();
  for (const auto& [id, worker] : std::map(workers_)) {
    cancelWorker(id);
  }
  auto next = std::make_shared<media::MediaTask>(
      engine(), gid(), sharedOptions(), sources_.front().uri);
  auto& owner = engine();
  boost::asio::post(owner.io(), [&owner, self = shared_from_this(), next] {
    owner.handOver(self, next);
  });
}

void HttpTask::stop(bool, std::function<void()> done)
{
  stopped_ = std::move(done);
  phase_ = Phase::Stopping;
  ticker_.cancel();
  tickScheduled_ = false;
  for (const auto& [id, worker] : std::map(workers_)) {
    worker->cancel();
    flush(*worker);
    retireWorker(*worker);
  }
  quiesce();
}

void HttpTask::quiesce()
{
  if (phase_ != Phase::Stopping || pendingWrites_ > 0 || !stopped_) {
    return;
  }
  checkpoint();
  auto finish = [this, self = shared_from_this(),
                 done = std::exchange(stopped_, nullptr)](std::error_code) {
    output_.reset();
    phase_ = Phase::Idle;
    done();
  };
  if (output_) {
    output_->close(std::move(finish));
  }
  else {
    finish({});
  }
}

void HttpTask::scheduleTick()
{
  if (tickScheduled_) {
    return;
  }
  tickScheduled_ = true;
  ticker_.expires_after(tickInterval);
  ticker_.async_wait(
      [this, self = weak_from_this()](const boost::system::error_code& error) {
        if (error || self.expired()) {
          return;
        }
        tickScheduled_ = false;
        tick();
      });
}

void HttpTask::tick()
{
  if (phase_ != Phase::Probing && phase_ != Phase::Opening &&
      phase_ != Phase::Running) {
    return;
  }
  const auto now = Clock::now();
  auto& global = engine().http().downloadLimit();
  measureFamilies();
  int64_t tickBytes = 0;
  int transferring = 0;
  for (const auto& [id, worker] : std::map(workers_)) {
    planner_.updateSpeed(
        id, static_cast<double>(worker->tickBytes) / tickSeconds, tickSeconds);
    tickBytes += worker->tickBytes;
    transferring += worker->receiving && !worker->paused() ? 1 : 0;
    worker->tickBytes = 0;
    if (!worker->buffer.empty() && now - worker->bufferedSince >= flushAfter) {
      flush(*worker);
    }
    if (worker->paused()) {
      if (unflushed_ <= writeBudget && limit_.available() &&
          global.available()) {
        worker->pause(false);
        worker->lastData = now;
      }
      continue;
    }
    if (!worker->connected && worker->connectedFamily()) {
      worker->connected = true;
      worker->lastData = now;
    }
    if (worker->connected && now - worker->lastData > stallAfter) {
      log::info("{}: connection {} stalled", gidText(gid()), id);
      const auto slot = worker->slot;
      cancelWorker(id);
      slotFailed(slot, ErrorCode::TimedOut, "Connection stalled", 1s);
    }
  }
  const auto configured = static_cast<int>(
      std::min(options().integer("split"),
               options().integer("max-connection-per-server")));
  const auto sample = static_cast<double>(tickBytes) / tickSeconds;
  rate_ = rate_ > 0 ? rate_ * 0.5 + sample * 0.5 : sample;
  if (growing_ && cap_ < configured && transferring >= cap_ &&
      now - stepAt_ >= growthPeriod) {
    if (rate_ >= rateAtStep_ * growthGain) {
      rateAtStep_ = rate_;
      cap_ = std::min(configured, cap_ * 2);
      stepAt_ = now;
    }
    else {
      growing_ = false; // the route is full; more connections only add load
    }
  }
  if (overloaded_ && cap_ < configured && now - calmSince_ > calmPeriod) {
    ++cap_;
    calmSince_ = now;
  }
  launch();
  scheduleTick();
}

void HttpTask::checkpoint()
{
  if (path_.empty() || planner_.length() <= 0 || options().flag("dry-run") ||
      phase_ == Phase::Restarting ||
      (phase_ == Phase::Done && state() == TaskState::Complete)) {
    return;
  }
  engine().state().saveTransfer({gid(), sources_.front().uri, path_, etag_,
                                 lastModified_, planner_.length(),
                                 persisted_.ranges()});
}

void HttpTask::loadProgress()
{
  if (options().flag("remove-control-file") || options().flag("dry-run")) {
    return;
  }
  if (const auto record = engine().state().loadTransfer(gid());
      record && record->totalLength > 0 && fileExists(record->path)) {
    path_ = record->path;
    owned_ = true;
    planner_.setLength(record->totalLength);
    planner_.restore(record->completed);
  }
}

void HttpTask::discardState() { engine().state().removeTransfer(gid()); }

void HttpTask::optionChanged(std::string_view name)
{
  if (name == "max-download-limit") {
    limit_.setRate(options().integer("max-download-limit"));
  }
}

bool HttpTask::changeUris(int fileIndex, const std::vector<std::string>& remove,
                          const std::vector<std::string>& add, int position)
{
  if (fileIndex != 1) {
    return false;
  }
  for (const auto& uri : remove) {
    const auto found =
        std::find_if(sources_.begin(), sources_.end(),
                     [&uri](const Source& s) { return s.uri == uri; });
    if (found != sources_.end() && found->active == 0) {
      sources_.erase(found);
    }
  }
  auto at = std::clamp<int64_t>(
      position < 0 ? static_cast<int64_t>(sources_.size()) : position, 0,
      static_cast<int64_t>(sources_.size()));
  for (const auto& uri : add) {
    const auto parts = uri::parse(uri);
    sources_.insert(sources_.begin() + at++,
                    Source{.uri = uri, .host = parts ? parts->host : ""});
  }
  if (state() == TaskState::Active) {
    launch();
  }
  return true;
}

int64_t HttpTask::totalLength() const
{
  return std::max<int64_t>(planner_.length(), 0);
}

int64_t HttpTask::completedLength() const
{
  return planner_.received().total();
}

int64_t HttpTask::downloadSpeed() const { return speed_.rate(); }

int HttpTask::connections() const
{
  return static_cast<int>(
      std::count_if(workers_.begin(), workers_.end(),
                    [](const auto& entry) { return entry.second->receiving; }));
}

std::vector<TaskFile> HttpTask::files() const
{
  TaskFile file;
  file.path = path_.empty() && !options().blank("out")
                  ? joinPath(options().get("dir"), options().get("out"))
                  : path_;
  file.length = totalLength();
  file.completed = completedLength();
  for (const auto& source : sources_) {
    file.uris.push_back({source.uri, source.used});
  }
  return {file};
}

void HttpTask::status(json::object& out, const StatusKeys& keys) const
{
  Task::status(out, keys);
  const auto length = planner_.length();
  if (length <= 0) {
    return;
  }
  // A coarse completion map for progress bars.
  int64_t pieceLength = 1 << 20;
  while ((length + pieceLength - 1) / pieceLength > 4096) {
    pieceLength *= 2;
  }
  const auto pieces = (length + pieceLength - 1) / pieceLength;
  if (keys.want("pieceLength")) {
    out["pieceLength"] = std::to_string(pieceLength);
  }
  if (keys.want("numPieces")) {
    out["numPieces"] = std::to_string(pieces);
  }
  if (keys.want("bitfield")) {
    std::string bits(static_cast<size_t>((pieces + 7) / 8), '\0');
    for (int64_t piece = 0; piece < pieces; ++piece) {
      const auto begin = piece * pieceLength;
      if (planner_.received().contains(begin,
                                       std::min(length, begin + pieceLength))) {
        bits[piece / 8] =
            static_cast<char>(bits[piece / 8] | (0x80 >> (piece % 8)));
      }
    }
    out["bitfield"] = text::toHex(bits);
  }
}

std::vector<std::string> HttpTask::sessionUris() const
{
  std::vector<std::string> uris;
  for (const auto& source : sources_) {
    uris.push_back(source.uri);
  }
  return uris;
}

} // namespace aria2::http
