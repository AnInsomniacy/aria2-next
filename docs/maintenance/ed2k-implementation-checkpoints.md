# ED2K/eMule Implementation Checkpoints

This document is the durable working record for native ED2K/eMule support in
aria2-next. It exists to survive long goal runs and context compaction. Future
agents must read this file before continuing ED2K work, update it after each
checkpoint, and avoid relying on conversation history as the source of truth.

## Scope Contract

The target is a complete native ED2K/eMule implementation for aria2-next, not a
minimal downloader and not a compatibility stub. "Complete" means every
protocol, behavior, persistence, status, and integration capability that is
needed for real ED2K/eMule client operation must be implemented or explicitly
rejected with a documented reason. The reference repositories must be treated as
the parity checklist for the protocol domain, while aria2-next remains the host
architecture.

The implementation must stay inside the existing C++11 codebase and integrate
with the current CMake, Command/EventPoll, DownloadEngine, RequestGroup,
DownloadContext, PieceStorage, DiskAdaptor, CLI, RPC, session, and documentation
patterns. It must not introduce a Go runtime, Java runtime, sidecar daemon,
Boost, Asio, a second event loop, or unrelated architecture changes.

The reference repositories under
`/Users/sekiro/Projects/oss/ed2k-references` are protocol and design references
only. They must not be copied into aria2-next. The relevant references are
`goed2k-core`, `goed2kd`, `amule`, `jed2k`, and `libed2k-qmule`.

The current ED2K code is a draft. It may be retained, refactored, or replaced as
needed, but each change must keep the implementation focused and verifiable.
Avoid placeholder files, empty scaffolding, broad unrelated refactors, fake
tests, and documentation claims that exceed the implementation.

## Completeness and Pruning Policy

This work should be a full protocol and behavior port, not a line-by-line code
port. The goal is to reproduce the useful ED2K/eMule capabilities from
`goed2k-core`, `goed2kd`, `amule`, `jed2k`, and `libed2k-qmule` in native
aria2-next C++11 using aria2-next's event loop, disk, session, CLI, RPC, and
documentation surfaces.

All meaningful reference capabilities start as in-scope. A capability can be
removed from the target only when it is obsolete, GUI-only, daemon-only,
platform-specific, dangerous, unmaintainable, irrelevant to aria2-next's CLI/RPC
model, or dependent on architecture that this project explicitly rejects. Every
removed capability must be recorded in the pruning ledger with the reference
files inspected, the reason for removal, and the user-visible impact.

The implementation may aggressively delete or avoid old edge behavior when the
reason is real. It must not prune by pretending a missing feature is complete.
If a reference behavior is old but still affects interoperability with ED2K,
eMule, aMule, Kad, Source Exchange, AICH, sharing, upload queues, or peer
credits, it remains in scope unless the maintainer accepts the documented
removal.

Do not advertise compatibility bits for pruned capabilities. If a capability is
removed as obsolete, GUI-only, social, or unsafe, the implementation should
avoid parsing it into supported state unless that state is required to reject or
ignore a packet safely.

The parity target includes download, search, discovery, verification, resume,
sharing, upload, server, Kad, status, RPC, CLI, and persistence behavior. GUI
features, web interfaces, remote-control protocols from other applications,
chat, IRC, collection UI, and unrelated application preferences are not native
aria2-next goals unless they carry protocol data required for interoperability.

## Verification Discipline

The ED2K work already has a large draft test footprint. Future verification
must stay meaningful and narrow. Add tests only when they prove a protocol
boundary, parser invariant, state-machine transition, persistence contract,
disk/write integrity rule, RPC field contract, or a fixed regression root cause.

Do not add broad scaffolding, placeholder tests, generated-looking matrix tests,
tests that only mirror implementation details, or tests that reimplement a
reference project in C++ assertions. Prefer one compact parser/state test and
one local command simulation over many near-duplicate cases. When behavior is
covered by an existing aria2-next path, reuse that path and add only the ED2K
specific assertion.

Checkpoint verification should use the smallest command set that proves the
changed surface. Full `cmake --preset default`, `cmake --build --preset
default`, `ctest --preset default`, and `build/default/aria2-next --version`
remain the final CP18 gate, not the routine command after every small edit.

## Current Snapshot

Snapshot date: 2026-05-18.

Repository: `/Users/sekiro/Projects/personal/aria2-next`.

Current branch: `main`.

Current HEAD observed during audit: `57650b79`.

The ED2K work is uncommitted. Modified tracked files include
`cmake/Sources.cmake`, `cmake/TestSources.cmake`,
`docs/completion/aria2-next`, `docs/manual/en/aria2-next.rst`,
`src/ContextAttribute.cc`, `src/ContextAttribute.h`,
`src/OptionHandlerFactory.cc`, `src/ProtocolDetector.cc`,
`src/ProtocolDetector.h`, `src/RequestGroup.cc`,
`src/RpcMethodFactory.cc`, `src/RpcMethodImpl.cc`,
`src/RpcMethodImpl.h`, `src/SessionSerializer.cc`,
`src/download_helper.cc`, `src/download_helper.h`, `src/prefs.cc`,
`src/prefs.h`, `src/usage_text.h`, `tests/DownloadHelperTest.cc`,
`tests/ProtocolDetectorTest.cc`, and `tests/SessionSerializerTest.cc`.

