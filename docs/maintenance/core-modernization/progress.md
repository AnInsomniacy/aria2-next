# Core Modernization Progress

This file is the compact chronological evidence trail for core modernization.
Keep entries checkpoint-sized. Do not record raw logs, packet captures,
generated reports, local public-network data, temporary downloads, local
caches, API payloads, or conversation text.

Use this format:

```text
YYYY-MM-DD CM-XXX status
Changed: concise tracker, code, or behavior summary.
Verified: exact final command and result, or documentation-only reason.
Remaining: next concrete gap.
Blocked: none, or exact blocker.
```

## Current Baseline

The completed `docs/maintenance/libtorrent-bt-migration` tracker is the
BitTorrent baseline. BitTorrent is owned by libtorrent-rasterbar and native
BitTorrent fallback is not allowed.

The completed `docs/maintenance/ed2k-refactor` and
`docs/maintenance/ed2k-download-hardening` trackers are the ED2K baseline.
ED2K remains native because no suitable maintained embeddable replacement
library has been identified. Future modernization should move ED2K onto modern
shared runtime, storage, crypto, and compression boundaries where practical
without reopening the protocol implementation as a wholesale replacement.

Current source inventory shows large remaining native or legacy areas:
HTTP/RPC/WebSocket, FTP/SFTP/SSH, network runtime, Metalink/XML, disk and
piece storage, crypto/compression, and core orchestration. These areas are the
scope of this tracker.

## Log

2026-05-23 CM-001 verified
Changed: Created the core modernization tracker with a no-fallback
modernization contract, library-first policy, removal policy, 99 percent
completion policy, focused test policy, roadmap, capability ledger, dependency
ledger, progress log, maintainer-selected final smoke fixtures, and nineteen
checkpoint matrices. Updated the maintenance root index to point to the new
tracker.
Verified: CSV parser check passed for 22 files. `git diff --check
docs/maintenance/README.md docs/maintenance/core-modernization` passed.
Remaining: Start CM-002 dependency policy when implementation begins.
Blocked: none.

2026-05-23 CM-002 verified
Changed: Promoted the dependency policy checkpoint from pending to verified.
Recorded the selected target dependency set, explicit legacy dependency removal
set, retained native ED2K and storage ownership, packaging dependency version
source, and the boundary that actual source and packaging removals start in
later implementation checkpoints.
Verified: Current evidence was read from AGENTS.md, overview.md, roadmap.csv,
capability-ledger.csv, dependency-ledger.csv, the CM-002 checkpoint file, prior
libtorrent and ED2K tracker overviews, CMakeLists.txt, cmake/modules,
cmake/Sources.cmake, cmake/TestSources.cmake, packaging/dependencies.env, and
README.md. The tracker records that legacy CMake options, source lists, README
claims, and packaging dependencies still exist before CM-003+ migration work.
Remaining: Start CM-003 build baseline.
Blocked: none.

2026-05-23 CM-003 verified
Changed: Raised the project build baseline to C++17, added required CMake gates
for libcurl, Boost.JSON, OpenSSL for SSL, zlib when enabled, and
libtorrent-rasterbar for BitTorrent, linked aria2_core against the target
dependency set, removed obsolete WinTLS, GnuTLS, nettle, GMP, libgcrypt, and
GnuTLS policy build switches and generated-config defines, and updated README
build option and TLS wording.
Verified: `cmake --preset default` passed and reported SSL=1, OpenSSL=1,
libcurl=1, Boost.JSON=1, BitTorrent=1, and libtorrent-rasterbar=1.
`cmake --build --preset default` passed and linked aria2-next and aria2_tests.
Stale build-option scan over cmake/modules, cmake/config.h.cmake.in, and
README.md found no removed WinTLS/GnuTLS/nettle/GMP/libgcrypt option or
generated-config names.
Remaining: Start CM-004 runtime foundation.
Blocked: none.

2026-05-23 CM-004 verified
Changed: Added `AsioRuntime` as the Boost.Asio runtime owner for posted tasks,
timers, wake requests, cancellation, and shutdown wake bridging. DownloadEngine
now owns the runtime, drains ready work around the legacy EventPoll bridge,
wakes the runtime on graceful and force halt, and consumes wake requests after
forcing one non-blocking poll. Recorded the old EventPoll deletion plan for the
post-protocol-migration cleanup boundary.
Verified: `cmake --preset default` passed. `cmake --build --preset default`
passed. `build/default/aria2_tests 'All Tests/aria2::DownloadEngineTest'`
passed with 4 tests. CSV parser check passed for 22 tracker files. `git diff
--check` passed.
Remaining: Start CM-005 libcurl transfer foundation.
Blocked: none.

