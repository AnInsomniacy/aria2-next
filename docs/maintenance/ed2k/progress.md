# Native ED2K/eMule Progress Log

This file is the chronological evidence trail for ED2K/eMule implementation
work. Append one compact entry after each checkpoint or material cleanup.

Use this format:

```text
YYYY-MM-DD CPX status
Changed: concise file and behavior summary.
Verified: exact command and result.
Remaining: next concrete gap.
Blocked: none, or exact blocker.
```

## Log

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
`git diff --check docs/maintenance/ed2k/overview.md` passed.
Remaining: CP3 should reduce `ed2k_helper.*` and command growth only where the
next feature checkpoint needs a cleaner module boundary.
Blocked: none.

2026-05-18 pruning-cleanup verified
Changed: Removed obsolete preview and captcha capability state from the eMule
misc option model and its helper test. Updated the tracker to require
aggressive cleanup of pruned compatibility surfaces and to avoid advertising
unsupported legacy UI/social capabilities.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`. `git diff --check
src/ed2k_helper.cc src/ed2k_helper.h tests/Ed2kHelperTest.cc
docs/maintenance/ed2k/overview.md` passed.
Remaining: Continue CP3 with focused module boundaries and no broad test
expansion.
Blocked: none.

2026-05-18 CP3 partial
Changed: Split ED2K MD4, root hash, and AICH hash primitives from the oversized
`ed2k_helper.*` module into `ed2k_hash.*`. Kept packet, link, tag, Source
Exchange, Kad, compression, and AICH payload parsing in `ed2k_helper.*` for now
to avoid behavior drift. Added the new hash module to the CMake source
inventory.
Verified: `git diff --check src/ed2k_helper.cc src/ed2k_helper.h
src/ed2k_hash.cc src/ed2k_hash.h cmake/Sources.cmake
docs/maintenance/ed2k/overview.md` passed.
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
from the eMule misc option model and helper test. These fields belong to pruned
social/UI surfaces and are no longer parsed into supported local state or
advertised by local peer info.
Verified: `rg -n "acceptCommentVersion|noViewSharedFiles|supportsPreview|supportsCaptcha|preview|captcha|acceptComment" src/ed2k_helper.* src/Ed2kCommand.* tests/Ed2kHelperTest.cc docs/maintenance/ed2k/overview.md`
shows only pruning-policy documentation references. `git diff --check
src/ed2k_helper.cc src/ed2k_helper.h tests/Ed2kHelperTest.cc
docs/maintenance/ed2k/overview.md` passed.
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
docs/maintenance/ed2k/overview.md` passed.
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
docs/maintenance/ed2k/overview.md` passed.
`cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue reducing `ed2k_helper.*` by extracting server state,
server.met, search, Kad, Source Exchange, compression, and AICH payload modules
as their checkpoints require. CP4 still needs a final audit before it can be
marked verified.
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

2026-05-18 CP3 verified
Changed: Moved ED2K protocol constants into `ed2k_constants.h`, reduced
`ed2k_helper.h` to a pure aggregation header, and moved production callers to
narrow ED2K headers for link, hash, packet, server, Kad, search, peer,
compression, and AICH surfaces. Kept only `tests/Ed2kHelperTest.cc` on the
aggregation header because it intentionally covers the broad helper surface.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`. `git diff --check` passed.
Remaining: CP4 should finish the ED2K link support audit and only add compact
parser coverage for real missing link variants.
Blocked: none.

2026-05-18 CP4 verified
Changed: Audited ED2K link behavior against the local goed2k-core, amule,
jed2k, and libed2k-qmule references. Confirmed support for file, server,
serverlist, nodeslist, percent-encoded separators, UTF-8 names, part hashes,
AICH master hashes, inline sources, source crypt options, and source hashes.
Tightened file links to reject zero-length files and invalid AICH base32 data
before they can create a download context or persisted session link.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`. `git diff --check` passed.
Remaining: CP5 should harden RequestGroup, disk, and resume state around the
existing ED2K link metadata without adding broad test scaffolding.
Blocked: none.

2026-05-18 tracker-split verified
Changed: Reorganized maintenance records so the root `docs/maintenance`
directory contains only the index, completed CMake and upstream issue-review
records live in task directories, and active ED2K tracking is split into
overview, checkpoint matrix, reference parity ledger, and progress log files.
Verified: Documentation-only restructuring. `git diff --check` passed.
Remaining: Continue CP5 using `docs/maintenance/ed2k/checkpoints.csv` as the
main progress source.
Blocked: none.

2026-05-18 CP5 partial
Changed: Fixed the ED2K session restore path for multiple persisted server
states. `ed2k-server-state` is now a cumulative hidden session option, and the
RequestGroup restore helper parses newline-delimited server-state payloads
while preserving the existing single-state format.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP5 persistence coverage for ED2K metadata, sources,
hashsets, AICH state, Kad state, shared files, and credits without broad test
scaffolding.
Blocked: none.

2026-05-18 CP5 partial
Changed: Fixed ED2K server-state resume scheduling. Restored server-state
endpoints are now merged into the RequestGroup server list so a save-session
restore can schedule server commands even when the user did not also pass
`--ed2k-server` or `--ed2k-server-list`.
Verified: The focused resume assertion failed before the fix with
`attrs->servers.size()` equal to 0. After the fix,
`cmake --build --preset default --target aria2_tests` passed and
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP5 persistence coverage for ED2K metadata, sources,
hashsets, AICH state, Kad state, shared files, and credits without broad test
scaffolding.
Blocked: none.

2026-05-18 CP5 partial
Changed: Normalized ED2K AICH link hashes to raw protocol hash bytes inside
the link model, with base32 decoding and encoding kept at the ED2K URI
boundary. Updated save-session serialization to include runtime-learned ED2K
piece hashsets in the persisted file link instead of writing only the original
input link.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP5 persistence coverage for ED2K sources, Kad state,
shared files, and credits without broad test scaffolding.
Blocked: none.

2026-05-18 CP5 verified
Changed: Persisted runtime-learned ED2K peers as inline ED2K link sources
during save-session, deduplicating against original link sources. Updated the
manual and maintenance overview so ED2K save-session documentation names the
implemented active download state exactly. Recorded that shared files and peer
credits remain CP13 and CP14 ownership because those subsystems do not exist
yet.
Verified: A focused session serializer assertion failed before the fix because
the learned source was absent from the saved ED2K link. After the fix,
`cmake --build --preset default --target aria2_tests` passed and
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: CP6 server TCP and UDP support is the next checkpoint.
Blocked: none.

2026-05-18 CP6 partial
Changed: Added shared ED2K server scheduling for initial and runtime-discovered
server endpoints. Learned OP_SERVERLIST endpoints now receive server login
commands. Server control commands no longer consume peer download slots, and
in-flight server connects are tracked to avoid duplicate reconnect storms.
Fixed packet body handling so server handlers can keep WRITE or DONE state
after parsing a packet.
Verified: The new server-list scheduling regression failed before the state
and scheduler fixes because the learned server never received a connection.
After the fix, `cmake --build --preset default --target aria2_tests` passed
and `ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`. `git diff --check` passed.
Remaining: Continue CP6 with server retry/backoff ownership, source request
cadence, server metadata handling, and any missing LowID/client UDP callback
behavior.
Blocked: none.
