# Native ED2K/eMule Refactor Overview

This directory is the active tracker for the ED2K/eMule refactor in
aria2-next. It supersedes `docs/maintenance/ed2k` as the source of truth for
future ED2K work.

The old tracker records a completed draft implementation. It is useful
history, but it is no longer authoritative because the reference set changed
and live interoperability testing showed that public ED2K downloads can still
stall after server source discovery.

## Tracker Files

| Path | Role | Update rule |
| --- | --- | --- |
| `overview.md` | Refactor scope, current evidence, architecture rules, and verification policy | Update when durable scope, reference authority, or architectural direction changes |
| `reference-audit.csv` | Authoritative reference subsystem audit and port, adapt, replace, or prune decisions | Update before implementing or pruning each subsystem |
| `checkpoints.csv` | Main checkpoint matrix for the refactor | Update after every checkpoint with status, evidence, remaining work, and blockers |
| `progress.md` | Compact chronological evidence trail | Append one entry after each checkpoint, root-cause finding, or material cleanup |

Use `checkpoints.csv` as the primary progress source. Use
`reference-audit.csv` to prevent scope loss. Use `progress.md` to survive goal
context compaction without inflating the repository with scratch logs.

## Scope

The target is practical, native ED2K/eMule interoperability inside aria2-next.
The feature must support real file downloads through ED2K servers, peer
connections, source discovery, Source Exchange, Kad, integrity checks, resume,
sharing, upload behavior, search, CLI/RPC status, Motrix Next integration
fields, and documented persistence formats.

The implementation must stay in native C++11 and reuse the existing
aria2-next architecture: CMake, Command/EventPoll, DownloadEngine,
RequestGroup, DownloadContext, PieceStorage, DiskAdaptor, CLI, RPC, session,
and documentation patterns. Do not add Go, Java, sidecar daemons, Boost, Asio,
a second event loop, or unrelated architecture changes.

The authoritative local reference set is:

| Reference | Role |
| --- | --- |
| `/Users/sekiro/Projects/oss/ed2k-references/amule-official` | Modern open eMule-compatible client behavior, server/peer/Kad/sharing/upload flows |
| `/Users/sekiro/Projects/oss/ed2k-references/emule-official-0.50a` | Canonical eMule behavior, packet flow, file identifiers, crypt/secure-ident capability surfaces |
| `/Users/sekiro/Projects/oss/ed2k-references/mldonkey-official` | Independent ED2K implementation for server, source, file, share, and Kad cross-checks |
| `/Users/sekiro/Projects/oss/ed2k-references/wireshark-official` | Packet dissector authority for wire framing, opcodes, tags, and packet variants |
| `/Users/sekiro/Projects/oss/ed2k-references/protocol-docs` | Protocol text for ED2K, eMule extensions, Kad, links, corruption handling, HighID/LowID, and callback behavior |

Do not use deleted or disallowed references as implementation authority. Do
not copy reference code directly.

## Current Evidence

The existing draft can parse ED2K links, create tasks, connect to servers, get
LowID state, request sources, and receive source lists. Live public testing
with the Windows XP ED2K link still stalled at zero downloaded bytes after peer
connection attempts. That failure means the current code has not proven
peer-level interoperability.

The strongest current risk areas are protocol capability mismatch and missing
modern peer request paths. The code advertises eMule capabilities such as
multi-packet and Source Exchange 2 while not fully implementing all related
wire paths. The authoritative references show important behavior around
`OP_FOUNDSOURCES_OBFU`, `OP_GETSOURCES_OBFU`, `OP_MULTIPACKET`,
`OP_MULTIPACKET_EXT`, `OP_MULTIPACKET_EXT2`, file identifiers, secure
identification, crypt/obfuscation options, callback routing, Source Exchange
v4 crypt options, AICH, and Kad state.

The refactor must start by proving the failure boundary with focused logs and
packet-level evidence. Do not continue adding broad ED2K features until the
server-source-to-peer-request path is understood.

## Architecture Direction

Keep the existing aria2-next event loop. The refactor should split behavior by
ownership only where it improves correctness and maintainability.

| Area | Target ownership |
| --- | --- |
| Link and metadata | `ed2k_link.*` plus RequestGroup-owned ED2K metadata |
| Packet framing and tags | `ed2k_packet.*` and focused packet helpers with bounded parsing |
| Server session | Focused server state machine for login, IDChange, source requests, OBFU sources, callback, status, server list, retry, and persistence |
| Peer download session | Focused peer state machine for hello, eMule info, truthful capabilities, request flow, queue state, parts, compression, AICH, and failure handling |
| Peer upload and sharing | Shared-file responder, upload queue, credits, and incoming shared-peer routing |
| Source policy | ED2K-aware source selection, queue/backoff, availability, duplicate prevention, and resume source handling |
| Kad | UDP command integration plus routing, traversal, source search, keyword search, publish, firewalled checks, and durable state |
| CLI/RPC/docs | Existing aria2-next option, manual, completion, RPC, and Motrix Next documentation paths |

Large files such as `src/Ed2kCommand.cc` and `src/Ed2kKadCommand.cc` should be
reduced through functional ownership boundaries, not mechanical churn. Delete
duplicated draft code when evidence shows that it is the wrong shape.

## Verification Policy

Verification must stay meaningful and compact. Add tests only for parser
contracts, packet framing, protocol state transitions, persistence formats,
disk/integrity safety, RPC field contracts, and confirmed regressions. Avoid
broad scaffolding, fake tests, placeholder files, and tests that simply mirror
implementation details.

Batch related changes and verify once at the end of the batch. Run
intermediate tests only when a failing boundary is needed to prove a root cause
or when a risky edit needs immediate feedback. Routine checkpoints should use
the smallest commands that prove the changed surface. Final completion
requires:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

Final completion also requires a short live ED2K smoke check. The smoke check
must prove server discovery, peer interoperability, and nonzero data transfer
on a known valid fixture. If the public ED2K network prevents deterministic
verification, record the exact packet-level external blocker instead of
claiming completion.

## Update Rules

Every checkpoint update must record changed files, final verification evidence,
remaining work, blockers, and relevant reference-audit decisions. Keep
`progress.md` checkpoint-sized. Do not record every investigation step, every
ordinary local build, or every small test run. Scratch logs, packet captures,
generated reports, and local caches must stay outside the repository.