Untracked ED2K files include `src/Ed2kAttribute.cc`,
`src/Ed2kAttribute.h`, `src/Ed2kCommand.cc`, `src/Ed2kCommand.h`,
`src/Ed2kKadCommand.cc`, `src/Ed2kKadCommand.h`,
`src/Ed2kKadState.cc`, `src/Ed2kKadState.h`, `src/ed2k_helper.cc`,
`src/ed2k_helper.h`, `tests/Ed2kHelperTest.cc`, and
`tests/Ed2kKadStateTest.cc`.

Observed size of the ED2K draft is already large: `src/ed2k_helper.cc` is
2666 lines, `src/Ed2kCommand.cc` is 966 lines, `src/Ed2kKadCommand.cc` is
390 lines, and the new or expanded ED2K-related tests are over 2300 lines.
This is enough code to justify a stabilization checkpoint before adding more
protocol behavior.

## Local Verification Snapshot

The latest targeted CP1 test run passed:

```bash
cmake --build --preset default --target aria2_tests
ctest --preset default --output-on-failure -R aria2_tests
```

Observed result:

```text
100% tests passed, 0 tests failed out of 1
```

Resolved CP1 failures:

| Test | File | Observed failure | Current reading |
| --- | --- | --- | --- |
| `DownloadHelperTest::testEd2kPeerCommandAnswersSourceExchange2` | `tests/DownloadHelperTest.cc:732` | Expected opcode `132`, actual opcode `1` | Root cause was not frame ordering. Source Exchange answer creation queued an entry with an empty server endpoint, and packet serialization rejected the empty IPv4 address before the answer could be written. `Ed2kCommand::queueSourceExchangeAnswer` now emits `0.0.0.0:0` for an unknown source server. |
| `DownloadHelperTest::testEd2kKadCommandUpdatesServerUdpStatus` | `tests/DownloadHelperTest.cc:785` | Expected users `1234`, actual users `0` | Root cause was test scheduling. The UDP status response was sent to a routine command socket, but the test ran the engine without first waiting for the local UDP socket to become readable. The command path now exposes a narrow local socket readability probe used only by this test. |

This does not mean ED2K is complete. It only means the current ED2K draft
baseline is no longer blocked by the two known CP1 test failures.

## Current Capability Inventory

The current draft has useful foundations. ED2K file links are routed through
`ProtocolDetector` and `download_helper`. ED2K download request groups are
created with `DownloadContext`, `DefaultPieceStorage`, `SegmentMan`, and the
existing disk path. Server endpoints can come from `--ed2k-server` and
`server.met`. Kad bootstrap nodes can come from `nodes.dat`. Search request
groups exist through RPC methods `aria2.ed2kSearch` and
`aria2.getEd2kSearchResults`. Save-session writes the ED2K file link plus
hidden server and Kad routing state options. Server TCP login, IDChange,
GetSources, FoundSources, callback request handling, server messages, server
lists, and UDP global status parsing are partially present. Peer TCP
handshake, eMule extended info, file request, file status, hashset request,
Source Exchange request and answer handling, normal part transfer, compressed
part transfer, MD4 part verification, and AICH packet parsing are partially
present.

The current draft is not complete ED2K/eMule support. It has no complete peer
listener for incoming TCP connections. Upload behavior is not an upload queue
with slots, ranks, limits, statistics, and credits. Sharing and imported shared
files are not represented as first-class persistent state. Kad has packet
helpers, bootstrap, hello, and simple search requests, but it does not yet have
a full traversal engine, publish loop, firewalled checks, routing refresh
policy, or durable operational state. AICH has hashing and packet parsing, but
not full tree verification, trust handling, or recovery application. Resume
persists only limited ED2K state. ED2K status is not exposed through CLI/RPC
with the full server, peer, Kad, queue, search, share, and upload model.

The documentation currently claims more than the implementation can prove. The
manual says ED2K support currently includes Source Exchange, Kad discovery,
compressed part decoding, MD4 part verification, and AICH metadata exchange,
while also saying sharing and broader persistence are incomplete. That wording
must be corrected before any release-facing commit.

## Architecture Findings

`src/RequestGroup.cc` follows a reasonable integration direction. ED2K uses the
existing request group lifecycle, duplicate-file guard, disk adaptor opening,
piece storage initialization, control file loading, and command scheduling.
That path should remain the spine of the implementation.

`src/Ed2kAttribute.*` is currently the shared state bag. It stores link
metadata, servers, server states, peers, piece hashes, AICH root hash, search
state, Kad routing table, Kad transactions, and scheduling cursors. This is
practical for a draft, but it should not become the final home for every ED2K
subsystem. The next design should keep `Ed2kAttribute` as the RequestGroup-owned
coordination object and move protocol-specific state into smaller owned
components.

