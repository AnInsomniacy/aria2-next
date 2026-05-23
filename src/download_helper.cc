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
#include "download_helper.h"

#include <algorithm>
#include <sstream>

#include "RequestGroup.h"
#include "Option.h"
#include "prefs.h"
#include "Metalink2RequestGroup.h"
#include "ProtocolDetector.h"
#include "paramed_string.h"
#include "UriListParser.h"
#include "DownloadContext.h"
#include "Ed2kAttribute.h"
#include "RecoverableException.h"
#include "DlAbortEx.h"
#include "message.h"
#include "fmt.h"
#include "FileEntry.h"
#include "LogFactory.h"
#include "File.h"
#include "util.h"
#include "array_fun.h"
#include "OptionHandler.h"
#include "ByteArrayDiskWriter.h"
#include "a2functional.h"
#include "ByteArrayDiskWriterFactory.h"
#include "BufferedFile.h"
#include "MetadataInfo.h"
#include "OptionParser.h"
#include "SegList.h"
#include "LibtorrentAttribute.h"
#include "bencode2.h"
#include "download_handlers.h"
#include "ed2k_hash.h"
#include "ed2k_kad.h"
#include "ed2k_link.h"
#include "ed2k_policy.h"
#include "ed2k_search.h"
#include "ed2k_server.h"
#include "SimpleRandomizer.h"
#ifdef ENABLE_BITTORRENT
#  include "bittorrent_helper.h"
#  include "BtConstants.h"
#  include "ValueBaseBencodeParser.h"
#  include <libtorrent/torrent_info.hpp>
#endif // ENABLE_BITTORRENT