2026-05-23 CM-005 verified
Changed: Added CurlSession as the libcurl multi owner with socket and timer
callbacks bridged through DownloadEngine. Added CurlDownloadCommand for the
first ordinary URL transfer path. HTTP/HTTPS initiation now routes to the curl
command while FTP/SFTP remain deferred. The curl data callback writes through
existing PieceStorage, SegmentMan, and DiskAdaptor boundaries, and completion
uses existing RequestGroup state.
Verified: `cmake --preset default` passed. `cmake --build --preset default`
passed. `build/default/aria2_tests
'All Tests/aria2::RequestGroupTest/aria2::RequestGroupTest::testInitiateConnectionFactoryUsesCurlForHttp'`
passed. `build/default/aria2_tests 'All Tests/aria2::DownloadEngineTest'`
passed with 4 tests. Local HTTP smoke under
`/Users/sekiro/Desktop/aria2-next-current/cm005-curl-smoke` exited 0 and
downloaded `curl-smoke-ok`. CSV parser check passed for 22 tracker files. `git
diff --check` passed.
Remaining: Start CM-006 HTTP and HTTPS feature migration.
Blocked: none.

2026-05-23 CM-006 verified
Changed: Expanded `CurlDownloadCommand` into the HTTP/HTTPS transfer owner for metadata probing, request option mapping, known-length storage setup, range requests, resume, unknown-length fallback, and libcurl error surfacing. Removed direct native HTTP initiation from the command factory and deleted `HttpInitiateConnectionCommand`; retained the remaining old HTTP helper graph only where FTP proxy and RPC server cleanup still own it in later checkpoints.
Verified: `cmake --build --preset default` passed. `build/default/aria2_tests 'All Tests/aria2::RequestGroupTest/aria2::RequestGroupTest::testInitiateConnectionFactoryUsesCurlForHttp'` passed. `build/default/aria2_tests 'All Tests/aria2::DownloadEngineTest'` passed. Local smokes under `/Users/sekiro/Desktop/aria2-next-current/cm006-final-smoke` verified HEAD metadata, byte-range GETs, resume-only missing ranges, and unknown-length fallback; earlier CM-006 smokes verified option propagation and 404 failure surfacing. CSV parser check passed for 22 tracker files. `git diff --check` passed.
Remaining: Start CM-007 FTP, FTPS, SFTP, and SCP migration through libcurl, then remove native FTP/SFTP and the temporary FTP proxy HTTP helper dependencies.
Blocked: none.

2026-05-23 CM-007 verified
Changed: Migrated FTP, FTPS, SFTP, and SCP initiation to `CurlDownloadCommand`. `CurlDownloadCommand` now maps FTP-family credentials, active/passive FTP mode, ASCII transfer mode, FTPS SSL ownership, SFTP/SCP private-key option, libcurl protocol allowlists, and libcurl-reported metadata length. Removed native FTP/SFTP command graphs, `SSHSession`, direct SocketCore SSH hooks, Platform libssh2 lifecycle, direct libssh2 CMake option/linkage, and the obsolete `FtpConnectionTest` parser fixture.
Verified: `cmake --build --preset default` passed. `build/default/aria2_tests 'All Tests/aria2::RequestGroupTest/aria2::RequestGroupTest::testInitiateConnectionFactoryUsesCurlForFtpFamily'` passed. `build/default/aria2_tests 'All Tests/aria2::ProtocolDetectorTest/aria2::ProtocolDetectorTest::testIsStreamProtocol'` passed. `build/default/aria2_tests 'All Tests/aria2::FeatureConfigTest'` passed. Stale scans over src, tests, cmake, README.md, and tracker files found no direct FTP/SFTP/libssh2 source residue outside tracker records.
Remaining: Start CM-008 cookie, auth, proxy, and netrc cleanup.
Blocked: none.