`src/ed2k_helper.*` is doing too much. It combines MD4, AICH hashing, link
parsing, endpoint parsing, tag parsing, packet framing, server.met parsing,
server state persistence, server search parsing, Kad packet parsing, nodes.dat
parsing, Source Exchange, compressed part parsing, eMule info, and AICH packets.
The file should be split by protocol responsibility before more behavior is
added.

`src/Ed2kCommand.*` is also overloaded. One command currently handles server
TCP and peer TCP, outbound connection setup, handshake, source discovery,
download requests, part writes, verification, Source Exchange, AICH metadata,
and partial upload responses. The target should keep a single event loop, but
separate the state machines into focused components so server sessions, peer
download sessions, and peer upload sessions do not keep growing inside one
class.

`src/Ed2kKadCommand.*` has a useful UDP socket integration. It binds an IPv4
UDP socket, uses aria2's read-check path, sends bootstrap/search/status packets,
and receives responses. It still lacks a real Kad traversal controller. The
next Kad work should build traversal state on top of this command rather than
spreading more one-off booleans through it.

## Reference Map

`goed2k-core` is the best modern architecture reference. Its `session.go`
aggregates transfers, server connections, DHT tracker, upload queue, credits,
shared store, and state. Its `transfer.go`, `policy.go`, `piece_picker.go`,
`downloading_piece.go`, and `peer_connection.go` show how peer sources,
piece/block selection, queue state, resume, Source Exchange, compressed
transfer, upload responses, and sharing fit together. Its `upload_queue.go`,
`client_credits.go`, `shared_file.go`, `client_state.go`, `dht_tracker.go`,
`kad_routing.go`, and `kad_traversal.go` are the main references for the
missing parts.

`goed2kd` is mainly useful for API and daemon-facing state shape. It should not
drive the native architecture because aria2-next must not gain a sidecar or Go
runtime.

`amule` is the strongest behavioral reference for mature eMule compatibility.
Its `PartFile`, `KnownFile`, `SharedFileList`, `UploadQueue`,
`UploadClient`, `ClientCredits`, `SearchList`, Kademlia code, and AICH-related
thread/hashset files should be used to verify real-world behavior, especially
for AICH, sharing, upload queue semantics, credits, Kad status, and ED2K link
variants.

`jed2k` is useful as a Java protocol cross-check. It clearly separates session,
server connection, peer connection, piece picker, Kad tracker, traversal
algorithms, packet combiners, and protocol message classes.

`libed2k-qmule` is useful for packet structures, opcode handling, transfer
policy, Kad tracker behavior, and C++ naming comparisons. It depends on Boost
and Asio patterns that must not be imported into aria2-next.

## Reference Parity Ledger

Every substantial reference subsystem must be classified before the final
verification checkpoint. The allowed decisions are `port`, `adapt`, `replace`,
or `prune`.

`port` means the behavior is implemented directly in aria2-next terms.
`adapt` means the behavior is preserved but mapped onto existing aria2-next
concepts such as RequestGroup, PieceStorage, DiskAdaptor, TransferStat, options,
or RPC fields. `replace` means aria2-next already has an equivalent mechanism
and the ED2K subsystem will use that mechanism. `prune` means the behavior is
not implemented and the pruning ledger explains why.