namespace aria2 {

namespace {
constexpr char DEFAULT_ED2K_SERVER_LIST[] =
    "45.82.80.155:5687,176.123.5.89:4725,85.121.5.137:4232,"
    "176.123.2.239:4232,145.239.2.134:4661,91.208.162.87:4232,"
    "37.15.61.236:4232";
constexpr char DEFAULT_ED2K_NODE_LIST_PATH[] = "${HOME}/.aMule/nodes.dat";
constexpr char DEFAULT_ED2K_MAC_NODE_LIST_PATH[] =
    "${HOME}/Library/Application Support/aMule/nodes.dat";

void unfoldURI(std::vector<std::string>& result,
               const std::vector<std::string>& args)
{
  for (const auto& i : args) {
    paramed_string::expand(std::begin(i), std::end(i),
                           std::back_inserter(result));
  }
}
} // namespace

namespace {
template <typename InputIterator>
void splitURI(std::vector<std::string>& result, InputIterator begin,
              InputIterator end, size_t numSplit, size_t maxIter)
{
  size_t numURIs = std::distance(begin, end);
  if (numURIs >= numSplit) {
    result.insert(std::end(result), begin, end);
  }
  else if (numURIs > 0) {
    size_t num = std::min(numSplit / numURIs, maxIter);
    for (size_t i = 0; i < num; ++i) {
      result.insert(std::end(result), begin, end);
    }
    if (num < maxIter) {
      result.insert(std::end(result), begin, begin + (numSplit % numURIs));
    }
  }
}
} // namespace

namespace {
std::shared_ptr<GroupId> getGID(const std::shared_ptr<Option>& option)
{
  std::shared_ptr<GroupId> gid;
  if (option->defined(PREF_GID)) {
    a2_gid_t n;
    if (GroupId::toNumericId(n, option->get(PREF_GID).c_str()) != 0) {
      throw DL_ABORT_EX(
          fmt("%s is invalid for GID.", option->get(PREF_GID).c_str()));
    }
    gid = GroupId::import(n);
    if (!gid) {
      throw DL_ABORT_EX(
          fmt("GID %s is not unique.", option->get(PREF_GID).c_str()));
    }
  }
  else {
    gid = GroupId::create();
  }
  return gid;
}
} // namespace

namespace {
void addUniqueEndpoint(std::vector<ed2k::Endpoint>& endpoints,
                       const ed2k::Endpoint& endpoint)
{
  if (endpoint.host.empty() || endpoint.port == 0) {
    return;
  }
  auto i = std::find_if(endpoints.begin(), endpoints.end(),
                        [&](const ed2k::Endpoint& item) {
                          return item.host == endpoint.host &&
                                 item.port == endpoint.port;
                        });
  if (i == endpoints.end()) {
    endpoints.push_back(endpoint);
  }
}

void addEndpointList(std::vector<ed2k::Endpoint>& endpoints,
                     const std::string& value)
{
  std::vector<Scip> entries;
  util::splitIter(value.begin(), value.end(), std::back_inserter(entries),
                  ',');
  for (const auto& entry : entries) {
    addUniqueEndpoint(
        endpoints,
        ed2k::parseEndpoint(std::string(entry.first, entry.second)));
  }
}

std::string readLocalFile(const std::string& path)
{
  BufferedFile fp(path.c_str(), BufferedFile::READ);
  if (!fp) {
    throw DL_ABORT_EX(fmt("Cannot open ED2K metadata file: %s", path.c_str()));
  }
  std::ostringstream out;
  fp.transfer(out);
  return out.str();
}

void mergeServerMetFile(std::vector<ed2k::Endpoint>& endpoints,
                        std::vector<ed2k::ServerState>& states,
                        const std::string& path)
{
  for (const auto& entry : ed2k::parseServerMetEntries(readLocalFile(path))) {
    addUniqueEndpoint(endpoints, entry.endpoint);
    auto i = std::find_if(states.begin(), states.end(),
                          [&](const ed2k::ServerState& state) {
                            return state.endpoint.host == entry.endpoint.host &&
                                   state.endpoint.port == entry.endpoint.port;
                          });
    if (i == states.end()) {
      ed2k::ServerState state;
      state.endpoint = entry.endpoint;
      state.name = entry.name;
      state.description = entry.description;
      state.users = entry.users;
      state.files = entry.files;
      state.maxUsers = entry.maxUsers;
      state.softFiles = entry.softFiles;
      state.hardFiles = entry.hardFiles;
      state.udpFlags = entry.udpFlags;
      state.lowIdUsers = entry.lowIdUsers;
      state.udpObfuscationPort = entry.udpObfuscationPort;
      state.tcpObfuscationPort = entry.tcpObfuscationPort;
      state.udpKey = entry.udpKey;
      states.push_back(state);
      continue;
    }
    if (!entry.name.empty()) {
      i->name = entry.name;
    }
    if (!entry.description.empty()) {
      i->description = entry.description;
    }
    if (entry.users) {
      i->users = entry.users;
    }
    if (entry.files) {
      i->files = entry.files;
    }
    if (entry.maxUsers) {
      i->maxUsers = entry.maxUsers;
    }
    if (entry.softFiles) {
      i->softFiles = entry.softFiles;
    }
    if (entry.hardFiles) {
      i->hardFiles = entry.hardFiles;
    }
    if (entry.udpFlags) {
      i->udpFlags = entry.udpFlags;
    }
    if (entry.lowIdUsers) {
      i->lowIdUsers = entry.lowIdUsers;
    }
    if (entry.udpObfuscationPort) {
      i->udpObfuscationPort = entry.udpObfuscationPort;
    }
    if (entry.tcpObfuscationPort) {
      i->tcpObfuscationPort = entry.tcpObfuscationPort;
    }
    if (entry.udpKey) {
      i->udpKey = entry.udpKey;
    }
  }
}

void addOptionEd2kServers(std::vector<ed2k::Endpoint>& endpoints,
                          std::vector<ed2k::ServerState>& states,
                          const std::shared_ptr<Option>& option)
{
  if (!option->blank(PREF_ED2K_SERVER)) {
    addEndpointList(endpoints, option->get(PREF_ED2K_SERVER));
  }
  if (!option->blank(PREF_ED2K_SERVER_LIST)) {
    mergeServerMetFile(endpoints, states, option->get(PREF_ED2K_SERVER_LIST));
  }
}

void addDefaultEd2kServersIfNeeded(std::vector<ed2k::Endpoint>& endpoints,
                                   const ed2k::Link& link,
                                   const std::shared_ptr<Option>& option)
{
  if (!endpoints.empty() || !link.sources.empty()) {
    return;
  }
  addEndpointList(endpoints, DEFAULT_ED2K_SERVER_LIST);
}

void addEd2kServerStateEndpoints(std::vector<ed2k::Endpoint>& endpoints,
                                 const std::vector<ed2k::ServerState>& states)
{
  for (const auto& state : states) {
    addUniqueEndpoint(endpoints, state.endpoint);
  }
}

std::vector<ed2k::ServerState>
createEd2kServerStates(const std::shared_ptr<Option>& option)
{
  std::vector<ed2k::ServerState> states;
  if (option->blank(PREF_ED2K_SERVER_STATE)) {
    return states;
  }
  std::istringstream in(option->get(PREF_ED2K_SERVER_STATE));
  for (std::string payload; std::getline(in, payload);) {
    if (payload.empty()) {
      continue;
    }
    ed2k::ServerState state;
    if (payload.size() % 2 != 0 || !util::isHexDigit(payload) ||
        !ed2k::parseServerStatePayload(
            state, util::fromHex(payload.begin(), payload.end()))) {
      throw DL_ABORT_EX("Cannot parse ED2K server state.");
    }
    states.push_back(state);
  }
  return states;
}

std::string defaultEd2kNodeListPath()
{
  return util::replace(DEFAULT_ED2K_NODE_LIST_PATH, "${HOME}",
                       util::getHomeDir());
}

std::string defaultEd2kMacNodeListPath()
{
  return util::replace(DEFAULT_ED2K_MAC_NODE_LIST_PATH, "${HOME}",
                       util::getHomeDir());
}

std::shared_ptr<ed2k::KadRoutingTable>
createEd2kKadRoutingTableFromNodesDat(const std::string& path,
                                      const std::string& selfId,
                                      bool required)
{
  if (!required && !File(path).isFile()) {
    return nullptr;
  }
  ed2k::NodesDat nodes;
  if (!ed2k::parseNodesDat(nodes, readLocalFile(path))) {
    if (required) {
      throw DL_ABORT_EX(fmt("Cannot parse ED2K node list: %s", path.c_str()));
    }
    return nullptr;
  }
  auto table = std::make_shared<ed2k::KadRoutingTable>(
      ed2k::ed2kHashToKadId(selfId));
  for (const auto& contact : nodes.contacts) {
    table->addRouterNode(contact);
  }
  return table;
}

std::shared_ptr<ed2k::KadRoutingTable> createEd2kKadRoutingTable(
    const std::shared_ptr<Option>& option, const std::string& selfId,
    ed2k::KadRoutingSnapshot* restoredSnapshot = nullptr,
    bool createEmpty = false)
{
  if (!option->blank(PREF_ED2K_KAD_ROUTING_STATE)) {
    ed2k::KadRoutingSnapshot snapshot;
    const auto state = option->get(PREF_ED2K_KAD_ROUTING_STATE);
    if (state.size() % 2 != 0 || !util::isHexDigit(state) ||
        !ed2k::parseKadRoutingStatePayload(
            snapshot, util::fromHex(state.begin(), state.end()))) {
      throw DL_ABORT_EX("Cannot parse ED2K Kad routing state.");
    }
    auto table = std::make_shared<ed2k::KadRoutingTable>(
        ed2k::ed2kHashToKadId(selfId));
    table->restore(snapshot);
    if (restoredSnapshot) {
      *restoredSnapshot = snapshot;
    }
    return table;
  }
  if (!option->blank(PREF_ED2K_NODE_LIST)) {
    return createEd2kKadRoutingTableFromNodesDat(
        option->get(PREF_ED2K_NODE_LIST), selfId, true);
  }
  auto table =
      createEd2kKadRoutingTableFromNodesDat(defaultEd2kNodeListPath(), selfId,
                                            false);
  if (table) {
    return table;
  }
  table = createEd2kKadRoutingTableFromNodesDat(defaultEd2kMacNodeListPath(),
                                                selfId, false);
  if (table) {
    return table;
  }
  if (createEmpty) {
    return std::make_shared<ed2k::KadRoutingTable>(
        ed2k::ed2kHashToKadId(selfId));
  }
  return nullptr;
}
} // namespace

namespace {
std::shared_ptr<RequestGroup>
createEd2kRequestGroup(const std::string& ed2kUri,
                       const std::shared_ptr<Option>& optionTemplate)
{
  auto link = ed2k::parseLink(ed2kUri);
  if (link.type != ed2k::LinkType::FILE) {
    throw DL_ABORT_EX("Only ED2K file links can create downloads.");
  }

  auto option = util::copy(optionTemplate);
  auto gid = getGID(option);
  auto rg = std::make_shared<RequestGroup>(gid, option);
  const auto outputName =
      option->blank(PREF_OUT) ? util::fixTaintedBasename(link.name)
                              : option->get(PREF_OUT);
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, link.size,
      util::applyDir(option->get(PREF_DIR), outputName));
  dctx->getFirstFileEntry()->setMaxConnectionPerServer(
      option->getAsInt(PREF_MAX_CONNECTION_PER_SERVER));
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->link = std::move(link);
  attrs->clientHash = getOrCreateEd2kClientHash(option.get());
  attrs->serverStates = createEd2kServerStates(option);
  addOptionEd2kServers(attrs->servers, attrs->serverStates, option);
  addEd2kServerStateEndpoints(attrs->servers, attrs->serverStates);
  addDefaultEd2kServersIfNeeded(attrs->servers, attrs->link, option);
  ed2k::KadRoutingSnapshot kadSnapshot;
  attrs->kadRoutingTable =
      createEd2kKadRoutingTable(option, attrs->clientHash, &kadSnapshot, true);
  if (attrs->kadRoutingTable) {
    restoreEd2kKadOperationalState(attrs.get(), kadSnapshot);
  }
  dctx->setAttribute(CTX_ATTR_ED2K, std::move(attrs));
  dctx->setAcceptMetalink(false);
  rg->setDownloadContext(dctx);
  const auto peerConnections =
      option->definedLocal(PREF_SPLIT)
          ? option->getAsInt(PREF_SPLIT)
          : ed2k::DEFAULT_PEER_CONNECTIONS;
  rg->setNumConcurrentCommand(peerConnections);