2026-05-23 CM-008 verified
Changed: Moved client auth, cookie, netrc, and proxy ownership to libcurl. `CurlDownloadCommand` now maps HTTP credentials, FTP-family credentials, libcurl cookie file load/save, netrc, proxy URI, no-proxy, tunnel mode, and proxy credentials. Removed native CookieStorage, Cookie, cookie_helper, NsCookieParser, Sqlite3CookieParser, Netrc, AuthConfig/AuthResolver, client HttpRequest/HttpResponse/HttpConnection, client proxy helper graphs, obsolete options `http-auth-challenge`, `ftp-reuse-connection`, and `ssh-host-key-md`, stale internal tests, and SQLite build and packaging ownership.
Verified: `cmake --build --preset default` passed. Focused RequestGroup curl routing tests passed. `build/default/aria2_tests 'All Tests/aria2::FeatureConfigTest'` passed. `build/default/aria2_tests 'All Tests/aria2::OptionHandlerTest'` passed. `git diff --check` passed. Packaging syntax checks passed for release workflow, CI workflow, and Docker release files. CSV parser check passed for 22 tracker files. Stale scans found no removed source files, SQLite ownership, removed options, or deleted client auth/cookie/proxy symbol ownership in active source, tests, CMake, README, workflows, or packaging.
Remaining: Start CM-009 Boost.Beast JSON-RPC HTTP server.
Blocked: none.

2026-05-23 CM-009 verified
Changed: Replaced the native JSON-RPC HTTP server command chain with Boost.Beast transport on the Asio runtime. Added `RpcBeastServer`, `RpcBeastSession`, `RpcHttpHandler`, and `AsioPumpCommand`; wired RPC startup through Beast; pruned deprecated `rpc-user`, `rpc-passwd`, `rpc-secure`, `rpc-certificate`, and `rpc-private-key`; deleted native `HttpServer*`, `HttpListenCommand`, `AbstractHttpServerResponseCommand`, `WebSocketResponseCommand`, and `HttpServerTest`.
Verified: `cmake --build --preset default` passed. `build/default/aria2_tests 'All Tests/aria2::RpcHttpHandlerTest'` passed. `build/default/aria2_tests 'All Tests/aria2::rpc::RpcMethodTest/aria2::rpc::RpcMethodTest::testAuthorize'` passed. `build/default/aria2_tests 'All Tests/aria2::rpc::RpcMethodTest/aria2::rpc::RpcMethodTest::testSystemMulticall'` passed. `build/default/aria2_tests 'All Tests/aria2::DownloadEngineTest'` passed. `build/default/aria2_tests 'All Tests/aria2::OptionHandlerTest'` passed. `build/default/aria2_tests 'All Tests/aria2::FeatureConfigTest'` passed. Local Beast JSON-RPC smoke under `/Users/sekiro/Desktop/aria2-next-current/cm009-rpc-smoke` returned `aria2.getVersion` through HTTP POST with `rpc-secret`. Stale scans for removed native HTTP server files and pruned RPC options passed. CSV parser check passed for 22 tracker files. `git diff --check` passed.
Remaining: Start CM-010 Boost.Beast WebSocket transport.
Blocked: none.

2026-05-23 CM-010 verified
Changed: Migrated JSON-RPC WebSocket transport from the native wslay session graph to Boost.Beast on the shared Asio runtime. Added `RpcWebSocketSession`; wired WebSocket upgrades through `RpcBeastServer`; changed `WebSocketSessionMan` into a Beast session notification registry; removed `WebSocketInteractionCommand`, old `WebSocketSession`, vendored `third_party/wslay`, generated wslay header setup, the wslay build target, and wslay link ownership.
Verified: `cmake --build --preset default` passed. `build/default/aria2_tests 'All Tests/aria2::rpc::WebSocketSessionManTest'` passed. `build/default/aria2_tests 'All Tests/aria2::RpcHttpHandlerTest'` passed. `build/default/aria2_tests 'All Tests/aria2::rpc::RpcMethodTest/aria2::rpc::RpcMethodTest::testAuthorize'` passed. Local WebSocket smoke under `/Users/sekiro/Desktop/aria2-next-current/cm010-websocket-smoke` returned `aria2.getVersion` through `ws://127.0.0.1:6891/jsonrpc` with `rpc-secret`. Stale active-source scans found no wslay or old native WebSocket session graph references outside tracker history.
Remaining: Start CM-011 Boost.JSON migration.
Blocked: none.