| Reference area | Primary reference files or areas | Required decision before completion |
| --- | --- | --- |
| Client/session lifecycle | `goed2k-core/session.go`, `goed2kd/internal/engine/engine.go`, `jed2k/core/.../Session.java`, aMule application/session wiring | `adapt`. Keep aria2-next `DownloadEngine` and `RequestGroup` as the lifecycle owner. Port only the ED2K client responsibilities: transfers, server sessions, peer sessions, incoming listener, Kad tracker, upload queue, credits, shared store, search, and periodic publish. Do not port daemon lifecycle or app shell code. |
| Server connection policy | `goed2k-core/server_connection.go`, `server_connection_policy.go`, `protocol/server/*`, `jed2k` server protocol, aMule `ServerSocket.*`, `ServerUDPSocket.*`, `ServerList.*`, libed2k server packets | `port`. Implement direct servers, server.met, login, HighID/LowID, status, source requests, search, callback, server lists, UDP status, retry/backoff, and persisted server state as native aria2-next command/state objects. Use aria2-next sockets and scheduler. |
| Transfer model | `goed2k-core/transfer.go`, `policy.go`, `piece_picker.go`, `downloading_piece.go`, `block_manager.go`, `piece_manager.go`, `jed2k Policy/PiecePicker`, aMule `PartFile.*`, libed2k `transfer.*`, `piece_picker.*` | `adapt`. Preserve ED2K source ranking, piece/block selection, endgame, failed peer recovery, finished state, and resume behavior, but map all disk writes and completion state to aria2-next `PieceStorage`, `SegmentMan`, `DiskAdaptor`, and control files. |
| Peer protocol | `goed2k-core/peer_connection.go`, `protocol/client/*`, `jed2k/protocol/client/*`, libed2k `peer_connection.*`, `packet_struct.*`, aMule `ClientTCPSocket.*`, `DownloadClient.cpp`, `UploadClient.cpp` | `port`. Implement outgoing and incoming TCP state machines, hello/eMule info, capability negotiation, file request/answer, status, hashsets, queue rank, accept upload, out-of-parts, cancel, 32/64-bit part request/response, compressed transfer, disconnect, duplicate rejection, failure scoring, reconnect backoff, and upload responses. |
| Client UDP reask/callback | aMule `ClientUDPSocket.cpp`, libed2k extended UDP opcodes, goed2k-core Kad/ED2K UDP sharing | `port`. Implement the ED2K client UDP reask, queue rank, file-not-found, queue-full, and callback paths only where they affect interoperability, queue state, LowID behavior, or upload/download continuity. |
| Source Exchange | `goed2k-core/policy_source_exchange.go`, `protocol/client/source_exchange.go`, aMule source exchange paths, libed2k `sources_request`, `sources_answer`, `sources_answer2` | `port`. Implement SX1 and SX2, including SX2 v4 entries, endpoint validation, source hash and crypt option handling, duplicate filtering, source labels, and source backoff. |
| Kad | `goed2k-core/dht_tracker.go`, `kad_node.go`, `kad_routing.go`, `kad_traversal.go`, `session_kad_publish.go`, `protocol/kad/*`, `jed2k` Kad protocol, aMule `src/kademlia/**`, libed2k `src/kademlia/**` | `port`. Implement classic Kad2 bootstrap, hello, routing, traversal, source search, source publish, keyword search, firewalled checks, refresh, and durable routing state. Drop Kad1 as an active compatibility target. |
| Search | `goed2k-core/search.go`, `goed2kd/internal/model/dto.go`, `internal/service/search_service.go`, aMule `SearchList.*`, `SearchFile.*`, `SearchExpr.*`, `jed2k` server/Kad search | `adapt`. Expose server and Kad search through aria2-next CLI/RPC. Preserve stable structured fields for Motrix Next and generate normal ED2K links that can feed `addUri`. Do not port a separate daemon API. |
| Integrity | `goed2k-core/ed2k_file_hash.go`, `protocol/md4.go`, transfer resume paths, aMule `KnownFile.*`, `PartFile.*`, `SHAHashSet.*`, libed2k hash/file structs | `port` plus `replace`. Port ED2K MD4 part/root/final and AICH tree/recovery behavior. Replace direct file state management with aria2-next piece and disk mechanisms. Corrupt data must be rejected, retried, and never persisted as valid resume state. |
| Compression | `goed2k-core/peer_connection.go`, `protocol/client/compressed_part.go`, aMule/libed2k compressed part handlers | `port`. Implement `OP_COMPRESSEDPART` and `OP_COMPRESSEDPART_I64` through aria2-next's zlib path with exact decompressed size and target range validation before normal disk/integrity handling. |
| Sharing | `goed2k-core/shared_file.go`, `shared_store.go`, `shared_import.go`, `shared_publish.go`, aMule `SharedFileList.*`, `KnownFile.*`, `PartFileHashThread.*` | `adapt`. Add a native shared-store model for completed downloads and imported files, but use aria2-next path, disk, hashing, and option patterns. Implement shared metadata, indexing, status/hashset/SX/AICH/part responses, and Kad/source publish. |
| Upload queue | `goed2k-core/upload_queue.go`, `client_credits.go`, `uploadable.go`, aMule `UploadQueue.*`, `UploadClient.cpp`, `ClientCredits.*`, libed2k bandwidth/policy | `adapt`. Implement queue rank, slots, limits, stats, part-serving, and peer credits using aria2-next transfer accounting and option conventions. Do not port aMule UI/friend-slot presentation, but keep protocol-visible ranking and credit behavior. |
| Persistence | `goed2k-core/client_state.go`, `protocol/transfer_resume_data.go`, aMule known.met/known2/AICH concepts, current aria2 session serializer | `adapt`. Persist ED2K metadata, piece/hashset/AICH state, sources, servers, Kad routing, shared files, and credits using restart-safe aria2-next session/control-file patterns. Document format ownership and migration rules. |
| RPC/CLI integration | `goed2kd/internal/model/dto.go`, `docs/API.md`, current aria2 RPC/manual/completion paths, Motrix Next needs | `adapt`. Expose ED2K state through existing aria2-next status/search surfaces where possible. Add stable ED2K fields only where generic fields cannot represent server, peer, Kad, queue, share, upload, or search state. |
| NAT traversal helpers | `goed2k-core/upnp.go`, `internal/upnp/*`, jed2k/libed2k UPnP/NAT-PMP helpers | `replace`. Use existing aria2-next networking and option model if NAT helpers are needed. Do not import a second UPnP/NAT-PMP stack during ED2K work. |
| HTTP/WebSocket daemon API | `goed2kd/internal/rpc/**`, `internal/app/daemon.go`, `docs/API.md` | `prune` for architecture, `adapt` for field shape. Keep useful DTO field ideas for Motrix Next, but do not port the daemon, HTTP router, WebSocket hub, config service, or standalone process model. |
| GUI, web UI, Android UI, chat, IRC, preview UI, collections UI | aMule GUI/webserver/TextClient/EC paths, jed2k Android app paths, libed2k chat/captcha/preview/shared-directory browsing handlers | `prune`. These are application UI or legacy social features, not aria2-next core download/search/share protocol requirements. Keep only protocol-visible denial or ignore responses when interoperability needs them. |