  if (option->getAsBool(PREF_ENABLE_RPC)) {
    rg->setPauseRequested(option->getAsBool(PREF_PAUSE));
  }

  removeOneshotOption(option);
  return rg;
}
} // namespace

namespace {
std::shared_ptr<RequestGroup>
createRequestGroup(const std::shared_ptr<Option>& optionTemplate,
                   const std::vector<std::string>& uris,
                   bool useOutOption = false)
{
  auto option = util::copy(optionTemplate);
  auto rg = std::make_shared<RequestGroup>(getGID(option), option);
  auto dctx = std::make_shared<DownloadContext>(
      option->getAsInt(PREF_PIECE_LENGTH), 0,
      useOutOption && !option->blank(PREF_OUT)
          ? util::applyDir(option->get(PREF_DIR), option->get(PREF_OUT))
          : A2STR::NIL);
  dctx->getFirstFileEntry()->setUris(uris);
  dctx->getFirstFileEntry()->setMaxConnectionPerServer(
      option->getAsInt(PREF_MAX_CONNECTION_PER_SERVER));
  const std::string& checksum = option->get(PREF_CHECKSUM);
  if (!checksum.empty()) {
    auto p = util::divide(std::begin(checksum), std::end(checksum), '=');
    std::string hashType(p.first.first, p.first.second);
    std::string hexDigest(p.second.first, p.second.second);
    util::lowercase(hashType);
    dctx->setDigest(hashType,
                    util::fromHex(std::begin(hexDigest), std::end(hexDigest)));
  }
  rg->setDownloadContext(dctx);

  if (option->getAsBool(PREF_ENABLE_RPC)) {
    rg->setPauseRequested(option->getAsBool(PREF_PAUSE));
  }

  removeOneshotOption(option);
  return rg;
}
} // namespace

