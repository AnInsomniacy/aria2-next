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

2026-05-18 CP12 partial
Changed: Added a focused native Kad traversal controller for source and
keyword lookups. Lookup now keeps candidate state, limits in-flight requests,
adds closer contacts from `KAD_RES`, continues querying closer contacts before
the final search phase, and completes `KAD_SEARCH_RES` transactions so
successful search contacts are not later failed by timeout cleanup. The UDP
command still owns socket I/O and packet scheduling on the existing event loop.
Verified: The focused command simulation failed before the change because the
first closer contact received `KAD_SEARCH_SOURCES_REQ` immediately instead of
another `KAD_REQ`. After the change, `cmake --build --preset default --target
aria2_tests` passed with the existing local Tcl/Tk search-path linker warning,
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`, and `git diff --check` passed.
Remaining: CP12 still needs Kad source publish, firewalled checks, and durable
operational state before it can be verified.
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

2026-05-18 CP6 partial
Changed: Added a persisted `nextSourceRequestTime` to ED2K server state and
allowed handshake-complete servers to be rescheduled when the timed source
refresh is due. The short-lived server command model now requests initial
sources after IDChange, records the next refresh, and can reconnect for later
GetSources without consuming peer download slots. The server-state parser still
accepts the previous draft payload version and defaults the new refresh cursor
to zero.
Verified: The focused source-refresh scheduler regression failed before the
change because a handshake-complete server was always skipped. After the
change, `cmake --build --preset default --target aria2_tests` passed,
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`, and `git diff --check` passed.
Remaining: Continue CP6 with server metadata handling and missing
LowID/client UDP callback behavior.
Blocked: none.

2026-05-18 CP6 partial
Changed: Added server.met entry parsing for server name, description, and
hostname preference tags. RequestGroup creation now merges server.met metadata
into ED2K server state for download and search tasks, and server-state session
payloads persist the metadata while still accepting version 1 and 2 draft
payloads.
Verified: A focused parser regression failed before implementation because
`parseServerMetEntries` did not exist. After the fix,
`cmake --build --preset default --target aria2_tests` passed and
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP6 with missing LowID/client UDP callback behavior and a
final server-session audit.
Blocked: none.

2026-05-18 CP6 partial
Changed: Allowed OP_CALLBACKREQUESTED parsing to accept both the base 6-byte
endpoint payload and the 23-byte extended payload used by aMule/libed2k style
servers. The current command path still routes the callback by endpoint and
does not claim crypt-layer handling from the extension bytes.
Verified: The focused callback parser assertion failed before implementation
for the 23-byte extended payload. After the fix,
`cmake --build --preset default --target aria2_tests` passed and
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP6 with client UDP reask behavior and a final
server-session audit.
Blocked: none.

2026-05-18 CP6 partial
Changed: Added compact ED2K client UDP helpers for REASKFILEPING and REASKACK.
Ed2kKadCommand now accepts eMule UDP reask packets in the existing UDP socket,
records REASKACK peers as queued, records QUEUEFULL and FILENOTFOUND peers as
dead, and answers matching REASKFILEPING packets with a minimal REASKACK. Full
upload-queue ranks and buddy callback routing remain CP14-owned behavior.
Verified: `cmake --build --preset default --target aria2_tests` passed and
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP6 with a final server-session audit, then move to CP7 or
record any CP14-owned UDP upload details in the parity ledger.
Blocked: none.

2026-05-18 CP6 verified
Changed: Completed the CP6 server-session audit and marked server connection
policy verified. The implemented server surface now covers direct servers,
server.met metadata, login, IDChange HighID/LowID state, source requests,
found sources, server callback routing, server status/messages, learned server
lists, UDP global status, retry/backoff, persisted server state, and compact
client UDP reask handling. UDP buddy callback and upload-queue-aware ranks
remain documented as CP14 upload ownership.
Verified: `git diff --check` passed. `cmake --build --preset default --target
aria2_tests` passed. `ctest --preset default --output-on-failure -R
aria2_tests` passed with `100% tests passed, 0 tests failed out of 1`.
Remaining: Move to CP7 peer outbound download session.
Blocked: none.

