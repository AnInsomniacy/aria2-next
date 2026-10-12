/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "ed2k/Kad.h"

#include "base/Log.h"
#include "base/Text.h"
#include "base/Path.h"
#include "ed2k/Client.h"
#include "ed2k/Compression.h"
#include "ed2k/Constants.h"
#include "ed2k/Ed2kTask.h"
#include "ed2k/Endpoint.h"
#include "ed2k/Network.h"
#include "ed2k/Packet.h"
#include "ed2k/Search.h"
#include "ed2k/Searches.h"
#include "ed2k/Store.h"

#include <algorithm>
#include <random>

namespace aria2::ed2k {

namespace ip = boost::asio::ip;
using namespace std::chrono_literals;

namespace {

constexpr uint8_t kadVersion = 8;
// Requests unanswered after this many seconds count as failed.
constexpr int64_t requestTimeout = 8;
constexpr size_t maxLookups = 5;
constexpr size_t bootstrapContacts = 20;
// Nodes index a file only when the first 32 bits of their ID and the file ID
// differ in no more than the low 24 (eMule's SEARCHTOLERANCE).
constexpr uint32_t searchTolerance = 1u << 24;
constexpr auto sourceLookupInterval = 1h;
constexpr auto sourceLookupRetry = 10min; // while a download has no source
constexpr auto publishInterval = 5h;
constexpr auto firewallCheckInterval = 1h;
constexpr auto firewallAnswerTime = 90s;
constexpr auto saveInterval = 5min;
constexpr size_t maxIndexedSources = 20000;
constexpr size_t maxSourcesPerAnswer = 300;
// Close nodes asked for keyword or source results, and to store a source.
constexpr size_t maxSearchNodes = 20;
constexpr size_t maxPublishNodes = 10;
constexpr auto stallTime = 2s;
constexpr size_t maxPacketSize = 64 << 10;

std::mt19937& rng()
{
  static std::mt19937 engine{std::random_device{}()};
  return engine;
}

std::string randomId()
{
  std::string id(HASH_LENGTH, '\0');
  for (auto& byte : id) {
    byte = static_cast<char>(rng()() & 0xff);
  }
  return id;
}

bool withinTolerance(const std::string& id, const std::string& target)
{
  return (readUInt32(id.data()) ^ readUInt32(target.data())) <= searchTolerance;
}

bool privateAddress(const std::string& host)
{
  boost::system::error_code error;
  const auto address = ip::make_address_v4(host, error);
  if (error) {
    return false;
  }
  const auto bytes = address.to_bytes();
  return bytes[0] == 10 || bytes[0] == 127 ||
         (bytes[0] == 172 && (bytes[1] & 0xf0) == 16) ||
         (bytes[0] == 192 && bytes[1] == 168);
}

std::string_view purposeName(KadTransactionPurpose purpose)
{
  switch (purpose) {
  case KadTransactionPurpose::BOOTSTRAP:
    return "bootstrap";
  case KadTransactionPurpose::SOURCE_LOOKUP:
    return "source lookup";
  case KadTransactionPurpose::KEYWORD_LOOKUP:
    return "keyword lookup";
  case KadTransactionPurpose::REFRESH:
    return "refresh";
  case KadTransactionPurpose::FIREWALLED_CHECK:
    return "firewall check";
  }
  return "request";
}

} // namespace

Kad::Kad(Network& network) : network_(network), table_(randomId())
{
  KadRoutingSnapshot snapshot;
  const auto state = network_.store().meta("kad_state");
  if (!state.empty() && parseKadRoutingStatePayload(snapshot, state)) {
    try {
      table_ = KadRoutingTable(snapshot.selfId);
      table_.restore(snapshot);
      selfId_ = snapshot.selfId;
      udpKey_ = snapshot.udpVerifyKey;
      observed_.insert(snapshot.observedAddresses.begin(),
                       snapshot.observedAddresses.end());
    }
    catch (const std::exception& error) {
      log::warn("ED2K: Kad state discarded: {}", error.what());
      table_ = KadRoutingTable(randomId());
      selfId_.clear();
    }
  }
  if (selfId_.empty()) {
    selfId_ = table_.snapshot().selfId;
  }
  while (udpKey_ == 0) {
    udpKey_ = rng()();
  }
  log::debug("ED2K: Kad node with {} contacts and {} routers",
             table_.liveSize(), table_.getRouterContacts().size());
}

Kad::~Kad() = default;

int64_t Kad::seconds() const
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             Clock::now().time_since_epoch())
      .count();
}