std::shared_ptr<RequestGroup>
createEd2kSearchRequestGroup(const ed2k::SearchQuery& query,
                             const std::shared_ptr<Option>& optionTemplate)
{
  auto option = util::copy(optionTemplate);
  auto gid = getGID(option);
  auto rg = std::make_shared<RequestGroup>(gid, option);
  auto dctx = std::make_shared<DownloadContext>(
      ed2k::PIECE_LENGTH, 0,
      util::applyDir(option->get(PREF_DIR),
                     "aria2-next-ed2k-search-" + gid->toHex()));
  auto attrs = std::make_shared<Ed2kAttribute>();
  attrs->searchActive = true;
  attrs->searchQuery = query;
  attrs->link.type = ed2k::LinkType::FILE;
  attrs->link.name = query.keyword;
  attrs->link.size = 0;
  attrs->clientHash = getOrCreateEd2kClientHash(option.get());
  attrs->serverStates = createEd2kServerStates(option);
  addOptionEd2kServers(attrs->servers, attrs->serverStates, option);
  addEd2kServerStateEndpoints(attrs->servers, attrs->serverStates);
  addDefaultEd2kServersIfNeeded(attrs->servers, attrs->link, option);
  if (!option->blank(PREF_ED2K_NODE_LIST) ||
      !option->blank(PREF_ED2K_KAD_ROUTING_STATE)) {
    ed2k::KadRoutingSnapshot kadSnapshot;
    attrs->kadRoutingTable = createEd2kKadRoutingTable(
        option, attrs->clientHash, &kadSnapshot);
    if (attrs->kadRoutingTable) {
      restoreEd2kKadOperationalState(attrs.get(), kadSnapshot);
    }
  }
  if (attrs->servers.empty()) {
    if (!attrs->kadRoutingTable ||
        attrs->kadRoutingTable->getRouterNodes().empty()) {
      throw DL_ABORT_EX("ED2K search requires at least one server or Kad node.");
    }
  }
  dctx->setAttribute(CTX_ATTR_ED2K, std::move(attrs));
  dctx->setAcceptMetalink(false);
  rg->setDownloadContext(dctx);
  rg->setNumConcurrentCommand(option->getAsInt(PREF_SPLIT));
  if (option->getAsBool(PREF_ENABLE_RPC)) {
    rg->setPauseRequested(option->getAsBool(PREF_PAUSE));
  }
  removeOneshotOption(option);
  return rg;
}