2026-05-18 CP7 partial
Changed: Added first-class ED2K peer state ownership for queue rank, part
status, dead-peer state, failure count, last failure time, and retry backoff.
UDP REASKACK, QUEUEFULL, and FILENOTFOUND now update PeerState instead of
separate endpoint lists, and peer scheduling skips dead peers until retry time.
Verified: A focused peer-state test failed before implementation because no
PeerState API existed. After implementation, `cmake --build --preset default
--target aria2_tests` passed and `ctest --preset default --output-on-failure
-R aria2_tests` passed with `100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP7 by routing queue rank packets, peer failure paths,
cancel/out-of-parts handling, and download-session state through PeerState.
Blocked: none.

2026-05-18 CP7 partial
Changed: Routed peer TCP queue-rank, accept-upload, out-of-parts, no-file, and
cancel-transfer packets through PeerState. Added OP_QUEUERANKING and
OP_CANCELTRANSFER constants plus compact queue-rank payload parsing for 2-byte
and 4-byte rank packets.
Verified: The focused queue-rank peer command test failed before the
implementation because PeerState did not record the remote rank. After the
implementation, `cmake --build --preset default --target aria2_tests` passed
and `ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP7 with 32-bit and 64-bit request lifecycle ownership,
duplicate peer rejection, disconnect handling, and corrupt-piece retry through
PeerState.
Blocked: none.

2026-05-18 CP7 partial
Changed: Routed peer command connection failures through PeerState retry
backoff with a generic failure marker. Kept no-file as a separate protocol
state so transport failures do not claim the peer lacks the file.
Verified: The focused peer command failure test failed before the change
because PeerState stayed live after the command failed. After the change,
`cmake --build --preset default --target aria2_tests` passed and `ctest
--preset default --output-on-failure -R aria2_tests` passed with `100% tests
passed, 0 tests failed out of 1`.
Remaining: Continue CP7 with request lifecycle ownership, duplicate peer
rejection, disconnect handling, and corrupt-piece retry through PeerState.
Blocked: none.

2026-05-18 CP7 partial
Changed: Added PeerState connecting ownership so peer scheduling skips
already in-flight peers and clears the connecting flag when a peer command is
destroyed. This suppresses duplicate outbound peer commands without changing
the existing single event loop.
Verified: The focused in-flight peer scheduling test failed before the change
because PeerState had no connecting state. After the change, `cmake --build
--preset default --target aria2_tests` passed and `ctest --preset default
--output-on-failure -R aria2_tests` passed with `100% tests passed, 0 tests
failed out of 1`.
Remaining: Continue CP7 with request lifecycle ownership, disconnect handling,
and corrupt-piece retry through PeerState.
Blocked: none.

2026-05-18 CP7 partial
Changed: Added a focused local-socket regression proving that a peer which
sends a full corrupt one-piece ED2K part is marked dead and placed into
PeerState retry backoff through the existing `DlRetryEx` peer failure path.
No production code change was needed after the corrected failing check showed
the current state path already handles the corrupt-piece transition.
Verified: The first test draft failed before it reached piece verification
because the corrupt payload was shorter than the file. After correcting the
payload to preserve the requested range and corrupt only the content,
`cmake --build --preset default --target aria2_tests` passed and `ctest
--preset default --output-on-failure -R aria2_tests` passed with `100% tests
passed, 0 tests failed out of 1`.
Remaining: Continue CP7 with remaining request lifecycle ownership,
disconnect handling, and peer download state separation where it reduces the
overloaded command path.
Blocked: none.