void Kad::addNodes(const std::string& path)
{
  const auto data = readFile(path);
  if (!data) {
    log::warn("ED2K: cannot read the Kad node list {}", path);
    return;
  }
  NodesDat nodes;
  if (!parseNodesDat(nodes, *data)) {
    log::warn("ED2K: {} is not a nodes.dat file", path);
    return;
  }
  const auto before = table_.getRouterContacts().size();
  for (const auto& contact : nodes.contacts) {
    table_.addRouterNode(contact);
  }
  // Contacts restored from the saved routing table are not counted again.
  log::info("ED2K: {} Kad nodes in {}, {} new", nodes.contacts.size(), path,
            table_.getRouterContacts().size() - before);
}

void Kad::save()
{
  auto snapshot = table_.snapshot();
  snapshot.udpVerifyKey = udpKey_;
  snapshot.firewalled = firewalled_;
  snapshot.observedAddresses.assign(observed_.begin(), observed_.end());
  network_.store().setMeta("kad_state", createKadRoutingStatePayload(snapshot));
  lastSave_ = Clock::now();
}

void Kad::describe(boost::json::object& out) const
{
  out["kadNodeCount"] = std::to_string(table_.liveSize());
  out["kadRouterCount"] = std::to_string(table_.getRouterContacts().size());
  out["kadFirewalled"] = firewalled_; // JSON boolean, as clients expect
  out["kadObservedAddressCount"] = std::to_string(observed_.size());
}

std::string_view Kad::lookupName(Purpose purpose)
{
  switch (purpose) {
  case Purpose::Sources:
    return "source";
  case Purpose::Keyword:
    return "keyword";
  case Purpose::Publish:
    return "publish";
  case Purpose::Refresh:
    return "refresh";
  }
  return "";
}

// --- sending ----------------------------------------------------------------

void Kad::send(const KadContact& contact, uint8_t opcode,
               const std::string& payload)
{
  boost::system::error_code error;
  const auto address = ip::make_address_v4(contact.host, error);
  if (error || !contact.udpPort) {
    return;
  }
  auto datagram = createDatagram(KAD_PROTOCOL, opcode, payload);
  // Nodes since Kad 6 take obfuscated packets keyed by their ID; the verify
  // keys let both sides trust the other's address.
  if (contact.version >= 6 && contact.id.size() == HASH_LENGTH) {
    datagram = createKadObfuscatedDatagram(
        datagram, contact.id, static_cast<uint16_t>(rng()()), contact.udpKey,
        createKadUdpVerifyKey(udpKey_, contact.host));
  }
  network_.sendUdp(ip::udp::endpoint(address, contact.udpPort),
                   std::move(datagram));
}

void Kad::reply(const ip::udp::endpoint& to, uint32_t senderKey, uint8_t opcode,
                const std::string& payload)
{
  auto datagram = createDatagram(KAD_PROTOCOL, opcode, payload);
  if (senderKey) {
    const auto host = to.address().to_string();
    datagram = createKadObfuscatedDatagram(datagram, senderKey,
                                           createKadUdpVerifyKey(udpKey_, host),
                                           static_cast<uint16_t>(rng()()));
  }
  network_.sendUdp(to, std::move(datagram));
}