## Pruning Ledger

Before CP17 can be marked verified, this ledger must contain every rejected
reference capability with evidence and a reason.

| Capability | Reference evidence | Decision | Reason | User-visible impact |
| --- | --- | --- | --- | --- |
| Go daemon runtime, HTTP router, WebSocket event hub, config daemon services | `goed2kd/internal/app/daemon.go`, `internal/rpc/http/*`, `internal/rpc/ws/*`, `internal/service/*` | `prune` | aria2-next already has a native CLI/RPC process. A sidecar or second API stack is explicitly out of scope. | Users use aria2-next RPC/CLI, not a goed2kd-compatible daemon API. |
| aMule GUI, webserver, TextClient, EC remote-control protocol, Android UI | aMule `src/webserver/**`, `TextClient.*`, `libs/ec/**`; jed2k `android/**` | `prune` | These are application shells and remote-control UIs. They do not belong in aria2-next core. | No aMule/jed2k UI compatibility. Protocol data needed by Motrix Next must be exposed through aria2-next RPC fields. |
| ED2K chat, IRC, captcha chat, shared-directory browsing UI, preview frames, collection UI | libed2k `peer_connection.cpp` handlers for message/captcha/preview/shared-directory/ismod; aMule chat and UI paths | `prune` | These features are obsolete or UI/social surfaces. They are not required for file download, search, source discovery, Kad, sharing, or upload queue operation. | aria2-next must not advertise preview/captcha/chat support. Unsupported UI requests may be rejected or ignored. File transfer interoperability remains the target. |
| Importing Boost/Asio/libed2k runtime architecture | libed2k `asio.cpp`, `io_service.hpp`, `session_impl.*`, Boost-heavy headers | `prune` | The goal requires native C++11 inside aria2-next's existing event loop. | No new event loop or Boost dependency. Equivalent protocol behavior is implemented in aria2-next commands/state. |
| Go/Java library runtime reuse | `goed2k-core`, `goed2kd`, `jed2k` source trees | `prune` | References are design inputs only. Runtime integration would violate the native C++11 requirement. | No Go or Java runtime dependency. Behavior must be reimplemented natively. |
| Independent UPnP/NAT-PMP stacks from references | `goed2k-core/upnp.go`, `internal/upnp/*`, jed2k `weupnp`, libed2k `upnp.cpp`, `natpmp.cpp` | `replace` | NAT behavior, if needed, should use aria2-next's existing networking/option architecture. | ED2K will not ship a separate NAT stack as part of this feature. |
| Deprecated server chat opcodes and Kad1 routing as active features | libed2k `packet_struct.hpp` deprecated chat/Kad1 opcode comments; aMule routing code ignores Kad1 contacts in `nodes.dat` | `prune` | References identify these as deprecated or ignored. Keeping them active would add risk without useful modern interoperability. | Old chat and Kad1-only nodes are not supported. Parsers should reject or ignore them safely. |

## Target Module Shape

The implementation should converge toward small native modules under `src/`
without creating a new maintained build system or event loop.

| Area | Suggested module ownership | Purpose |
| --- | --- | --- |
| Link and metadata | `ed2k_link.*` | File, server, serverlist, nodeslist links, safe names, part hashes, AICH hash, inline sources, source hash, crypt options. |
| Binary protocol | `ed2k_packet.*`, `ed2k_tag.*` | Packet framing, opcodes, tags, endian helpers, bounded parsing. |
| Hashing and integrity | `ed2k_hash.*`, `ed2k_aich.*` | MD4 part/root/final hashes, AICH root/tree/file-hash answers/recovery data. |
| Server support | `Ed2kServerSession.*`, `Ed2kServerState.*` | ED2K TCP login, IDChange, source requests, search, callback, status, server list, retry/backoff, persistence. |
| Peer support | `Ed2kPeerSession.*`, `Ed2kPeerState.*` | Incoming/outgoing peer TCP, hello, eMule info, capabilities, file status, hashset, parts, compression, SX, disconnects, scoring. |
| Scheduling | `Ed2kSourcePolicy.*`, `Ed2kPiecePicker.*` | Source priority, availability, queue state, endgame, duplicate block prevention, failed peer recovery. |
| Kad | `Ed2kKadRouting.*`, `Ed2kKadTraversal.*`, `Ed2kKadCommand.*` | UDP integration, routing table, bootstrap, traversal, source search, keyword search, publish, firewall checks, durable state. |
| Sharing and upload | `Ed2kSharedStore.*`, `Ed2kUploadQueue.*`, `Ed2kCredits.*` | Completed/shared file indexing, upload queue/rank/slots/limits/statistics, peer credits, upload responses. |
| RPC and CLI | existing RPC, prefs, manual, completion files | Stable Motrix Next fields, search results, status surfaces, documented options. |