2026-05-18 CP7 partial
Changed: Added PeerState ownership for requested ED2K part ranges. Outbound
peer part requests now record the requested ranges, successful full-piece
completion clears them, and disconnected, out-of-parts, cancel, and failure
states also clear pending request state. This mirrors the reference queue
distinction between requested/downloaded blocks without introducing the full
CP10 source and piece policy yet.
Verified: The focused requested-parts peer command test failed before
implementation because PeerState had no requested range state. After the
change, `cmake --build --preset default --target aria2_tests` passed, `ctest
--preset default --output-on-failure -R aria2_tests` passed with `100% tests
passed, 0 tests failed out of 1`, and `git diff --check` passed.
Remaining: Continue CP7 with disconnect behavior audit and decide whether a
small peer download-session boundary should be split from `Ed2kCommand`.
Blocked: none.

2026-05-18 CP7 partial
Changed: Tightened ED2K peer disconnect handling during packet reads. EOF while
reading a partial header or body now raises the existing retry path instead of
silently rescheduling a peer command with no readable socket. The path records
PeerState failure, clears connecting and accepted state, clears requested
parts, and applies reconnect backoff.
Verified: The focused half-packet disconnect regression failed before the fix
because PeerState stayed live. After the fix, `cmake --build --preset default
--target aria2_tests` passed and `ctest --preset default --output-on-failure
-R aria2_tests` passed with `100% tests passed, 0 tests failed out of 1`.
Remaining: Continue CP7 by auditing bad part range and segment release
behavior, then decide whether a small peer download-session boundary should be
split from `Ed2kCommand`.
Blocked: none.

2026-05-18 CP7 partial
Changed: Moved ED2K part range validation ahead of disk writes. Peer part data
is now accepted only when the begin offset matches an in-flight SegmentMan
segment owned by the command. Unexpected part ranges enter the existing retry
backoff path without writing the payload to the target file.
Verified: The focused unexpected-part regression failed before the fix because
the payload reached the target file before range validation. After the fix,
`cmake --build --preset default --target aria2_tests` passed and `ctest
--preset default --output-on-failure -R aria2_tests` passed with `100% tests
passed, 0 tests failed out of 1`.
Remaining: Continue CP7 by deciding whether a small peer download-session
boundary should be split from `Ed2kCommand` before moving to CP9/CP10.
Blocked: none.

2026-05-18 CP7 partial
Changed: Split ED2K peer data transfer ownership out of the overloaded
`Ed2kCommand` path into `Ed2kPeerTransfer`. The new boundary owns expected
part length lookup, in-flight SegmentMan validation, disk writes, segment
completion, and MD4 piece verification. `Ed2kCommand` still owns socket,
packet, and peer protocol state until CP9/CP10 justify a larger session split.
Verified: `cmake --build --preset default --target aria2_tests` passed after
CMake regenerated for the new source file. `ctest --preset default
--output-on-failure -R aria2_tests` passed with `100% tests passed, 0 tests
failed out of 1`.
Remaining: Continue CP7 by auditing the remaining peer packet state
transitions, then either mark CP7 verified or record the exact CP9/CP10
dependency.
Blocked: none.

2026-05-18 CP7 verified
Changed: Closed the outbound peer download checkpoint with focused evidence
for the remaining accepted transfer paths. Added compact coverage for
compressed peer parts flowing through the command, zlib inflate path,
Ed2kPeerTransfer write path, MD4 verification, requested-part cleanup, and
final file content. Added 64-bit request-parts payload assertions to the
protocol payload coverage.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Move to CP8 incoming peer listener. Source Exchange merge policy,
full source/piece scheduling, incoming peer acceptance, sharing, and upload
remain owned by later checkpoints.
Blocked: none.

2026-05-18 CP8 verified
Changed: Added an ED2K TCP listener for incoming active-download peers. The
listener binds through aria2-next's existing EventPoll path, is created once
per engine from ED2K request-group startup, honors `--ed2k-listen-port`, routes
accepted sockets only when there is a unique active ED2K download group,
rejects duplicate active endpoints, and hands accepted sockets to the existing
peer command state machine. Incoming peer Hello handling now records the remote
user hash and migrates the temporary TCP source endpoint to the peer's
advertised ED2K listen port. Shared/completed-file matching remains CP13/CP14
ownership.
Verified: `git diff --check` passed. `cmake --build --preset default --target
aria2_tests` passed with the existing local Tcl/Tk search-path linker warning.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: Move to CP9 Source Exchange merge policy. Full shared-file binding
and upload responses remain owned by CP13 and CP14.
Blocked: none.