void Kad::request(const KadContact& contact, uint8_t opcode,
                  const std::string& payload, uint8_t answer,
                  KadTransactionPurpose purpose, const std::string& target)
{
  KadTransaction transaction;
  transaction.endpoint.host = contact.host;
  transaction.endpoint.port = contact.udpPort;
  transaction.contact = contact;
  transaction.purpose = purpose;
  transaction.expectedOpcode = answer;
  transaction.targetId = target;
  transaction.sentTime = seconds();
  transactions_.add(transaction);
  send(contact, opcode, payload);
}

// --- receiving --------------------------------------------------------------

bool Kad::obfuscated(const ip::udp::endpoint& from, const std::string& data)
{
  if (!from.address().is_v4()) {
    return false;
  }
  KadObfuscatedDatagram parsed;
  const auto host = from.address().to_string();
  // The marker bits say which key the sender used; old clients set them at
  // random, so both are tried.
  const bool byVerifyKey =
      (static_cast<uint8_t>(data.empty() ? 0 : data[0]) & 0x03) == 2;
  bool ok = byVerifyKey
                ? parseKadObfuscatedDatagram(
                      parsed, data, createKadUdpVerifyKey(udpKey_, host))
                : parseKadObfuscatedDatagram(parsed, data, selfId_);
  if (!ok) {
    return false;
  }
  if (parsed.datagram.size() < 2 ||
      (static_cast<uint8_t>(parsed.datagram[0]) != KAD_PROTOCOL &&
       static_cast<uint8_t>(parsed.datagram[0]) != KAD_PACKED_PROTOCOL)) {
    return false;
  }
  datagram(from, parsed.datagram);
  // Keep the key the sender gave us for answers to it.
  if (parsed.senderVerifyKey) {
    KadContact contact;
    Endpoint endpoint{host, from.port()};
    if (table_.findByEndpoint(contact, endpoint) &&
        contact.udpKey != parsed.senderVerifyKey) {
      contact.udpKey = parsed.senderVerifyKey;
      table_.heardAbout(contact, seconds());
    }
  }
  return true;
}

void Kad::datagram(const ip::udp::endpoint& from, const std::string& data)
{
  if (data.size() < 2 || !from.address().is_v4()) {
    return;
  }
  const auto protocol = static_cast<uint8_t>(data[0]);
  const auto opcode = static_cast<uint8_t>(data[1]);
  std::string payload = data.substr(2);
  if (protocol == KAD_PACKED_PROTOCOL) {
    std::string inflated;
    if (!inflatePackedPacketPayload(inflated, payload, maxPacketSize)) {
      return;
    }
    payload = std::move(inflated);
  }
  else if (protocol != KAD_PROTOCOL) {
    return;
  }
  // Answers carry the sender's key in obfuscated packets only; plain ones
  // are answered in plain.
  KadContact sender = known(from);
  try {
    handle(from, opcode, payload, sender.udpKey);
  }
  catch (const std::exception& error) {
    log::debug("ED2K: bad Kad packet 0x{:02x} from {}: {}", opcode,
               from.address().to_string(), error.what());
  }
}

KadContact Kad::known(const ip::udp::endpoint& from) const
{
  KadContact contact;
  table_.findByEndpoint(contact,
                        Endpoint{from.address().to_string(), from.port()});
  return contact;
}

KadContact Kad::contactFor(const ip::udp::endpoint& from, const std::string& id,
                           uint16_t tcpPort, uint8_t version,
                           uint32_t senderKey) const
{
  KadContact contact;
  contact.id = id;
  contact.host = from.address().to_string();
  contact.udpPort = from.port();
  contact.tcpPort = tcpPort;
  contact.version = version;
  contact.udpKey = senderKey;
  return contact;
}

