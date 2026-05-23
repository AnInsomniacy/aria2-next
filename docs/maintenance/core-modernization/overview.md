# Core Modernization

This tracker owns the post-ED2K and post-libtorrent modernization of
aria2-next. The goal is to replace old native protocol and runtime code with
mature maintained libraries where those libraries clearly own the domain
better than aria2-next. The migration is intentionally not a compatibility
preservation project. Obsolete native engines, backend switches, options, docs,
and tests must be removed once their responsibility has moved or the feature is
pruned.

## Tracker Files

| Path | Role |
| --- | --- |
| `overview.md` | Scope, architecture rules, library policy, cleanup policy, verification policy, and update rules |
| `roadmap.csv` | Single checkpoint index and current progress entry point |
| `capability-ledger.csv` | Capability-by-capability migration, retention, replacement, and removal decisions |
| `dependency-ledger.csv` | Dependency-by-dependency ownership, packaging, and removal decisions |
| `progress.md` | Compact chronological evidence trail |
| `checkpoints/CM-001-foundation.csv` | Tracker activation, source inventory, current dependency facts, and final goal contract |
| `checkpoints/CM-002-dependency-policy.csv` | Target dependency decisions, removed dependency decisions, and packaging baseline policy |
| `checkpoints/CM-003-build-baseline.csv` | C++ standard, CMake detection, generated config, source inventory, and dependency gates |
| `checkpoints/CM-004-runtime-foundation.csv` | Boost.Asio runtime ownership, timers, wakeups, signals, and shutdown boundaries |
| `checkpoints/CM-005-libcurl-transfer-foundation.csv` | libcurl multi-socket foundation, transfer events, range writes, and status mapping |
| `checkpoints/CM-006-http-feature-parity.csv` | HTTP and HTTPS migration, redirects, headers, auth, proxying, resume, decoding, and native HTTP removal |
| `checkpoints/CM-007-ftp-sftp-migration.csv` | FTP, FTPS, SFTP, and SCP migration to libcurl and native FTP/SFTP removal |
| `checkpoints/CM-008-cookie-auth-proxy-cleanup.csv` | Cookie, netrc, authentication, proxy, browser-cookie, and legacy parser cleanup |
| `checkpoints/CM-009-rpc-beast-server.csv` | Boost.Beast JSON-RPC HTTP server migration and native HTTP server removal |
| `checkpoints/CM-010-websocket-beast.csv` | Boost.Beast WebSocket migration and wslay/native WebSocket removal |
| `checkpoints/CM-011-json-boost.csv` | Boost.JSON migration and custom JSON parser/writer removal |
| `checkpoints/CM-012-xml-metalink.csv` | XML-RPC removal, Metalink decision, and pugixml-backed Metalink4 path if retained |
| `checkpoints/CM-013-crypto-tls-cleanup.csv` | OpenSSL-only TLS and crypto ownership, stale backend removal, and ED2K hash boundary review |
| `checkpoints/CM-014-storage-engine.csv` | Modern storage boundary, verified progress, resume, flush, hash, and 99 percent completion truth |
| `checkpoints/CM-015-ed2k-runtime-bridge.csv` | ED2K bridge onto the modern runtime, storage, crypto, and compression boundaries |
| `checkpoints/CM-016-libtorrent-boundary-review.csv` | BitTorrent ingress residue review after libtorrent migration |
| `checkpoints/CM-017-option-doc-pruning.csv` | Public option, help, manual, completion, README, and stale compatibility claim pruning |
| `checkpoints/CM-018-packaging-release-closure.csv` | Packaging dependency closure, release scripts, CI, static linkage, and runtime dependency checks |
| `checkpoints/CM-019-final-validation.csv` | Final local verification, stale scans, legal smoke tests, and tracker closure |

Read `overview.md` and `roadmap.csv` first. During implementation, read only
the active checkpoint file plus the ledgers needed for a current decision. Read
the full tracker only for final review or when a blocker crosses checkpoint
boundaries.

## Goal Contract

