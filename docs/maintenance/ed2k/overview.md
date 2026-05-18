# Native ED2K/eMule Implementation Overview

This directory is the persistent tracker for native ED2K/eMule support in
aria2-next. It exists to survive long goal runs and context compaction. Future
agents must read this file, `checkpoints.csv`, `reference-parity.csv`, and
`progress.md` before continuing ED2K work.

## Tracker Files

| Path | Role | Update rule |
| --- | --- | --- |
| `overview.md` | Scope, current architecture reading, verification discipline, and module boundaries | Update only when the durable engineering policy or current snapshot changes |
| `checkpoints.csv` | Checkpoint status matrix and acceptance criteria | Update the relevant row after each checkpoint |
| `reference-parity.csv` | Reference subsystem decisions, pruning decisions, and remaining parity work | Update before or during the checkpoint that implements, adapts, replaces, or prunes the behavior |
| `progress.md` | Chronological progress evidence | Append one compact entry after each checkpoint or material cleanup |

Use `checkpoints.csv` as the main progress source. Use `progress.md` as the
evidence trail. Use `reference-parity.csv` to prevent scope loss or accidental
porting of obsolete surfaces.

## Scope Contract

The target is a complete native ED2K/eMule implementation for aria2-next, not a
minimal downloader, stub, or cosmetic compatibility layer. Complete means every
protocol, behavior, persistence, status, and integration capability needed for
real ED2K/eMule client operation is implemented or explicitly pruned with
recorded evidence.

The implementation stays inside the existing C++11 codebase and integrates with
the current CMake, Command/EventPoll, DownloadEngine, RequestGroup,
DownloadContext, PieceStorage, DiskAdaptor, CLI, RPC, session, and
documentation patterns. It must not introduce a Go runtime, Java runtime,
sidecar daemon, Boost, Asio, a second event loop, or unrelated architecture
changes.

The local reference repositories under
`/Users/sekiro/Projects/oss/ed2k-references` are protocol and design references
only. The relevant reference set is `goed2k-core`, `goed2kd`, `amule`, `jed2k`,
and `libed2k-qmule`. Do not copy their code directly.

## Current Snapshot

Snapshot date: 2026-05-18.

Repository: `/Users/sekiro/Projects/personal/aria2-next`.

Current branch: `main`.

Current HEAD observed during the tracker split: `d2fc1f53`.

Operational status: ED2K/eMule support is still incomplete. CP0 through CP14
are verified. CP15 through CP18 remain partial or open. The current
implementation is roughly 79 percent of the complete target and must not be
presented as full ED2K/eMule support.

The draft has been committed through the first protocol module split.
`src/ed2k_helper.cc` has been deleted. `src/ed2k_helper.h` remains only as an
aggregation header for broad helper tests. Protocol constants live in
`src/ed2k_constants.h`, and production callers use focused ED2K protocol
headers where practical.

Remaining large draft surfaces are the active state-machine files, especially
`src/Ed2kCommand.cc` and `src/Ed2kKadCommand.cc`. Reduce them only when a
checkpoint needs a cleaner server, peer, Kad, scheduling, sharing, or upload
boundary.

## Current Capability Inventory

The current draft has useful foundations. ED2K file links are routed through
`ProtocolDetector` and `download_helper`. ED2K download request groups are
created with `DownloadContext`, `DefaultPieceStorage`, `SegmentMan`, and the
existing disk path. Server endpoints can come from `--ed2k-server` and
`server.met`. Kad bootstrap nodes can come from `nodes.dat`. Search request
groups exist through RPC methods `aria2.ed2kSearch` and
`aria2.getEd2kSearchResults`. Save-session writes the ED2K file link with
learned sources and integrity metadata plus hidden server and Kad routing state
options.

Server TCP login, IDChange, GetSources, FoundSources, callback request
handling, server messages, server lists, and UDP global status parsing are
partially present. Peer TCP handshake, eMule extended info, file request, file
status, hashset request, Source Exchange request and answer handling, normal
part transfer, compressed part transfer, MD4 part verification, and AICH packet
parsing are present. Source Exchange merge now preserves SX2 v4 user hash and
crypt options, source labels, deduplication, endpoint filtering, and existing
peer backoff state.

Incoming peer listening exists for the active-download path. The listener uses
the existing event loop, accepts TCP peers, routes them only when there is a
unique active ED2K download group, rejects duplicate active endpoints, and
normalizes incoming Hello endpoints to the peer's advertised ED2K listen port.
Shared/completed-file matching is handled by the native shared store and shared
peer command. Upload behavior now has a native upload queue with slot limits,
queue ranks, protocol-visible accept/ranking responses, transfer-stat upload
accounting, peer credit counters, and hidden session persistence. Sharing and
imported shared files are first-class persistent state. Kad has bootstrap,
hello, traversal, search, publish, firewalled checks, routing refresh, and
durable operational state. AICH has part-aware root hashing, packet parsing,
file-hash answer handling, recovery-data verification, corrupt-piece recovery
preservation for active downloads, and shared-file serving. Resume persists
active download metadata, learned sources, hashsets, server state, Kad routing
state, shared-file state, and peer credit state. CLI/RPC status does not yet
expose the full server, peer, Kad, queue, search, share, and upload model.