void Kad::handle(const ip::udp::endpoint& from, uint8_t opcode,
                 const std::string& payload, uint32_t senderKey)
{
  const auto now = seconds();
  const Endpoint endpoint{from.address().to_string(), from.port()};
  KadTransaction transaction;
  switch (opcode) {
  case KAD_BOOTSTRAP_REQ: {
    auto contacts = table_.findClosest(selfId_, bootstrapContacts, false);
    reply(from, senderKey, KAD_BOOTSTRAP_RES,
          createKadBootstrapResponsePayload(selfId_, network_.tcpPort(),
                                            kadVersion, contacts));
    return;
  }
  case KAD_BOOTSTRAP_RES: {
    if (!transactions_.complete(endpoint, KAD_BOOTSTRAP_RES, transaction)) {
      return;
    }
    KadBootstrapResponse response;
    if (!parseKadBootstrapResponsePayload(response, payload)) {
      return;
    }
    table_.nodeSeen(contactFor(from, response.id, response.tcpPort,
                               response.version, senderKey),
                    now);
    for (const auto& contact : response.contacts) {
      table_.heardAbout(contact, now);
    }
    log::debug("ED2K: Kad bootstrap from {} gave {} contacts", endpoint.host,
               response.contacts.size());
    return;
  }
  case KAD_HELLO_REQ: {
    KadHello hello;
    if (!parseKadHelloPayload(hello, payload)) {
      return;
    }
    if (!hello.udpFirewalled) {
      table_.nodeSeen(
          contactFor(from, hello.id, hello.tcpPort, hello.version, senderKey),
          now);
    }
    reply(from, senderKey, KAD_HELLO_RES,
          createKadHelloPayload(selfId_, network_.tcpPort(), kadVersion, false,
                                firewalled_, false));
    return;
  }
  case KAD_HELLO_RES: {
    KadHello hello;
    if (!transactions_.complete(endpoint, KAD_HELLO_RES, transaction) ||
        !parseKadHelloPayload(hello, payload)) {
      return;
    }
    if (!hello.udpFirewalled) {
      table_.nodeSeen(
          contactFor(from, hello.id, hello.tcpPort, hello.version, senderKey),
          now);
    }
    if (hello.requestsAck) {
      reply(from, senderKey, KAD_HELLO_RES_ACK,
            createKadHelloAckPayload(selfId_));
    }
    return;
  }
  case KAD_HELLO_RES_ACK:
    return;
  case KAD_REQ: {
    KadRequest request;
    if (!parseKadRequestPayload(request, payload) ||
        request.receiverId != selfId_) {
      return; // asks a node that had our address before
    }
    const size_t count = std::max<size_t>(request.searchType & 0x1f, 1);
    auto contacts = table_.findClosestExcluding(request.targetId,
                                                known(from).id, count, false);
    if (contacts.empty()) {
      contacts = table_.findClosestExcluding(request.targetId, known(from).id,
                                             count, true);
    }
    reply(from, senderKey, KAD_RES,
          createKadResponsePayload(request.targetId, contacts));
    return;
  }
  case KAD_RES: {
    KadResponse response;
    if (!parseKadResponsePayload(response, payload) ||
        !transactions_.complete(endpoint, KAD_RES, response.targetId,
                                transaction)) {
      return;
    }
    auto contact = transaction.contact;
    if (senderKey) {
      contact.udpKey = senderKey;
    }
    table_.nodeSeen(contact, now);
    std::vector<KadContact> closer;
    for (const auto& item : response.contacts) {
      if (item.id != selfId_) {
        table_.heardAbout(item, now);
        closer.push_back(item);
      }
    }
    if (auto lookup = lookups_.find(response.targetId);
        lookup != lookups_.end()) {
      lookup->second.lastAnswer = Clock::now();
      perform(lookup->second,
              lookup->second.traversal.onResponse(contact, closer));
    }
    return;
  }
  case KAD_PING:
    reply(from, senderKey, KAD_PONG, packUInt16(from.port()));
    return;
  case KAD_PONG:
    return;
  case KAD_SEARCH_SOURCES_REQ: {
    KadSearchSourcesRequest request;
    if (!parseKadSearchSourcesRequestPayload(request, payload)) {
      return;
    }
    const auto entries = sources_.find(
        request.targetId, request.startPosition & 0x7fff, maxSourcesPerAnswer);
    if (!entries.empty()) {
      reply(from, senderKey, KAD_SEARCH_RES,
            createKadSearchResultPayload(selfId_, request.targetId, entries));
    }
    return;
  }
  case KAD_SEARCH_KEYS_REQ:
    return; // keywords are not indexed here
  case KAD_SEARCH_RES: {
    KadSearchResult result;
    if (!parseKadSearchResultPayload(result, payload)) {
      return;
    }
    // Large answers come in several packets; the first ends the request.
    if (transactions_.complete(endpoint, KAD_SEARCH_RES, result.targetId,
                               transaction)) {
      if (auto lookup = lookups_.find(result.targetId);
          lookup != lookups_.end()) {
        lookup->second.traversal.onSearchResponse(transaction.contact);
      }
    }
    if (auto lookup = lookups_.find(result.targetId);
        lookup != lookups_.end()) {
      lookup->second.lastAnswer = Clock::now();
      lookup->second.results += result.entries.size();
    }
    searchResult(known(from), result);
    return;
  }
  case KAD_PUBLISH_SOURCE_REQ: {
    KadPublishSourceRequest request;
    if (!parseKadPublishSourceRequestPayload(request, payload)) {
      return;
    }
    if (!withinTolerance(selfId_, request.fileId) &&
        !privateAddress(endpoint.host)) {
      return;
    }
    // The source's address is the one the publish came from.
    auto entry = request.source;
    entry.tags.erase(
        std::remove_if(entry.tags.begin(), entry.tags.end(),
                       [](const Tag& tag) { return tag.id == 0xfe; }),
        entry.tags.end());
    Tag address;
    address.id = 0xfe;
    address.valueType = TagValueType::UINT;
    address.intValue = from.address().to_v4().to_uint();
    entry.tags.insert(entry.tags.begin(), address);
    if (sources_.size() < maxIndexedSources) {
      sources_.store(request.fileId, entry);
    }
    reply(from, senderKey, KAD_PUBLISH_RES,
          createKadPublishResultPayload(
              request.fileId,
              static_cast<uint8_t>(std::min<size_t>(
                  sources_.find(request.fileId, 0, 0).size(), 100))));
    return;
  }
  case KAD_PUBLISH_RES: {
    KadPublishResult result;
    if (!parseKadPublishResultPayload(result, payload) ||
        !transactions_.complete(endpoint, KAD_PUBLISH_RES, result.fileId,
                                transaction)) {
      return;
    }
    if (auto lookup = lookups_.find(result.fileId); lookup != lookups_.end()) {
      lookup->second.traversal.onSearchResponse(transaction.contact);
    }
    return;
  }
  case KAD_FIREWALLED_REQ: {
    // Another node tests whether its TCP port is reachable from outside.
    KadFirewalledRequest request;
    if (!parseKadFirewalledRequestPayload(request, payload) ||
        !request.tcpPort) {
      return;
    }
    network_.client(from.address().to_v4(), request.tcpPort)->checkFirewall();
    reply(from, senderKey, KAD_FIREWALLED_RES,
          createKadFirewalledResponsePayload(endpoint.host));
    return;
  }
  case KAD_FIREWALLED_RES: {
    KadFirewalledResponse response;
    if (!firewallCheckers_.contains(from.address().to_v4()) ||
        !parseKadFirewalledResponsePayload(response, payload)) {
      return;
    }
    boost::system::error_code error;
    const auto address = ip::make_address_v4(response.ipAddress, error);
    if (!error && !address.is_unspecified()) {
      observed_.insert(response.ipAddress);
      network_.observedAddress(address);
    }
    return;
  }
  case KAD_FIREWALLED_ACK_RES:
    tcpReachable(from.address().to_v4());
    return;
  default:
    return;
  }
}