#if defined(ENABLE_BITTORRENT) || defined(ENABLE_METALINK)
namespace {
std::shared_ptr<MetadataInfo>
createMetadataInfo(const std::shared_ptr<GroupId>& gid, const std::string& uri)
{
  return std::make_shared<MetadataInfo>(gid, uri);
}
} // namespace

namespace {
std::shared_ptr<MetadataInfo> createMetadataInfoDataOnly()
{
  return std::make_shared<MetadataInfo>();
}
} // namespace
#endif // ENABLE_BITTORRENT || ENABLE_METALINK

#ifdef ENABLE_BITTORRENT

namespace {
std::shared_ptr<RequestGroup>
createLibtorrentRequestGroup(LibtorrentAttribute::SourceType sourceType,
                             const std::string& sourceUri,
                             const std::string& torrentData,
                             const std::vector<std::string>& webSeedUris,
                             const std::shared_ptr<Option>& optionTemplate)
{
  auto option = util::copy(optionTemplate);
  auto gid = getGID(option);
  auto rg = std::make_shared<RequestGroup>(gid, option);
  auto dctx = std::make_shared<DownloadContext>();
  dctx->markTotalLengthIsUnknown();
  auto displayName =
      sourceType == LibtorrentAttribute::SourceType::MAGNET
          ? std::string("magnet")
          : File(sourceUri).getBasename();
  if (displayName.empty()) {
    displayName = "torrent";
  }
  std::vector<std::shared_ptr<FileEntry>> entries;
  entries.push_back(
      std::make_shared<FileEntry>(util::applyDir(option->get(PREF_DIR),
                                                util::escapePath(displayName)),
                                  0, 0));
  dctx->setFileEntries(entries.begin(), entries.end());
  dctx->setAttribute(
      CTX_ATTR_LIBTORRENT,
      make_unique<LibtorrentAttribute>(sourceType, sourceUri, torrentData,
                                       webSeedUris));
  dctx->setAcceptMetalink(false);
  rg->setDownloadContext(dctx);
  rg->setFileAllocationEnabled(false);
  rg->setPreLocalFileCheckEnabled(false);
  rg->setMetadataInfo(createMetadataInfo(gid, sourceUri));
  rg->setNumConcurrentCommand(option->getAsInt(PREF_SPLIT));
  if (option->getAsBool(PREF_ENABLE_RPC)) {
    rg->setPauseRequested(option->getAsBool(PREF_PAUSE));
  }
  removeOneshotOption(option);
  return rg;
}
} // namespace