2026-05-18 CP9 verified
Changed: Added a native Source Exchange merge policy for SX1 and SX2 answers.
The merge path now filters invalid endpoints, loopback endpoints, and the
answering peer itself; deduplicates repeated sources; preserves SX2 v4 user
hash and crypt options; records source ownership in PeerState; and leaves
existing retry/backoff state intact for known peers. Server, resume, Kad, and
SX peer sources now carry explicit source flags for later scheduling and status
work.
Verified: `git diff --check` passed. `cmake --build --preset default --target
aria2_tests` passed with the existing local Tcl/Tk search-path linker warning.
`ctest --preset default --output-on-failure -R aria2_tests` initially exposed
a non-deterministic existing local socket timing failure in
`testEd2kPeerCommandBacksOffOnDisconnect`; an immediate direct `aria2_tests`
rerun and a second `ctest --preset default --output-on-failure -R aria2_tests`
both passed with `100% tests passed, 0 tests failed out of 1`.
Remaining: Move to CP10 ED2K source and piece policy.
Blocked: none.

2026-05-18 CP10 verified
Changed: Added a focused ED2K source and piece policy module. Peer scheduling
no longer depends on a simple cursor; it skips ineligible peers, preserves
retry backoff, ranks lower failure counts first, then ED2K source ownership,
queue state, and queue rank. Part requests now use peer part availability,
preserve suitable in-flight segments, and reclaim clean idle-owner segments
through SegmentMan instead of adding a separate ED2K block manager. Inline
file-link sources now keep a distinct source label instead of being recorded
as resume sources.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`. `git diff --check` passed.
Remaining: Move to CP11 integrity and compressed transfer completion. AICH tree
verification and recovery remain incomplete, and sharing/upload behavior
remains CP13/CP14 scope.
Blocked: none.

2026-05-18 CP11 verified
Changed: Completed the active-download integrity checkpoint. AICH root hashing
now keeps ED2K part-level tree structure, AICH recovery packets are parsed and
verified against the trusted root, OP_AICHANSWER stores verified recovery
blocks by part index, and corrupt ED2K pieces can preserve AICH-verified clean
blocks while retrying damaged ranges through the existing SegmentMan,
PieceStorage, and disk paths. The parser now returns false for truncated AICH
recovery payloads instead of leaking parser exceptions.
Verified: `cmake --build --preset default --target aria2_tests` passed with
the existing local Tcl/Tk search-path linker warning. `build/default/aria2_tests`
passed with `OK (1086)`. `ctest --preset default --output-on-failure -R
aria2_tests` passed with `100% tests passed, 0 tests failed out of 1`.
`git diff --check` passed.
Remaining: Move to CP12 Kad bootstrap and routing. Sharing and upload-side AICH
serving remain CP13 and CP14 ownership.
Blocked: none.

2026-05-18 CP12 partial
Changed: Advanced Kad source and keyword lookup from direct one-shot search
requests to the reference two-stage shape. Known Kad contacts now receive
`KAD_REQ` first. `KAD_RES` responses are matched to tracked transactions,
closer contacts are inserted into the routing table, and the returned contacts
then receive `KAD_SEARCH_SOURCES_REQ` or `KAD_SEARCH_KEYS_REQ`.
Verified: `git diff --check` passed. `cmake --build --preset default --target
aria2_tests` passed. `ctest --preset default --output-on-failure -R
aria2_tests` passed with `100% tests passed, 0 tests failed out of 1`.
Remaining: CP12 still needs a full iterative traversal controller,
timeout/failure backoff, refresh, source publish, firewalled checks, and
durable operational state.
Blocked: none.

2026-05-18 CP12 partial
Changed: Connected Kad transaction timeout handling back to routing quality.
Kad transactions now retain the contacted node, and expired transactions mark
that node failed in the routing table instead of being discarded silently.
Verified: `git diff --check` passed. `cmake --build --preset default --target
aria2_tests` passed with the existing local Tcl/Tk search-path linker warning.
`ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: CP12 still needs a full iterative traversal controller, refresh,
source publish, firewalled checks, and durable operational state.
Blocked: none.