void Kad::searchResult(const KadContact&, const KadSearchResult& result)
{
  auto lookup = lookups_.find(result.targetId);
  if (lookup == lookups_.end()) {
    return;
  }
  auto& search = lookup->second;
  if (search.purpose == Purpose::Sources) {
    auto* file = network_.file(search.fileHash);
    if (!file || !file->downloading()) {
      return;
    }
    size_t added = 0;
    for (const auto& source : extractKadSourceEndpointDetails(result)) {
      // Types 1 and 4 are reachable sources; the others need a buddy or a
      // direct UDP callback, which this client does not do.
      if (source.sourceType != 1 && source.sourceType != 4) {
        continue;
      }
      file->addSource(source.endpoint, 0, {}, Origin::Kad);
      ++added;
    }
    log::debug("ED2K: Kad found {} sources of {}", added, file->name());
  }
  else if (search.purpose == Purpose::Keyword) {
    auto entries = kadSearchEntriesToSearchResults(result.entries, "kad");
    for (auto& entry : entries) {
      entry.hash = kadIdToEd2kHash(entry.hash);
    }
    network_.searches().kadResults(search.search, entries);
  }
}

void Kad::tcpReachable(const ip::address_v4& from)
{
  if (!firewallCheckers_.contains(from)) {
    return;
  }
  if (firewalled_) {
    log::info("ED2K: Kad reports our TCP port open");
  }
  firewalled_ = false;
  reachedAt_ = Clock::now();
}

