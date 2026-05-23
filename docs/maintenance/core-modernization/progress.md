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
