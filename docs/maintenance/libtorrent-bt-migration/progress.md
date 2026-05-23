# Libtorrent BitTorrent Replacement Progress

This file is the compact chronological evidence trail for the libtorrent
BitTorrent replacement. Keep entries checkpoint-sized. Do not record raw logs,
packet captures, generated reports, local public-network data, or conversation
text.

Use this format:

```text
YYYY-MM-DD BTM-XXX status
Changed: concise tracker, code, or behavior summary.
Verified: exact final command and result, or documentation-only reason.
Remaining: next concrete gap.
Blocked: none, or exact blocker.
```

## Log

2026-05-23 BTM-001 active
Changed: Created the libtorrent BitTorrent replacement tracker with continuous
checkpoint numbering, a no-fallback migration contract, capability decisions,
focused test policy, native BitTorrent cleanup rules, and a separate
non-BitTorrent stall-fix checkpoint.
Verified: Documentation setup pending local format checks.
Remaining: Finish BTM-001 documentation verification, then start BTM-002
dependency and build integration.
Blocked: none.

2026-05-23 BTM-002 verified
Changed: Added a CMake dependency gate for libtorrent-rasterbar when
`ARIA2_ENABLE_BITTORRENT=ON`, exported `HAVE_LIBTORRENT_RASTERBAR`, linked
`aria2_core` to the pkg-config target and Boost headers, raised the project
C++ boundary to C++14, and added the dependency to the feature summary. This
moves the build boundary away from the old native BitTorrent engine by making
BitTorrent support require libtorrent-rasterbar.
Verified: Before the change, `cmake -S . -B build/libtorrent-gate-red -G Ninja
-DARIA2_ENABLE_BITTORRENT=ON` configured successfully without
libtorrent-rasterbar. After the change, `cmake -S . -B
build/libtorrent-gate-green -G Ninja -DARIA2_ENABLE_BITTORRENT=ON` failed with
`BitTorrent support now requires libtorrent-rasterbar`. `cmake -S . -B
build/no-bt-gate -G Ninja -DARIA2_ENABLE_BITTORRENT=OFF` succeeded and reported
`BitTorrent=; libtorrent-rasterbar=`. With libtorrent-rasterbar 2.0.12 and
Boost installed, `cmake -S . -B build/libtorrent-positive -G Ninja
-DARIA2_ENABLE_BITTORRENT=ON` configured with `libtorrent-rasterbar=1`, and
`cmake --build build/libtorrent-positive --target aria2_tests -- -j2` linked
the test executable. Direct header probes showed C++11 fails and C++14
compiles when Boost headers are available.
Remaining: Release dependency closure remains for BTM-010.
Blocked: none.

2026-05-23 BTM-003 verified
Changed: Added `LibtorrentAttribute` as the new BitTorrent request boundary,
added `CTX_ATTR_LIBTORRENT`, routed torrent files, torrent data, magnet links,
CLI/RPC addTorrent, and URI torrent detection into libtorrent-backed request
groups, stopped attaching the old `CTX_ATTR_BT` at ingress, and removed stale
native metadata post-handler tests from the active test target.
Verified: `build/libtorrent-positive/aria2_tests` passed with `OK (1136)`.
Remaining: Implement BTM-004 libtorrent session ownership, alert polling,
torrent handle lifecycle, and engine wakeups.
Blocked: none.

2026-05-23 BTM-004 verified
Changed: Added `LibtorrentSession` as the DownloadEngine-owned session holder,
added `LibtorrentCommand` for add_torrent, alert polling, metadata shape
refresh, progress updates, and lifecycle shutdown, and routed
`CTX_ATTR_LIBTORRENT` request groups to the libtorrent command path before the
old native `CTX_ATTR_BT` branch.
Verified: `cmake --build build/libtorrent-positive --target aria2_tests -- -j2`
succeeded. `build/libtorrent-positive/aria2_tests` passed with `OK (1138)`.
`git diff --check CMakeLists.txt cmake src tests
docs/maintenance/libtorrent-bt-migration` passed. CSV parser check over
`docs/maintenance/libtorrent-bt-migration/**/*.csv` passed with `CSV OK 13`.
Remaining: Start BTM-005 status, RPC, console, verified progress, and
false-completion cleanup.
Blocked: none.