// --- periodic work ----------------------------------------------------------

void Kad::tick(Clock::time_point now)
{
  expire(now);
  bootstrap(now);
  if (table_.liveSize() == 0) {
    return;
  }
  checkFirewall(now);
  startLookups(now);
  if (now - lastSave_ > saveInterval) {
    save();
  }
}

void Kad::bootstrap(Clock::time_point)
{
  if (table_.liveSize() >= 10) {
    return;
  }
  // A few routers a second until the table holds enough contacts.
  const auto routers = table_.getRouterContacts();
  if (routers.empty()) {
    return;
  }
  for (int i = 0; i < 3; ++i) {
    const auto& router = routers[nextRouter_++ % routers.size()];
    request(router, KAD_BOOTSTRAP_REQ, {}, KAD_BOOTSTRAP_RES,
            KadTransactionPurpose::BOOTSTRAP, {});
  }
  // Contacts heard of but never answered are greeted.
  for (const auto& contact : table_.findClosest(selfId_, 3, true)) {
    request(contact, KAD_HELLO_REQ,
            createKadHelloPayload(selfId_, network_.tcpPort(), kadVersion,
                                  false, firewalled_, false),
            KAD_HELLO_RES, KadTransactionPurpose::BOOTSTRAP, {});
  }
}

void Kad::checkFirewall(Clock::time_point now)
{
  if (firewallCheck_ != Clock::time_point{} &&
      now - firewallCheck_ < firewallCheckInterval) {
    if (!firewalled_ && reachedAt_ < firewallCheck_ &&
        now - firewallCheck_ > firewallAnswerTime) {
      log::info("ED2K: Kad reports our TCP port closed");
      firewalled_ = true;
    }
    return;
  }
  if (table_.liveSize() < 4) {
    return;
  }
  firewallCheck_ = now;
  firewallCheckers_.clear();
  auto contacts = table_.findClosest(randomId(), 4, false);
  for (const auto& contact : contacts) {
    boost::system::error_code error;
    const auto address = ip::make_address_v4(contact.host, error);
    if (error) {
      continue;
    }
    firewallCheckers_.insert(address);
    request(contact, KAD_FIREWALLED_REQ,
            createKadFirewalledRequestPayload(
                network_.tcpPort(), network_.userHash(), SOURCE_CRYPT_SUPPORT),
            KAD_FIREWALLED_RES, KadTransactionPurpose::FIREWALLED_CHECK, {});
  }
}