2026-05-18 CP12 partial
Changed: Added Kad routing-table refresh scheduling on the existing UDP command
path. Refresh now sends `KAD_REQ` with `KAD_FIND_NODE` to close known contacts.
Kad transactions carry an explicit purpose, and `KAD_RES` completion now
matches endpoint plus target so maintenance refresh cannot consume source or
keyword lookup responses from the same node.
Verified: The focused refresh command test failed before implementation because
the only `KAD_REQ` target was the file hash. After the change,
`git diff --check` passed, `cmake --build --preset default --target
aria2_tests` passed with the existing local Tcl/Tk search-path linker warning,
and `ctest --preset default --output-on-failure -R aria2_tests` passed with
`100% tests passed, 0 tests failed out of 1`.
Remaining: CP12 still needs a full iterative traversal controller, source
publish, firewalled checks, and durable operational state.
Blocked: none.

2026-05-18 CP12 partial
Changed: Added a native Kad published-source index and wired
`KAD_PUBLISH_SOURCE_REQ` plus direct `KAD_SEARCH_SOURCES_REQ` handling into the
existing UDP command path. Published source entries are stored by file hash,
deduplicated by source ID or endpoint, acknowledged with `KAD_PUBLISH_RES`, and
returned as `KAD_SEARCH_RES` through the focused Kad search-result payload
helper. This matches the local reference behavior for inbound source publish
and local source lookup without adding another event loop or runtime.
Verified: The focused local UDP publish/search command test failed before the
change because no publish response was sent. After the change,
`cmake --build --preset default --target aria2_tests` passed with the existing
local Tcl/Tk search-path linker warning, and `ctest --preset default
--output-on-failure -R aria2_tests` passed with `100% tests passed, 0 tests
failed out of 1`.
Remaining: CP12 still needs outbound source publish, firewalled checks, and
durable operational state.
Blocked: none.

2026-05-18 CP12 partial
Changed: Added passive Kad firewalled check handling on the existing UDP
command path. `KAD_FIREWALLED_REQ` is now parsed and answered with
`KAD_FIREWALLED_RES` containing the requester's observed UDP address, matching
the reference behavior for remote firewall probes without adding buddy,
callback, or upload-queue behavior.
Verified: The focused local UDP firewalled request test failed before the
change because no response was sent. After the change, `cmake --build --preset
default --target aria2_tests` passed with the existing local Tcl/Tk search-path
linker warning, `ctest --preset default --output-on-failure -R aria2_tests`
passed with `100% tests passed, 0 tests failed out of 1`, and `git diff
--check` passed.
Remaining: CP12 still needs outbound source publish, active firewalled checks,
and durable operational state.
Blocked: none.

2026-05-18 CP12 partial
Changed: Added active Kad firewalled probes on the existing UDP command path.
When known Kad nodes are available and the ED2K listen port is enabled,
`Ed2kKadCommand` sends `KAD_FIREWALLED_REQ` to close contacts, tracks the
transaction as a firewalled check, records observed addresses from
`KAD_FIREWALLED_RES`, and updates request-level firewalled state without adding
a second traversal engine.
Verified: The focused local UDP firewalled probe test failed before the change
because no `KAD_FIREWALLED_REQ` was sent. After the change, `cmake --build
--preset default --target aria2_tests` passed with the existing local Tcl/Tk
search-path linker warning, `ctest --preset default --output-on-failure -R
aria2_tests` passed with `100% tests passed, 0 tests failed out of 1`, and
`git diff --check` passed.
Remaining: CP12 still needs outbound source publish and durable operational
state.
Blocked: none.
