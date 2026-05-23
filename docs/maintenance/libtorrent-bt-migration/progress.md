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

2026-05-23 BTM-005 verified
Changed: Added libtorrent status storage to `LibtorrentAttribute`, updated
`LibtorrentCommand` to refresh verified libtorrent total, completed, upload,
speed, connection, info-hash, bitfield, seeding, and metadata state, and made
`RequestGroup` and RPC progress read libtorrent status before native
`PieceStorage` for libtorrent-backed downloads.
Verified: `testLibtorrentVerifiedProgressOverridesPieceStorage` covers the BT
false-completion path where old PieceStorage is marked complete but libtorrent
reports incomplete. `testGatherProgressLibtorrentStatus` covers RPC status
fields sourced from libtorrent. `build/libtorrent-positive/aria2_tests` passed
with `OK (1140)`. `git diff --check CMakeLists.txt cmake src tests
docs/maintenance/libtorrent-bt-migration` passed. CSV parser check passed with
`CSV OK 13`.
Public smoke under `/Users/sekiro/Desktop/aria2-next-current` completed the
Debian netinst torrent, returned an empty active RPC result after completion,
and removed the `.aria2` control file.
Remaining: Continue BTM-006 resume data and native BT persistence cleanup.
Blocked: none.

2026-05-23 BTM-006 active
Changed: Added libtorrent resume data storage to `LibtorrentAttribute`, added a
libtorrent-specific `.aria2` control file that stores libtorrent fast-resume
data instead of native BT in-flight pieces, loads that data before adding the
torrent, and requests resume data on pause, shutdown, and completion while
waiting briefly for the async alert before removing the torrent.
Verified: `testLibtorrentResumeDataRoundTrip` covers the local resume data
round trip. Public Debian netinst smoke saved
`downloads/debian-netinst.torrent.aria2` after interrupt, loaded it on restart,
advanced from 304611328 bytes to 562036736 bytes, then completed 791674880
bytes and removed the control file. `cmake --build build/libtorrent-positive
--target aria2_tests -- -j2 && build/libtorrent-positive/aria2_tests` passed
with `OK (1141)`.
Remaining: Run magnet-metadata smoke when practical, then remove obsolete
native BT progress persistence in BTM-006.3 or BTM-008.
Blocked: none.

2026-05-23 BTM-007 active
Changed: Added GID-indexed libtorrent torrent-handle tracking, mapped active
per-download download limit, upload limit, and maximum peer changes to the
libtorrent handle, applied stored group options when the torrent is added, and
kept option-only updates from creating an unused libtorrent session.
Verified: `testLibtorrentSessionTracksActiveTorrent`,
`testChangeOption`, `All Tests/aria2::rpc::RpcMethodTest`, and
`All Tests/aria2::RequestGroupTest` passed. `cmake --build
build/libtorrent-positive --target aria2_tests -- -j2` succeeded.
`build/libtorrent-positive/aria2_tests` passed with `OK (1142)`.
`git diff --check CMakeLists.txt cmake src tests
docs/maintenance/libtorrent-bt-migration` passed. CSV parser check passed with
`CSV OK 13`.
Remaining: Map file priorities, pause/resume/remove details, and seed ratio or
seed time policy before closing BTM-007.
Blocked: none.

2026-05-23 BTM-007 active
Changed: Mapped `--select-file` and RPC `select-file` changes to libtorrent
file priorities. Torrent-file requests derive priorities before `add_torrent`,
runtime RPC changes update stored priorities, and metadata refresh applies
stored priorities once metadata exists.
Verified: `testCreateRequestGroupForUri_LibtorrentTorrentSelectFile`,
`testChangeOptionLibtorrentSelectFile`, `All Tests/aria2::DownloadHelperTest`,
`All Tests/aria2::rpc::RpcMethodTest`, and
`All Tests/aria2::RequestGroupTest` passed.
`build/libtorrent-positive/aria2_tests` passed with `OK (1144)`.
Remaining: Map pause/resume/remove details and seed ratio or seed time policy
before closing BTM-007.
Blocked: none.

2026-05-23 BTM-007 verified
Changed: Added libtorrent seed-policy mapping for `--seed-time` and `--seed-ratio`, kept selected-file completion separate from full-torrent seeding, routed `on-bt-download-complete` through the libtorrent command path once per download, and made `RequestGroup::allDownloadFinished()` use libtorrent seeding status for migrated BT downloads.
Verified: `testStopsWhenSeedTimeElapsed`, `testStopsWhenShareRatioReached`, `testKeepsSeedingWhenRatioDisabled`, and `testLibtorrentAllDownloadFinishedUsesSeedingStatus` cover the new behavior. `build/libtorrent-positive/aria2_tests` passed with `OK (1148)`. `git diff --check CMakeLists.txt cmake src tests docs/maintenance/libtorrent-bt-migration` passed. CSV parser check passed with `CSV OK 13`.
Remaining: Start BTM-008 native BitTorrent cleanup and obsolete test removal.
Blocked: none.