void Kad::startLookups(Clock::time_point now)
{
  std::string refresh;
  if (lookups_.size() < maxLookups && table_.needRefresh(refresh, seconds())) {
    startLookup(Purpose::Refresh, refresh, 0, {}, 0);
  }
  for (const auto& [hash, file] : network_.files()) {
    if (lookups_.size() >= maxLookups) {
      break;
    }
    const auto id = ed2kHashToKadId(hash);
    if (lookups_.contains(id)) {
      continue;
    }
    if (file->downloading()) {
      auto& last = lastSourceLookup_[hash];
      const auto interval =
          file->sources().empty() ? sourceLookupRetry : sourceLookupInterval;
      if (last == Clock::time_point{} || now - last > interval) {
        last = now;
        startLookup(Purpose::Sources, id, static_cast<uint64_t>(file->size()),
                    hash, 0);
      }
    }
    else if (file->sharesAnything() && !firewalled_) {
      auto& last = lastPublish_[hash];
      if (last == Clock::time_point{} || now - last > publishInterval) {
        last = now;
        startLookup(Purpose::Publish, id, static_cast<uint64_t>(file->size()),
                    hash, 0);
      }
    }
  }
  // Downloads with verified parts are sources too.
  if (!firewalled_) {
    for (const auto& [hash, file] : network_.files()) {
      if (lookups_.size() >= maxLookups) {
        break;
      }
      if (!file->downloading() || !file->sharesAnything()) {
        continue;
      }
      const auto id = ed2kHashToKadId(hash);
      auto& last = lastPublish_[hash];
      if (!lookups_.contains(id) &&
          (last == Clock::time_point{} || now - last > publishInterval)) {
        last = now;
        startLookup(Purpose::Publish, id, static_cast<uint64_t>(file->size()),
                    hash, 0);
      }
    }
  }
}

bool Kad::search(Gid id, const std::string& keyword)
{
  const auto word = pickKadKeyword(keyword);
  if (word.empty()) {
    return true; // nothing Kad can look up
  }
  if (table_.liveSize() == 0) {
    return false;
  }
  const auto target = ed2kHashToKadId(createKadKeywordTarget(word));
  lookups_.erase(target); // a newer search for the same keyword takes over
  startLookup(Purpose::Keyword, target, 0, {}, id);
  return true;
}

void Kad::stopSearch(Gid id)
{
  std::erase_if(lookups_, [id](const auto& item) {
    return item.second.purpose == Purpose::Keyword && item.second.search == id;
  });
}

void Kad::startLookup(Purpose purpose, const std::string& target, uint64_t size,
                      const std::string& fileHash, Gid search)
{
  if (lookups_.contains(target)) {
    return;
  }
  auto seeds = table_.findClosest(target, 50, true);
  if (seeds.empty()) {
    seeds = table_.getRouterContacts();
  }
  if (seeds.empty()) {
    return;
  }
  // Only nodes close enough to the target index it.
  KadTraversal::Searchable searchable;
  if (purpose != Purpose::Refresh) {
    searchable = [target](const KadContact& contact) {
      return withinTolerance(contact.id, target) ||
             privateAddress(contact.host);
    };
  }
  const size_t searches =
      purpose == Purpose::Publish ? maxPublishNodes : maxSearchNodes;
  Lookup lookup{purpose,
                KadTraversal(purpose == Purpose::Keyword
                                 ? KadTraversalKind::KEYWORD_LOOKUP
                                 : KadTraversalKind::SOURCE_LOOKUP,
                             target, size, std::move(searchable), searches),
                fileHash,
                size,
                search,
                Clock::now(),
                Clock::now()};
  auto& stored = lookups_.emplace(target, std::move(lookup)).first->second;
  perform(stored, stored.traversal.start(seeds));
}