namespace {
std::shared_ptr<RequestGroup>
createBtRequestGroup(const std::string& metaInfoUri,
                     const std::shared_ptr<Option>& optionTemplate,
                     const std::vector<std::string>& auxUris,
                     const ValueBase* torrent, bool adjustAnnounceUri = true)
{
  if (!torrent) {
    throw DL_ABORT_EX2("Bencode decoding failed",
                       error_code::BENCODE_PARSE_ERROR);
  }
  std::string torrentData = bencode2::encode(torrent);
  return createLibtorrentRequestGroup(
      metaInfoUri.empty() ? LibtorrentAttribute::SourceType::TORRENT_DATA
                          : LibtorrentAttribute::SourceType::TORRENT_FILE,
      metaInfoUri, torrentData, auxUris, optionTemplate);
}
} // namespace

namespace {
std::shared_ptr<RequestGroup>
createBtMagnetRequestGroup(const std::string& magnetLink,
                           const std::shared_ptr<Option>& optionTemplate)
{
  return createLibtorrentRequestGroup(LibtorrentAttribute::SourceType::MAGNET,
                                      magnetLink, "", {}, optionTemplate);
}
} // namespace

void createRequestGroupForBitTorrent(
    std::vector<std::shared_ptr<RequestGroup>>& result,
    const std::shared_ptr<Option>& option, const std::vector<std::string>& uris,
    const std::string& metaInfoUri, const std::string& torrentData,
    bool adjustAnnounceUri)
{
  std::unique_ptr<ValueBase> torrent;
  bittorrent::ValueBaseBencodeParser parser;
  if (torrentData.empty()) {
    torrent = parseFile(parser, metaInfoUri);
  }
  else {
    ssize_t error;
    torrent = parser.parseFinal(torrentData.c_str(), torrentData.size(), error);
  }
  if (!torrent) {
    throw DL_ABORT_EX2("Bencode decoding failed",
                       error_code::BENCODE_PARSE_ERROR);
  }
  createRequestGroupForBitTorrent(result, option, uris, metaInfoUri,
                                  torrent.get(), adjustAnnounceUri);
}

void createRequestGroupForBitTorrent(
    std::vector<std::shared_ptr<RequestGroup>>& result,
    const std::shared_ptr<Option>& option, const std::vector<std::string>& uris,
    const std::string& metaInfoUri, const ValueBase* torrent,
    bool adjustAnnounceUri)
{
  std::vector<std::string> nargs;
  if (option->get(PREF_PARAMETERIZED_URI) == A2_V_TRUE) {
    unfoldURI(nargs, uris);
  }
  else {
    nargs = uris;
  }
  // we ignore -Z option here
  size_t numSplit = option->getAsInt(PREF_SPLIT);
  auto rg = createBtRequestGroup(metaInfoUri, option, nargs, torrent,
                                 adjustAnnounceUri);
  rg->setNumConcurrentCommand(numSplit);
  result.push_back(rg);
}

#endif // ENABLE_BITTORRENT

#ifdef ENABLE_METALINK
void createRequestGroupForMetalink(
    std::vector<std::shared_ptr<RequestGroup>>& result,
    const std::shared_ptr<Option>& option, const std::string& metalinkData)
{
  if (metalinkData.empty()) {
    Metalink2RequestGroup().generate(result, option->get(PREF_METALINK_FILE),
                                     option,
                                     option->get(PREF_METALINK_BASE_URI));
  }
  else {
    auto dw = std::make_shared<ByteArrayDiskWriter>();
    dw->setString(metalinkData);
    Metalink2RequestGroup().generate(result, dw, option,
                                     option->get(PREF_METALINK_BASE_URI));
  }
}
#endif // ENABLE_METALINK