This split is not a license for a sweeping rewrite. It is the target boundary.
Move code only when the next checkpoint needs the boundary.

## Checkpoint Status Legend

`done` means the code path exists and has local verification.

`partial` means useful code exists but the capability is incomplete or
unverified.

`todo` means the capability has not been implemented in a meaningful way.

`blocked` means work cannot proceed without a resolved contradiction or local
architecture decision.

`verified` means the checkpoint has passed its stated local verification.

## Checkpoint Matrix

| ID | Status | Checkpoint | Current evidence | Acceptance criteria | Verification |
| --- | --- | --- | --- | --- | --- |
| CP0 | partial | Baseline audit and tracking | This document records current diff, known failures, reference map, parity ledger, pruning policy, and target checkpoints. | This file is committed or kept updated with every checkpoint. Current red tests are either fixed or documented with root cause. | `git status --short`; targeted `ctest` result recorded in progress log. |
| CP1 | verified | Stabilize current ED2K tests | The two known ED2K failures were traced and fixed. Source Exchange now serializes unknown source servers as `0.0.0.0:0`; the UDP server status test waits for the command socket to become readable before executing the receive path. | Root cause is traced for Source Exchange answer serialization and UDP server status test scheduling. Fixes are minimal and do not hide failures. | `cmake --build --preset default --target aria2_tests`; `ctest --preset default --output-on-failure -R aria2_tests`. |
| CP2 | verified | Reference parity audit | The parity ledger now classifies the meaningful reference subsystems as port, adapt, replace, or prune. The pruning ledger records daemon/UI/runtime/deprecated surfaces that must not be ported. | Audit all reference areas in the parity ledger. Record a decision for each meaningful subsystem. Add pruned items only with evidence and reason. | Reference inspection with `rg`/`sed`; documentation review; `git diff --check docs/maintenance/ed2k-implementation-checkpoints.md`. |
| CP3 | partial | Protocol module boundaries | `ed2k_helper.*` contains link, packet, tag, hash, server, Kad, SX, compression, state, and AICH logic. | Split only the necessary helpers into focused modules with no behavior drift. CMake source inventory remains accurate. | ED2K helper tests pass before and after split. |
| CP4 | partial | Link support | File links, options, part hashes, AICH hash, source hash, crypt options, server links, serverlist links, and nodeslist links have parser coverage in draft helpers. | Parsing and serialization are complete for file/server/serverlist/nodeslist links, part hashes, AICH hashes, inline sources, source hashes, crypt options, safe output filenames, and all reference link variants that are not pruned. | Focused link parser tests plus `ProtocolDetectorTest`. |
| CP5 | partial | RequestGroup, disk, and resume spine | ED2K request groups use `DownloadContext`, `DefaultPieceStorage`, `SegmentMan`, disk adaptor, and control file paths. Session writes file link plus limited server/Kad state. | ED2K downloads and searches start through existing request group paths. Resume is restart-safe for file metadata, piece state, hashset state, AICH state, sources, server state, Kad state, shared files, and credits where implemented. | `SessionSerializerTest`; focused ED2K resume test; no regression in normal downloads. |
| CP6 | partial | Server TCP and UDP support | Login, IDChange, GetSources, FoundSources, callback request, server status, server message, server list, and UDP global status are partially present. | Direct servers and server.met load correctly. Server login, HighID/LowID, status/messages, GetSources/FoundSources, callback, server list, UDP global status, retry/backoff, and persisted server state are correct and scheduled. | Focused server packet tests and local command simulation. |
| CP7 | partial | Peer outbound download session | Outgoing peer TCP, hello, eMule info, file request/status, hashset, accept upload, queue rank, normal parts, compressed parts, and part hash verification are partially present. | Peer state machine handles 32-bit and 64-bit requests, normal and compressed parts, out-of-parts, cancel transfer, duplicate rejection, failure scoring, reconnect backoff, and corrupt piece retry. | Focused peer command tests with local socket pairs and disk-backed piece verification. |
| CP8 | todo | Incoming peer listener | No complete incoming TCP listener was found in the draft. | Incoming ED2K peer connections are accepted through aria2's event loop, matched to shared/completed files or active downloads, deduplicated, and routed into the peer state machine. | Local listener test with one incoming peer handshake. |
| CP9 | partial | Source Exchange SX1/SX2 | Request/answer helpers and command handlers exist. One SX2 test currently fails because the observed frame is `OP_EMULEINFO`. | SX1 and SX2, including SX2 v4 entries, merge sources into policy state with endpoint validation, deduplication, source labels, and backoff. | Source Exchange parser tests and peer command test scanning full packet sequence. |
| CP10 | todo | ED2K source and piece policy | The draft schedules peers with simple cursors and uses generic `SegmentMan`. | Source priority, server/Kad/SX/inline/resume sources, peer availability, queue state, endgame, duplicate block prevention, failed peer recovery, and safe retry are implemented without bypassing existing disk paths. | Scheduling invariant tests and local multi-peer transfer simulation. |
| CP11 | partial | Integrity and compressed transfer | MD4, root hash, compressed part inflate, and AICH packet parsing exist. AICH tree/recovery is not complete. | MD4 part/root/final verification, AICH root/tree/file-hash answers/recovery data, compressed part size/range validation, and corrupt piece retry are complete. | MD4/AICH/compression tests plus corrupt-piece retry test. |
| CP12 | partial | Kad bootstrap and routing | UDP socket, nodes.dat, bootstrap, hello, simple source/keyword search, and routing table snapshot exist. | Classic Kad supports nodes.dat loading, bootstrap, hello, routing table maintenance, traversal, source search, source publish, keyword search, firewalled checks, refresh, and durable state. | Kad packet tests, routing tests, traversal tests, and local UDP command simulation. |
| CP13 | todo | Sharing and shared store | No first-class shared store was found. Completed downloads are not yet modeled as shareable ED2K resources. | Completed downloads are shareable. Imported shared files are hashable and indexable. Shared metadata persists and can answer status, hashset, SX, AICH, and part requests. | Shared file indexing test and upload response tests. |
| CP14 | todo | Upload queue and credits | The draft has partial response helpers but no upload queue, slots, limits, statistics, or credits. | Upload queue, queue rank, slots, limits, upload statistics, and peer credit behavior integrate with existing transfer stats and limit model. | Upload queue policy tests and local peer request simulation. |
| CP15 | partial | Search through CLI/RPC | RPC methods exist for search and results. Server and Kad search are only partially implemented. | Server and Kad search return stable structured fields for hash, name, size, sources, complete sources, type, extension, media metadata, source network, and generated ED2K link. Results can create normal downloads. | RPC unit coverage or local JSON-RPC smoke test. |
| CP16 | todo | Status surfaces for Motrix Next | Search result fields exist. Full ED2K status fields do not. | ED2K download, server, peer, Kad, queue, search, share, and upload state are exposed through existing CLI/RPC paths where possible. ED2K-specific fields are stable and documented. | RPC status tests and Motrix integration note review. |
| CP17 | partial | Documentation and completions | Manual and completion entries exist, but manual overstates some draft capabilities. | CLI help, manual, RPC docs, completions, and Motrix Next notes match the actual implementation exactly. | Documentation review plus completion/help generation checks used by this repository. |
| CP18 | todo | Full local verification | Targeted `aria2_tests` now pass after CP1. Full project verification has not been rerun after this checkpoint. | `cmake --preset default`, `cmake --build --preset default`, `ctest --preset default`, and `build/default/aria2-next --version` all pass. Packaging shell syntax checks run if packaging or release-facing files change. | Required commands recorded in the final progress log. |