2026-05-23 CM-011 verified
Changed: Added `BoostJsonValue` as the Boost.JSON conversion boundary for internal `ValueBase` RPC values. Migrated JSON-RPC HTTP and WebSocket request parsing, RPC response serialization, and WebSocket notification serialization to Boost.JSON. Removed `JsonParser`, `JsonDiskWriter`, `ValueBaseJsonParser`, and `ValueBaseJsonParserTest`; retained `json.cc/json.h` only for JSONP GET query shaping.
Verified: `cmake --preset default` passed. `cmake --build --preset default` passed. `build/default/aria2_tests 'All Tests/aria2::JsonTest'` passed. `build/default/aria2_tests 'All Tests/aria2::rpc::RpcResponseTest'` passed. `build/default/aria2_tests 'All Tests/aria2::RpcHttpHandlerTest'` passed. `build/default/aria2_tests 'All Tests/aria2::rpc::WebSocketSessionManTest'` passed. `build/default/aria2_tests 'All Tests/aria2::rpc::RpcMethodTest/aria2::rpc::RpcMethodTest::testAuthorize'` passed. CSV parser check passed for 22 tracker files. Stale scans found no old custom JSON parser or writer references outside tracker history.
Remaining: Start CM-012 XML-RPC removal and Metalink decision.
Blocked: none.

2026-05-23 CM-012 verified
Changed: Removed XML-RPC, Metalink3, Metalink4, libxml2, Expat, XML parser
wrappers, XML parser state machines, Metalink request creation, RPC
`addMetalink`, libaria2 `addMetalink`, follow-metalink handling, Metalink
protocol detection, related options, stale tests, public manual references,
completion entries, packaging Expat builds, and XML dependency feature flags.
Added `ValueBaseFrameController` to keep the remaining ValueBase struct parser
independent of XML-specific parser infrastructure.
Verified: `cmake --preset default` passed. `cmake --build --preset default`
passed. `ctest --preset default --output-on-failure` passed. Public stale scans
found no XML-RPC, Metalink, Expat, or libxml2 residue in source, tests, CMake,
README, packaging, completion, or manual docs. Completion and packaging script
syntax checks passed. CSV parser check passed for tracker files. `git diff
--check` passed.
Remaining: Start CM-013 crypto TLS cleanup.
Blocked: none.

2026-05-23 CM-013 verified
Changed: Centralized direct TLS and crypto ownership on OpenSSL. Removed GnuTLS, WinTLS, nettle, libgcrypt, GMP, internal digest, internal ARC4, and DHKeyExchange residue. Kept ED2K's narrow native MD4 helper and mapped MD5, SHA-1/AICH, and RC4 obfuscation through OpenSSL-backed paths. Cleaned public docs, release workflow, Dockerfiles, Windows package note, and dependency records.
Verified: `cmake --preset default` passed. `cmake --build --preset default` passed. `ctest --preset default --output-on-failure` passed. Focused MessageDigest, FeatureConfig, SocketCore, and ED2K MD4/AICH/obfuscation tests passed. HTTPS smoke under `/Users/sekiro/Desktop/aria2-next-current/cm013-https-smoke` completed with certificate checking enabled. Packaging script syntax checks, CSV parser check, stale backend scans, and `git diff --check` passed.
Remaining: Start CM-014 modern storage and completion truth.
Blocked: none.

2026-05-23 CM-014 verified
Changed: Split non-BitTorrent storage progress into verified `completedLength` and separate `inFlightCompletedLength`. Updated PieceStorage, DefaultPieceStorage, UnknownLengthPieceStorage, RequestGroup, DownloadResult, JSON-RPC status output, public API comments, and manual docs. Added focused `StorageTruthTest` plus RequestGroup and RPC coverage for false-completion prevention.
Verified: `cmake --build --preset default` passed. Full `ctest --preset default --output-on-failure` passed. Focused StorageTruth, RequestGroup, RpcMethod, DefaultProgressInfoFile, Ed2kCommand, and ED2K transfer reclaim/parallel block tests passed. CSV parser check and `git diff --check` passed.
Remaining: Start CM-015 ED2K bridge onto modern runtime, storage, crypto, and status boundaries.
Blocked: none.

2026-05-23 CM-015 verified
Changed: Bridged ED2K scheduling further onto the Asio runtime by moving Kad polling from routine-command spin to runtime wake scheduling and waking the runtime when ED2K peer/server work is queued. Added ED2K RPC subfields for verified `completedLength` and separate `inFlightCompletedLength`. Removed the dead ED2K `inflateCompressedPartData` helper and stale test while retaining streaming compressed-part and packed-packet zlib paths.
Verified: `cmake --build --preset default --target aria2_tests` passed. Focused ED2K helper, Kad state, shared store, command, DownloadHelper, RequestGroupMan ED2K sharing, SessionSerializer ED2K save, and RpcMethod suites passed. Full `cmake --build --preset default`, `ctest --preset default --output-on-failure`, CSV parser check, active source/test stale `inflateCompressedPartData` scan, and `git diff --check` passed.
Remaining: Start CM-016 libtorrent boundary residue review.
Blocked: none.