namespace {
class AccRequestGroup {
private:
  std::vector<std::shared_ptr<RequestGroup>>& requestGroups_;
  ProtocolDetector detector_;
  std::shared_ptr<Option> option_;
  bool ignoreLocalPath_;
  bool throwOnError_;

public:
  AccRequestGroup(std::vector<std::shared_ptr<RequestGroup>>& requestGroups,
                  std::shared_ptr<Option> option, bool ignoreLocalPath = false,
                  bool throwOnError = false)
      : requestGroups_(requestGroups),
        option_(std::move(option)),
        ignoreLocalPath_(ignoreLocalPath),
        throwOnError_(throwOnError)
  {
  }

  void operator()(const std::string& uri)
  {
    if (detector_.isStreamProtocol(uri)) {
      std::vector<std::string> streamURIs;
      size_t numIter = option_->getAsInt(PREF_MAX_CONNECTION_PER_SERVER);
      size_t numSplit = option_->getAsInt(PREF_SPLIT);
      size_t num = std::min(numIter, numSplit);
      for (size_t i = 0; i < num; ++i) {
        streamURIs.push_back(uri);
      }
      auto rg = createRequestGroup(option_, streamURIs);
      rg->setNumConcurrentCommand(numSplit);
      requestGroups_.push_back(rg);
    }
#ifdef ENABLE_BITTORRENT
    else if (detector_.guessTorrentMagnet(uri)) {
      requestGroups_.push_back(createBtMagnetRequestGroup(uri, option_));
    }
    else if (!ignoreLocalPath_ && detector_.guessTorrentFile(uri)) {
      try {
        bittorrent::ValueBaseBencodeParser parser;
        auto torrent = parseFile(parser, uri);
        if (!torrent) {
          throw DL_ABORT_EX2("Bencode decoding failed",
                             error_code::BENCODE_PARSE_ERROR);
        }
        requestGroups_.push_back(
            createBtRequestGroup(uri, option_, {}, torrent.get()));
      }
      catch (RecoverableException& e) {
        if (throwOnError_) {
          throw;
        }
        else {
          // error occurred while parsing torrent file.
          // We simply ignore it.
          A2_LOG_ERROR_EX(EX_EXCEPTION_CAUGHT, e);
        }
      }
    }
#endif // ENABLE_BITTORRENT
    else if (detector_.guessEd2kLink(uri)) {
      try {
        requestGroups_.push_back(createEd2kRequestGroup(uri, option_));
      }
      catch (RecoverableException& e) {
        if (throwOnError_) {
          throw;
        }
        else {
          A2_LOG_ERROR_EX(EX_EXCEPTION_CAUGHT, e);
        }
      }
    }
#ifdef ENABLE_METALINK
    else if (!ignoreLocalPath_ && detector_.guessMetalinkFile(uri)) {
      try {
        Metalink2RequestGroup().generate(requestGroups_, uri, option_,
                                         option_->get(PREF_METALINK_BASE_URI));
      }
      catch (RecoverableException& e) {
        if (throwOnError_) {
          throw;
        }
        else {
          // error occurred while parsing metalink file.
          // We simply ignore it.
          A2_LOG_ERROR_EX(EX_EXCEPTION_CAUGHT, e);
        }
      }
    }
#endif // ENABLE_METALINK
    else {
      if (throwOnError_) {
        throw DL_ABORT_EX(fmt(MSG_UNRECOGNIZED_URI, uri.c_str()));
      }
      else {
        A2_LOG_ERROR(fmt(MSG_UNRECOGNIZED_URI, uri.c_str()));
      }
    }
  }
};
} // namespace

namespace {
class StreamProtocolFilter {
private:
  ProtocolDetector detector_;

public:
  bool operator()(const std::string& uri)
  {
    return detector_.isStreamProtocol(uri);
  }
};
} // namespace