void Kad::perform(Lookup& lookup,
                  const std::vector<KadTraversalAction>& actions)
{
  const auto& target = lookup.traversal.targetId();
  for (const auto& action : actions) {
    const auto& contact = action.contact;
    if (action.type == KadTraversalActionType::FIND_NODE) {
      const uint8_t type = lookup.purpose == Purpose::Publish ? KAD_STORE
                           : lookup.purpose == Purpose::Refresh
                               ? KAD_FIND_NODE
                               : KAD_FIND_VALUE;
      request(contact, KAD_REQ,
              createKadRequestPayload(type, target, contact.id), KAD_RES,
              lookup.purpose == Purpose::Keyword
                  ? KadTransactionPurpose::KEYWORD_LOOKUP
              : lookup.purpose == Purpose::Refresh
                  ? KadTransactionPurpose::REFRESH
                  : KadTransactionPurpose::SOURCE_LOOKUP,
              target);
      continue;
    }
    switch (lookup.purpose) {
    case Purpose::Sources:
      request(contact, KAD_SEARCH_SOURCES_REQ,
              createKadSearchSourcesRequestPayload(target, 0, lookup.size),
              KAD_SEARCH_RES, KadTransactionPurpose::SOURCE_LOOKUP, target);
      break;
    case Purpose::Keyword:
      request(contact, KAD_SEARCH_KEYS_REQ,
              createKadSearchKeysRequestPayload(target, 0), KAD_SEARCH_RES,
              KadTransactionPurpose::KEYWORD_LOOKUP, target);
      break;
    case Purpose::Publish: {
      Endpoint self;
      self.port = network_.tcpPort();
      request(contact, KAD_PUBLISH_SOURCE_REQ,
              createKadPublishSourceRequestPayload(
                  target, self, ed2kHashToKadId(network_.userHash()),
                  lookup.size, network_.udpPort(), SOURCE_CRYPT_SUPPORT),
              KAD_PUBLISH_RES, KadTransactionPurpose::SOURCE_LOOKUP, target);
      break;
    }
    case Purpose::Refresh:
      break;
    }
  }
}

void Kad::expire(Clock::time_point now)
{
  for (const auto& transaction :
       transactions_.expire(seconds(), requestTimeout)) {
    if (transaction.expectedOpcode == KAD_RES ||
        transaction.expectedOpcode == KAD_HELLO_RES ||
        transaction.expectedOpcode == KAD_BOOTSTRAP_RES) {
      table_.nodeFailed(transaction.contact);
    }
    if (transaction.targetId.empty()) {
      continue;
    }
    auto lookup = lookups_.find(transaction.targetId);
    if (lookup == lookups_.end()) {
      continue;
    }
    if (transaction.expectedOpcode == KAD_RES) {
      perform(lookup->second,
              lookup->second.traversal.onFailure(transaction.contact));
    }
    else {
      lookup->second.traversal.onSearchFailure(transaction.contact);
    }
    log::debug("ED2K: Kad {} to {} timed out", purposeName(transaction.purpose),
               transaction.contact.host);
  }
  std::erase_if(lookups_, [now](const auto& item) {
    const auto& lookup = item.second;
    const auto lifetime = lookup.purpose == Purpose::Publish ? 140s : 45s;
    if (!lookup.traversal.done() && now - lookup.started <= lifetime) {
      return false;
    }
    const auto& traversal = lookup.traversal;
    log::debug("ED2K: Kad {} lookup {} {} after {} ms: {} asked, {} answered, "
               "{} searched, {} results",
               lookupName(lookup.purpose), text::toHex(item.first).substr(0, 8),
               traversal.done() ? "finished" : "expired",
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   now - lookup.started)
                   .count(),
               traversal.queried(), traversal.answered(), traversal.searched(),
               lookup.results);
    return true;
  });
  // A lookup that heard nothing for a while asks one more contact, so dead
  // contacts do not hold it up for the full request timeout.
  for (auto& [target, lookup] : lookups_) {
    if (now - lookup.lastAnswer > stallTime) {
      lookup.lastAnswer = now;
      perform(lookup, lookup.traversal.stalled());
    }
  }
}

} // namespace aria2::ed2k