## Required Progress Log Format

After each checkpoint, append one compact entry under this section. Keep entries
factual. Do not repeat the entire matrix.

Use this format:

```text
YYYY-MM-DD CPX status
Changed: concise file and behavior summary.
Verified: exact command and result.
Remaining: next concrete gap.
Blocked: none, or exact blocker.
```

## Progress Log

2026-05-18 CP0 partial
Changed: Created the durable ED2K checkpoint record from the current
uncommitted diff, local test result, architecture audit, and reference map.
Verified: `ctest --preset default --output-on-failure -R aria2_tests` was run
and failed with two ED2K test failures.
Remaining: Fix CP1 before adding more protocol behavior.
Blocked: none.

2026-05-18 CP0 scope-update
Changed: Strengthened the scope from a practical downloader to complete native
ED2K/eMule parity against the local reference repositories. Added the
completeness policy, pruning ledger, and a formal reference parity ledger.
Verified: Documentation-only update. No build or test command was required for
this scope clarification.
Remaining: CP1 must still fix the current red tests, and CP2 must classify all
meaningful reference subsystems as port/adapt/replace/prune before broad
feature work continues.
Blocked: none.

2026-05-18 CP1 verified
Changed: Fixed the Source Exchange answer path so peers without a known source
server serialize the server field as `0.0.0.0:0` instead of failing packet
creation. Added a narrow `Ed2kKadCommand` local UDP readability probe and used
it in the existing UDP server status test so the test waits for the response
packet before executing the receive path.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: CP2 reference parity audit is the next checkpoint before broad
feature work.
Blocked: none.

2026-05-18 CP2 verified
Changed: Completed the local reference parity audit in this tracker. Classified
client/session lifecycle, server policy, transfer model, peer protocol, client
UDP reask/callback, Source Exchange, Kad, search, integrity, compression,
sharing, upload queue, persistence, RPC/CLI integration, NAT helpers, daemon
APIs, and UI/social/deprecated surfaces as port, adapt, replace, or prune.
Added a verification discipline section that limits future testing to compact
protocol, state, persistence, RPC, disk, and regression checks.
Verified: Local references were inspected with focused `rg` and `sed` commands.
`git diff --check docs/maintenance/ed2k-implementation-checkpoints.md` passed.
Remaining: CP3 should reduce `ed2k_helper.*` and command growth only where the
next feature checkpoint needs a cleaner module boundary.
Blocked: none.