void createRequestGroupForUri(
    std::vector<std::shared_ptr<RequestGroup>>& result,
    const std::shared_ptr<Option>& option, const std::vector<std::string>& uris,
    bool ignoreForceSequential, bool ignoreLocalPath, bool throwOnError)
{
  std::vector<std::string> nargs;
  if (option->get(PREF_PARAMETERIZED_URI) == A2_V_TRUE) {
    unfoldURI(nargs, uris);
  }
  else {
    nargs = uris;
  }
  if (!ignoreForceSequential &&
      option->get(PREF_FORCE_SEQUENTIAL) == A2_V_TRUE) {
    std::for_each(
        std::begin(nargs), std::end(nargs),
        AccRequestGroup(result, option, ignoreLocalPath, throwOnError));
  }
  else {
    auto strmProtoEnd = std::stable_partition(
        std::begin(nargs), std::end(nargs), StreamProtocolFilter());
    // let's process http/ftp protocols first.
    if (std::begin(nargs) != strmProtoEnd) {
      size_t numIter = option->getAsInt(PREF_MAX_CONNECTION_PER_SERVER);
      size_t numSplit = option->getAsInt(PREF_SPLIT);
      std::vector<std::string> streamURIs;
      splitURI(streamURIs, std::begin(nargs), strmProtoEnd, numSplit, numIter);
      try {
        auto rg = createRequestGroup(option, streamURIs, true);
        rg->setNumConcurrentCommand(numSplit);
        result.push_back(rg);
      }
      catch (RecoverableException& e) {
        if (throwOnError) {
          throw;
        }
        else {
          A2_LOG_ERROR_EX(EX_EXCEPTION_CAUGHT, e);
        }
      }
    }
    // process remaining URIs(local metalink, BitTorrent files)
    std::for_each(
        strmProtoEnd, std::end(nargs),
        AccRequestGroup(result, option, ignoreLocalPath, throwOnError));
  }
}

bool createRequestGroupFromUriListParser(
    std::vector<std::shared_ptr<RequestGroup>>& result, const Option* option,
    UriListParser* uriListParser)
{
  // Since result already contains some entries, we cache the size of
  // it. Later, we use this value to determine RequestGroup is
  // actually created.
  size_t num = result.size();
  while (uriListParser->hasNext()) {
    std::vector<std::string> uris;
    Option tempOption;
    uriListParser->parseNext(uris, tempOption);
    if (uris.empty()) {
      continue;
    }
    auto requestOption = std::make_shared<Option>(*option);
    requestOption->remove(PREF_OUT);
    const auto& oparser = OptionParser::getInstance();
    for (size_t i = 1, len = option::countOption(); i < len; ++i) {
      auto pref = option::i2p(i);
      auto h = oparser->find(pref);
      if (h && h->getInitialOption() && tempOption.defined(pref)) {
        requestOption->put(pref, tempOption.get(pref));
      }
    }
    // This does not throw exception because throwOnError = false.
    createRequestGroupForUri(result, requestOption, uris);
    if (num < result.size()) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<UriListParser> openUriListParser(const std::string& filename)
{
  std::string listPath;

  auto f = File(filename);
  if (!f.exists() || f.isDir()) {
    throw DL_ABORT_EX(fmt(EX_FILE_OPEN, filename.c_str(),
                          "File not found or it is a directory"));
  }
  listPath = filename;

  return std::make_shared<UriListParser>(listPath);
}

void createRequestGroupForUriList(
    std::vector<std::shared_ptr<RequestGroup>>& result,
    const std::shared_ptr<Option>& option)
{
  auto uriListParser = openUriListParser(option->get(PREF_INPUT_FILE));
  while (createRequestGroupFromUriListParser(result, option.get(),
                                             uriListParser.get()))
    ;
}

std::shared_ptr<MetadataInfo> createMetadataInfoFromFirstFileEntry(
    const std::shared_ptr<GroupId>& gid,
    const std::shared_ptr<DownloadContext>& dctx)
{
  if (dctx->getFileEntries().empty()) {
    return nullptr;
  }
  else {
    auto uris = dctx->getFileEntries()[0]->getUris();
    if (uris.empty()) {
      return nullptr;
    }
    return std::make_shared<MetadataInfo>(gid, uris[0]);
  }
}

void removeOneshotOption(const std::shared_ptr<Option>& option)
{
  option->remove(PREF_PAUSE);
  option->remove(PREF_GID);
  option->remove(PREF_ED2K_SERVER_STATE);
  option->remove(PREF_ED2K_SHARED_FILE_STATE);
  option->remove(PREF_ED2K_KAD_ROUTING_STATE);
}

} // namespace aria2