2026-05-23 BTM-008 verified
Changed: Removed the obsolete native BitTorrent engine source graph, including
native peer protocol, DHT, tracker, PEX, LPD, MSE, metadata, endgame, scheduler,
resume, and stale internal tests. Removed native-only CLI options from the
option parser, usage text, manual, API option lists, and regenerated bash
completion. Kept only shared parser and metadata helpers required by torrent,
magnet, Metalink, and libtorrent ingress. Added libtorrent-backed tracker,
DHT bootstrap, PEX, LPD, encryption, file-priority, limit, and seed-policy
option mapping where retained options still have real behavior.
Verified: `cmake --build build/libtorrent-positive --target aria2-next
aria2_tests -- -j2` succeeded. `build/libtorrent-positive/aria2_tests` passed
with `OK (819)`. Removed-option scan over help output, completion, source, and
manual returned no matches. Native BT residue scan over source, tests, and CMake
returned no matches. CSV parser check passed with `CSV OK 13`.
Remaining: Start BTM-009 non-BitTorrent 0B or 99 percent stall watchdog.
Blocked: none.

2026-05-23 BTM-009 verified
Changed: Added a focused native transfer progress watchdog to `DownloadCommand`
for HTTP, FTP, and SFTP download commands. The watchdog tracks payload progress
through `PeerStat` session bytes, resets on real payload advancement, and raises
the existing timeout retry path when socket activity continues without download
progress. This keeps the separate non-BitTorrent 0B or 99 percent stall path out
of libtorrent and avoids broad timeout behavior in `AbstractCommand`.
Verified: `build/libtorrent-positive/aria2_tests 'All
Tests/aria2::AbstractCommandTest'` passed with `OK (2)`.
`build/libtorrent-positive/aria2_tests 'All Tests/aria2::RequestGroupTest'`
passed with `OK (10)`. `build/libtorrent-positive/aria2_tests 'All
Tests/aria2::rpc::RpcMethodTest'` passed with `OK (46)`.
`build/libtorrent-positive/aria2_tests` passed with `OK (820)`.
Remaining: Start BTM-010 docs, completion, packaging, and release-path updates.
Blocked: none.

2026-05-23 BTM-010 verified
Changed: Updated the manual, technical notes, libaria2 notes, packaging docs,
release workflow, MinGW and Android release Dockerfiles, and release dependency
records for the libtorrent-only BitTorrent backend. Added Boost 1.91.0 and
libtorrent-rasterbar 2.0.12 to `packaging/dependencies.env`, added shared
release helpers for Boost headers and static libtorrent builds, and pinned
libtorrent's OpenSSL discovery to the release prefix to avoid host library
leakage.
Verified: `bash -n packaging/scripts/common.sh packaging/scripts/mingw-release
packaging/scripts/android-release scripts/release.sh scripts/bump-version.sh
tools/build_test.sh` passed. `.github/workflows/release.yml` and
`.github/workflows/ci.yml` parsed with Python YAML. `git diff --check
.github/workflows/release.yml packaging docs/manual
docs/maintenance/libtorrent-bt-migration` passed. CSV parser check over
`docs/maintenance/libtorrent-bt-migration/**/*.csv` passed. Local helper smoke
under `/Users/sekiro/Desktop/aria2-next-current` downloaded verified Boost and
libtorrent archives, built prefix OpenSSL 3.5.6, installed Boost headers, built
static libtorrent-rasterbar 2.0.12, and `pkg-config --libs --static
libtorrent-rasterbar` reported only the prefix library path plus framework and
OpenSSL link flags.
Remaining: Start BTM-011 final default build, CTest, version command, and
sequential public BitTorrent smoke validation.
Blocked: none.

2026-05-23 BTM-011 verified
Changed: Completed final local validation and sequential public BitTorrent
smoke validation for the libtorrent-only BitTorrent path.
Verified: `cmake --preset default`, `cmake --build --preset default`, `ctest
--preset default`, and `build/default/aria2-next --version` passed. The version
command reported Aria2 Next 2.1.4 with BitTorrent enabled. Sequential public
Debian 13.5.0 amd64 netinst torrent smoke under
`/Users/sekiro/Desktop/aria2-next-current/btm011-smoke-140641` completed with
aria2-next before the 180 second RPC sample, returned an empty active RPC
result, and produced `OK` console status with 6.9 MiB/s average speed. The
same torrent completed with Homebrew aria2c before the 90 second RPC sample and
with a minimal libtorrent-rasterbar 2.0.12 smoke client at 86 seconds. All
three completed ISO files had SHA-256
`95838884f5ea6c82421dfe6baaa5a639dbbe6756c1e380f9fe7a7cb0c1949d2a`.
Remaining: Run the final completion audit against the original goal contract.
Blocked: none.

2026-05-23 BTM-011 verified
Changed: Added maintainer-selected KNOPPIX 9.1 DVD torrent smoke evidence to
the final validation checkpoint.
Verified: Sequential 90 second smoke under
`/Users/sekiro/Desktop/aria2-next-current/knoppix-90s-smoke-143139` used
`/Users/sekiro/Desktop/Test Torrents/KNOPPIX_V9.1DVD-2021-01-25-DE.torrent`.
At 90 seconds, aria2-next reported 9,531,780 B/s and 869,269,504 bytes
complete; reference aria2c reported 11,705,750 B/s and 1,014,476,647 bytes
complete. Both remained active as expected for this larger torrent.
Remaining: Run the final completion audit against the original goal contract.
Blocked: none.

2026-05-23 BTM-008 verified
Changed: Removed orphaned native tracker and extension-message helper files
that remained outside the active build, and renamed the small shared constants
surface to `TorrentMetadataConstants`.
Verified: `cmake --build --preset default` succeeded. `ctest --preset default`
passed. Stale orphan scan for AnnounceList, AnnounceTier, UDPTrackerRequest,
BtConstants, LPD multicast constants, native peer constants, and
extension_message_test_helper returned no active runtime hits.
Remaining: Run the final completion audit against the original goal contract.
Blocked: none.