Release-facing documentation currently risks overstating draft support. Before
any release-facing checkpoint is marked verified, CLI help, manual text, RPC
documentation, completion data, and Motrix Next integration notes must match
the actual implementation exactly.

## Architecture Reading

`src/RequestGroup.cc` is the right integration spine. ED2K should continue to
use the existing request group lifecycle, duplicate-file guard, disk adaptor
opening, piece storage initialization, control file loading, and command
scheduling.

`src/Ed2kAttribute.*` is currently the shared RequestGroup-owned coordination
object. It stores link metadata, servers, server states, peers, piece hashes,
AICH root hash, search state, Kad routing table, Kad transactions, and
scheduling cursors. Keep it as the request-level coordinator, but move
subsystem behavior into smaller owned components as checkpoints need those
boundaries.

`src/Ed2kCommand.*` is overloaded. One command currently handles server TCP,
peer TCP, outbound connection setup, handshake, source discovery, download
requests, part writes, verification, Source Exchange, AICH metadata, and
partial upload responses. The final shape should keep aria2-next's single event
loop while separating server sessions, peer download sessions, and peer upload
sessions into focused state machines.

`src/Ed2kKadCommand.*` has useful UDP socket integration. It binds an IPv4 UDP
socket, uses aria2-next's read-check path, sends bootstrap, search, and status
packets, and receives responses. The missing part is a real Kad traversal
controller on top of that command.

## Target Module Shape

| Area | Suggested module ownership | Purpose |
| --- | --- | --- |
| Link and metadata | `ed2k_link.*` | File, server, serverlist, nodeslist links, safe names, part hashes, AICH hash, inline sources, source hash, crypt options |
| Binary protocol | `ed2k_packet.*`, `ed2k_tag.*` | Packet framing, opcodes, tags, endian helpers, bounded parsing |
| Hashing and integrity | `ed2k_hash.*`, `ed2k_aich.*` | MD4 part/root/final hashes, AICH root/tree/file-hash answers/recovery data |
| Server support | `Ed2kServerSession.*`, `Ed2kServerState.*` | ED2K TCP login, IDChange, source requests, search, callback, status, server list, retry/backoff, persistence |
| Peer support | `Ed2kPeerSession.*`, `Ed2kPeerState.*` | Incoming/outgoing peer TCP, hello, eMule info, capabilities, file status, hashset, parts, compression, Source Exchange, disconnects, scoring |
| Scheduling | `Ed2kSourcePolicy.*`, `Ed2kPiecePicker.*` | Source priority, availability, queue state, endgame, duplicate block prevention, failed peer recovery |
| Kad | `Ed2kKadRouting.*`, `Ed2kKadTraversal.*`, `Ed2kKadCommand.*` | UDP integration, routing table, bootstrap, traversal, source search, keyword search, publish, firewall checks, durable state |
| Sharing and upload | `Ed2kSharedStore.*`, `Ed2kUploadQueue.*`, `Ed2kCredits.*` | Completed/shared file indexing, upload queue/rank/slots/limits/statistics, peer credits, upload responses |
| RPC and CLI | Existing RPC, prefs, manual, completion files | Stable Motrix Next fields, search results, status surfaces, documented options |

This split is a target boundary, not permission for mechanical churn. Move code
only when the next checkpoint needs the boundary.

## Verification Discipline

Keep verification meaningful and narrow. Add tests only when they prove a
protocol boundary, parser invariant, state-machine transition, persistence
contract, disk/write integrity rule, RPC field contract, or fixed regression
root cause.

Avoid broad scaffolding, placeholder tests, generated-looking matrix tests,
tests that mirror implementation details, and tests that reimplement a
reference project in C++ assertions. Prefer one compact parser or state test
and one local command simulation over many near-duplicate cases. When behavior
is already covered by an existing aria2-next path, reuse that path and add only
the ED2K-specific assertion.

Checkpoint verification should use the smallest command set that proves the
changed surface. Full `cmake --preset default`, `cmake --build --preset
default`, `ctest --preset default`, and `build/default/aria2-next --version`
remain the final CP18 gate, not the routine command after every small edit.

## Update Rules

After each checkpoint, update `checkpoints.csv` and append one compact entry to
`progress.md`. If reference behavior is implemented, adapted, replaced, or
pruned, update `reference-parity.csv` in the same change.

A checkpoint is not verified until the relevant code path exists, the documented
acceptance criteria are met, and local verification evidence is recorded.

The tracker must not contain temporary scratch data, raw API payloads, copied
reference code, unsupported claims, or conversation-only context.