Modernize aria2-next's remaining old native protocol and runtime layers after
the ED2K hardening and libtorrent BitTorrent replacement work. Use mature
maintained libraries for protocol and runtime domains where a suitable library
exists. Remove obsolete native implementations and stale tests instead of
keeping hidden fallback paths. Preserve the supported aria2-next product shape:
the `aria2-next` executable, supported CLI workflow, JSON-RPC for supported
methods, session save/load for supported downloads, Motrix Next integration,
ED2K, BitTorrent through libtorrent-rasterbar, ordinary URL downloads through
libcurl, and maintained release artifacts.

Stop only when every checkpoint in `roadmap.csv` is verified, the ledgers
record the final ownership decisions, local build and test gates pass, stale
code and stale option scans pass, release packaging checks pass, and legal
smoke evidence for ordinary URL downloads, BitTorrent, and ED2K is recorded.

## Commit Policy

Work on the current branch only. Do not push. Commit only at meaningful,
verified boundaries where a snapshot materially helps review or rollback, such
as a completed tracker checkpoint, a build-system migration boundary, a large
native subsystem removal, or a validated protocol/runtime replacement. Do not
commit after routine edits, partial refactors, formatting-only cleanup, or
unverified work.

## Library Policy

The policy is library-first, not library-blind. A mature library should replace
native code when it owns protocol correctness, security maintenance,
cross-platform behavior, and performance better than local code can. Native
code should remain only where aria2-next owns a real product differentiator or
where no suitable maintained embeddable library exists.

| Domain | Target owner | Policy |
| --- | --- | --- |
| HTTP, HTTPS, FTP, FTPS, SFTP, SCP | libcurl | Replace native protocol stacks and use libcurl multi-socket integration |
| BitTorrent and magnets | libtorrent-rasterbar | Keep libtorrent as the only BitTorrent backend |
| ED2K/eMule | Native aria2-next protocol modules | Retain protocol logic because no suitable maintained embeddable library exists |
| Event loop and async runtime | Boost.Asio | Use one application runtime and remove parallel native poller ownership after migration |
| JSON-RPC HTTP and WebSocket transport | Boost.Beast | Replace native HTTP server and wslay/native WebSocket code |
| JSON parsing and serialization | Boost.JSON | Replace custom JSON parser and writer code |
| Metalink XML if retained | pugixml | Retain only a small Metalink4 path if product value remains |
| TLS and general crypto | OpenSSL | Use one backend and remove GnuTLS, nettle, libgcrypt, GMP, WinTLS, and internal fallback ownership where obsolete |
| Compression | zlib through library-owned paths | Keep only required compression glue; remove duplicate local protocol decoding where libcurl owns it |
| Storage and verified progress | aria2-next | Keep and modernize because multi-source segmentation, control files, resume, and integrity are core product behavior |

## Scope

In scope:

| Area | Expected outcome |
| --- | --- |
| Build and dependencies | CMake and release packaging detect and require the target dependency set with clear failure messages |
| Runtime | A single Boost.Asio-owned runtime drives modern library integration, timers, wakeups, signals, and shutdown |
| Ordinary URL downloads | libcurl owns HTTP(S), FTP(S), SFTP, SCP, proxying, auth, cookies, redirects, resume, and decoding |
| RPC | Boost.Beast and Boost.JSON own JSON-RPC HTTP and WebSocket transport and JSON parsing |
| Storage | Completion truth, resume, control-file cleanup, and 99 percent behavior use verified persisted state |
| ED2K | ED2K protocol behavior remains native but moves off stale shared infrastructure where practical |
| BitTorrent | libtorrent boundary stays clean and native torrent residue is removed where no longer needed |
| Cleanup | Obsolete options, docs, tests, source files, build switches, and third-party code are removed |
| Packaging | Release artifacts include the modern dependency closure and reject host leakage |

Out of scope:

| Area | Reason |
| --- | --- |
| Reintroducing native BitTorrent | Already replaced by libtorrent and not allowed as a fallback |
| Porting ED2K to an unrelated client core | No mature embeddable library was identified, and current ED2K ownership is already local |
| GUI, daemon, or sidecar redesign | aria2-next remains one CLI/RPC engine process |
| Broad public-network benchmark claims | Public network smoke is evidence, not a deterministic performance gate |
| Compatibility aliases for removed legacy features | This migration intentionally prunes obsolete behavior |

## Removal Policy

Remove a native subsystem when its target owner is verified or when the feature
is pruned. Do not leave inactive compatibility modes, hidden fallback engines,
or build switches that still imply support for removed behavior.

Expected removal candidates include native HTTP client framing, native FTP
state machines, direct libssh2 SFTP commands, native proxy handshake code,
native cookie storage and browser cookie import, custom netrc parsing where
libcurl owns it, native DNS backends, native TLS backends outside OpenSSL,
GnuTLS, nettle, libgcrypt, GMP, WinTLS, custom JSON parsing, XML-RPC, wslay,
native WebSocket framing, Expat/libxml2 dual XML ownership, Metalink3 if
Metalink stays, old platform fallback source, stale CMake feature probes,
stale manual sections, stale completion entries, and tests for removed
internals.

Keep small native helpers only when they protect supported behavior and do not
duplicate a library responsibility.

## 99 Percent Policy

Completion must be based on verified transfer, disk flush, and integrity state.
User-facing progress may include received or buffered bytes when useful, but
final state, `.aria2` control-file cleanup, session save/load decisions, and
RPC completed status must never depend on unverified in-flight bytes.

BitTorrent already uses libtorrent verified state. Ordinary URL downloads and
ED2K must use the same truth boundary after storage modernization.

## Test Policy

Tests must be few and high value. Add or retain tests for routing boundaries,
dependency gates, request creation, completion truth, verified progress, resume
and control-file behavior, storage correctness, RPC contract behavior, option
pruning, stale-code scans, and confirmed regressions.

Delete tests that only cover removed native protocol internals, incidental
logs, old backend selection, or compatibility behavior that no longer exists.
Do not add broad fake socket integration scaffolding when a focused boundary
test proves the behavior.

## Verification Policy

Use focused verification during checkpoints. Final verification requires:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2-next --version
```

For dependency, CMake, packaging, or release-path changes, also run the relevant
syntax and closure checks:

```bash
bash -n tools/build_test.sh
bash -n scripts/bump-version.sh
bash -n scripts/release.sh
bash -n packaging/scripts/common.sh
bash -n packaging/scripts/mingw-release
bash -n packaging/scripts/android-release
```

Use `/Users/sekiro/Desktop/aria2-next-current` for temporary smoke downloads,
logs, and comparison artifacts. Do not commit those artifacts.

Use these maintainer-selected smoke fixtures for final validation:

| Surface | Fixture |
| --- | --- |
| HTTP/HTTPS | `https://secure-appldnld.apple.com/iPhone/061-7481.20100202.4orot/iPhone1,1_3.1.3_7E18_Restore.ipsw` |
| BitTorrent magnet | `magnet:?xt=urn:btih:c03bb709bd7efe79688775c4fc925141e41db287&dn=KNOPPIX_V9.1DVD-2021-01-25-EN` |
| BitTorrent torrent file | `/Users/sekiro/Desktop/Test Torrents/KNOPPIX_V9.1CD-2021-01-25-EN.torrent` |
| ED2K | `ed2k://|file|eMule0.50a-Installer.exe|3389035|3D366ED505B977FC61C9A6EE01E96329|h=EKE4PSKRQ65MWEPFTRDSAHW5VMDIMFAJ|/` |

## Update Rules

After each checkpoint, update `roadmap.csv`, the matching checkpoint file,
`capability-ledger.csv` when ownership changes, `dependency-ledger.csv` when a
dependency decision changes, and `progress.md` with compact evidence.

Keep entries checkpoint-sized and durable. Do not commit raw logs, packet
captures, generated reports, local caches, public-network scratch data,
temporary API payloads, or conversation text.