2026-05-18 pruning-cleanup verified
Changed: Removed obsolete preview and captcha capability state from the eMule
misc option model and its helper test. Updated this tracker to require
aggressive cleanup of pruned compatibility surfaces and to avoid advertising
unsupported legacy UI/social capabilities.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`. `git diff --check
src/ed2k_helper.cc src/ed2k_helper.h tests/Ed2kHelperTest.cc
docs/maintenance/ed2k-implementation-checkpoints.md` passed.
Remaining: Continue CP3 with focused module boundaries and no broad test
expansion.
Blocked: none.

2026-05-18 CP3 partial
Changed: Split ED2K MD4, root hash, and AICH hash primitives from the oversized
`ed2k_helper.*` module into `ed2k_hash.*`. Kept packet, link, tag, SX, Kad,
compression, and AICH payload parsing in `ed2k_helper.*` for now to avoid
behavior drift. Added the new hash module to the CMake source inventory.
Verified: `git diff --check src/ed2k_helper.cc src/ed2k_helper.h
src/ed2k_hash.cc src/ed2k_hash.h cmake/Sources.cmake
docs/maintenance/ed2k-implementation-checkpoints.md` passed.
`cmake --build --preset default --target aria2_tests` passed. A sandboxed
`ctest --preset default --output-on-failure -R aria2_tests` failed because the
sandbox rejected local socket binds with `Operation not permitted`. The same
command passed outside the sandbox with `100% tests passed, 0 tests failed out
of 1`.
Remaining: Continue CP3 only where the next checkpoint needs cleaner link,
packet, Kad, or state boundaries. Do not add broad test scaffolding.
Blocked: none.

2026-05-18 pruning-cleanup verified
Changed: Removed the remaining comment and shared-file-view capability fields
from the eMule misc option model and helper test. These fields belong to
pruned social/UI surfaces and are no longer parsed into supported local state
or advertised by local peer info.
Verified: `rg -n "acceptCommentVersion|noViewSharedFiles|supportsPreview|supportsCaptcha|preview|captcha|acceptComment" src/ed2k_helper.* src/Ed2kCommand.* tests/Ed2kHelperTest.cc docs/maintenance/ed2k-implementation-checkpoints.md`
shows only pruning-policy documentation references. `git diff --check
src/ed2k_helper.cc src/ed2k_helper.h tests/Ed2kHelperTest.cc
docs/maintenance/ed2k-implementation-checkpoints.md` passed.
`cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP3 or CP4 with protocol-relevant module boundaries and
link behavior only.
Blocked: none.

2026-05-18 CP3 partial
Changed: Split ED2K URI link and text endpoint parsing/serialization from
`ed2k_helper.*` into `ed2k_link.*`. Kept binary endpoint packing, packet
helpers, server payloads, Kad payloads, Source Exchange, compression, and AICH
payloads in `ed2k_helper.*` to avoid a broad protocol rewrite.
Verified: `git diff --check cmake/Sources.cmake src/ed2k_helper.cc
src/ed2k_helper.h src/ed2k_link.cc src/ed2k_link.h
docs/maintenance/ed2k-implementation-checkpoints.md` passed.
`cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP3 only for module boundaries needed by the next protocol
checkpoint, likely packet/tag or server state, without adding duplicate tests.
Blocked: none.

2026-05-18 CP3/CP4 partial
Changed: Split ED2K endian helpers, packet framing, tag parsing, and tag
serialization from `ed2k_helper.*` into `ed2k_packet.*`. Tightened CP4 link
behavior by accepting browser-style `%7C` ED2K separators and routing default
ED2K output names through aria2-next's existing tainted-basename sanitizer.
Verified: `git diff --check cmake/Sources.cmake src/ed2k_helper.cc
src/ed2k_helper.h src/ed2k_packet.cc src/ed2k_packet.h src/ed2k_link.cc
src/download_helper.cc tests/Ed2kHelperTest.cc tests/DownloadHelperTest.cc
docs/maintenance/ed2k-implementation-checkpoints.md` passed.
`cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue reducing `ed2k_helper.*` by extracting server state,
server.met, search, Kad, Source Exchange, compression, and AICH payload
modules as their checkpoints require. CP4 still needs a final audit before it
can be marked verified.
Blocked: none.

2026-05-18 CP3 partial
Changed: Deleted the oversized `ed2k_helper.cc` implementation file and kept
`ed2k_helper.h` as a compatibility aggregation header for opcode constants and
focused ED2K protocol modules. Split binary endpoint helpers, server protocol
payloads and persisted server state, server and Kad search conversion, Kad
search/publish packets, peer file-status/hashset/part/Source Exchange/eMule
info packets, compressed part decoding, AICH packet payloads, and Kad
bootstrap/routing/nodes.dat handling into separate modules.
Verified: `cmake --build --preset default --target aria2_tests` passed after
the split. `ctest --preset default --output-on-failure -R aria2_tests` passed
with `100% tests passed, 0 tests failed out of 1`. `git diff --check` passed.
Remaining: CP3 still needs a later include-surface audit so callers can move
from the aggregation header to the narrow protocol headers where practical.
Avoid adding test volume for mechanical include cleanup.
Blocked: none.
