# ED2K/eMule Reference Alignment Progress

This file is the compact chronological evidence trail for the ED2K/eMule
reference-alignment refactor.

Use this format:

```text
YYYY-MM-DD RAx status
Changed: concise tracker, code, or behavior summary.
Verified: exact final command and result, or documentation-only reason.
Remaining: next concrete gap.
Blocked: none, or exact blocker.
```

Keep entries checkpoint-sized. Do not record every investigation step, ordinary
build, small parser edit, raw public-network log, packet capture, generated
report, local cache, or conversation summary.

## Log

2026-05-19 RA0 verified
Changed: Replaced the previous active ED2K refactor tracker with a new
reference-alignment tracker. The new tracker separates the progress index,
reference ledger, domain checkpoint files, and compact progress log. It changes
the goal from public-network download completion to authoritative reference
alignment, protocol truth, maintainable ownership, and compact local
verification.
Verified: `git diff --check docs/maintenance` passed. CSV consistency checks
passed for `checkpoint-index.csv`, `reference-ledger.csv`, and every
`checkpoints/*.csv` file.
Remaining: Start RA1 current implementation ownership audit.
Blocked: none.

2026-05-19 RA1 verified
Changed: Audited the current ED2K implementation surface and recorded the
source inventory in `overview.md`. The audit maps focused helper modules,
RequestGroup-owned state, TCP command ownership, UDP/Kad ownership, sharing,
upload, session, RPC, documentation, and local test coverage. It also records
the immediate capability-truth risk around advertised eMule peer info and the
main maintainability risk in `Ed2kCommand.cc` and `Ed2kKadCommand.cc`.
Verified: Focused `rg` and `sed` inspection of ED2K source, integration, and
test files; `git diff --check docs/maintenance`; CSV consistency check for all
`docs/maintenance/ed2k-refactor` CSV files.
Remaining: Start RA2 authoritative reference mapping against amule-official,
emule-official-0.50a, mldonkey-official, wireshark-official, and protocol-docs.
Blocked: none.

2026-05-19 RA2 verified
Changed: Completed the authoritative reference map. `overview.md` now records
the concrete reference entry points for aMule, eMule 0.50a, MLDonkey,
Wireshark, and protocol-docs. `reference-ledger.csv` remains the scope ledger
for port, adapt, replace, and prune decisions; domain rows stay pending until
their implementation checkpoints prove alignment.
Verified: Focused `find`, `rg`, and `sed` inspection of the authoritative
reference set; `git diff --check docs/maintenance`; CSV consistency check for
all `docs/maintenance/ed2k-refactor` CSV files.
Remaining: Start RA10 link and metadata alignment.
Blocked: none.

2026-05-19 RA10 verified
Changed: Aligned ED2K link and file identity parsing with the authoritative
aMule/eMule link behavior. File names now normalize path separators before
output path selection, file sizes reject values at or above the eMule/aMule
2^38 byte limit, empty part-hash lists are rejected, and `search` links parse
as metadata without becoming normal downloads. Updated the RA10 checkpoint and
reference ledger.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`build/default/aria2_tests` passed with `OK (1093)`.
Remaining: Start RA11 metadata file and persistence alignment.
Blocked: none.

2026-05-19 RA11 verified
Changed: Aligned metadata file import and persistence ownership. `server.met`
parsing now preserves operational tags for users, files, limits, UDP flags,
LowID users, UDP key, obfuscation ports, and dynip hostnames into native
ServerState. `nodes.dat` parsing now filters unusable and Kad1-only contacts
and applies the aMule/eMule verified-contact fallback for bootstrap files.
Legacy part.met, known.met, known2.met, and clients.met remain replaced by
aria2-next control files, hidden session state, SharedStore, and UploadQueue
ownership.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`build/default/aria2_tests` passed with `OK (1093)`.
Remaining: Start RA20 server TCP alignment.
Blocked: none.

2026-05-19 RA20 verified
Changed: Aligned ED2K server TCP handling with the authoritative references.
Classic `OP_GETSOURCES` keeps hash-plus-size payloads, TCP status no longer
clears UDP-only server metadata, IDChange accepts extension tails, server ident
updates name and description, server-list and server-ident parsers tolerate
trailing data, and malformed FoundSources or search result packets finish the
current request without marking the server failed.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`build/default/aria2_tests` passed with `OK (1093)`. `git diff --check`
passed.
Remaining: Start RA21 server UDP alignment.
Blocked: none.

2026-05-19 RA21 verified
Changed: Aligned ED2K server UDP and callback state. UDP status replies now
clear matched challenges and accept reference-style extension tails. Packed UDP
source replies preserve valid sources when later packets are unrelated or have
bogus trailing bytes. Server-mediated LowID callback remains supported while
buddy-only and direct UDP callback ownership stay pruned until a later
checkpoint proves they are required.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`build/default/aria2_tests` passed with `OK (1093)`.
Remaining: Start RA30 peer hello and capability truth.
Blocked: none.

2026-05-19 RA30 verified
Changed: Aligned peer hello capability truth against aMule, eMule, and
Wireshark. Local ED2K peer capability construction now lives in
`ed2k_peer.*` and is shared by active-download and shared-only peer commands.
Hello packets advertise implemented AICH, Unicode, compression, Source
Exchange, and large-file support. Multipacket, extended multipacket, crypt,
secure identification, Kad, and file identifiers remain unadvertised until
their packet and state paths are complete. Standalone eMule info keeps the
reference-compatible legacy tag shape.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`build/default/aria2_tests` passed with `OK (1094)`.
Remaining: Start RA31 modern peer request flow.
Blocked: none.

2026-05-19 RA31 verified
Changed: Closed the modern peer request-flow alignment audit. The current
plain fallback flow matches the authoritative aMule/eMule request sequence:
request filename with extended metadata, request file status for multi-part
files, request and parse hashsets with the reference hash-plus-count payload,
ask Source Exchange and AICH through separate eMule packets, start upload
through `OP_STARTUPLOADREQ`, and handle accept, queue-rank, no-file, filename,
status, hashset, AICH, and Source Exchange responses through the native peer
and shared-responder paths. Multipacket, extended multipacket, file identifier,
`OP_HASHSETREQUEST2`, and `OP_HASHSETANSWER2` remain disabled because RA30
keeps those local capability bits unadvertised.
Verified: Focused inspection against aMule `DownloadClient.cpp`,
`ClientTCPSocket.cpp`, `KnownFile.cpp`, eMule `UploadClient.cpp`,
`FileIdentifier.cpp`, `opcodes.h`, Wireshark ED2K dissector notes, and current
`Ed2kCommand.cc`, `Ed2kSharedResponder.*`, and `ed2k_peer.*`. No code changed,
so no C++ test run was needed for this checkpoint.
Remaining: Start RA32 peer transfer control and failure state.
Blocked: none.

2026-05-19 RA32 verified
Changed: Aligned peer transfer-control boundaries and failure state. The
current peer path validates normal and compressed part packet hash, range, and
length before disk writes, clears requested ranges on disconnect, cancel,
out-of-parts, no-file, bad packet, corrupt piece, and duplicate endpoint
replacement paths, and sends failed peers into PeerState retry/backoff without
halting unrelated sources. Added a protocol-boundary guard so 32-bit
`OP_REQUESTPARTS` payloads reject offsets above `UINT32_MAX`; large offsets use
the existing I64 request path.
Verified: The new `Ed2kHelperTest::testProtocolPayloads` assertion failed
before the guard because 32-bit request payloads accepted a 4 GiB offset. After
the fix, `cmake --build --preset default --target aria2_tests` passed and
`build/default/aria2_tests` passed with `OK (1094)`.
Remaining: Start RA40 disk transfer and integrity alignment.
Blocked: none.

2026-05-19 RA40 verified
Changed: Aligned ED2K MD4 hashset boundaries with the authoritative aMule and
eMule model. Hashset request and validation now use the ED2K theoretical
part-hash count instead of aria2 data-piece count. Files smaller than
`PARTSIZE` return no hashset entries. Exact `PARTSIZE` multiples request and
validate the reference trailing empty MD4 hash. Shared-file import and hashset
responses use the same boundary rules.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`build/default/aria2_tests` passed with `OK (1095)`. `git diff --check`
passed.
Remaining: Start RA50 source policy and Source Exchange alignment.
Blocked: none.

2026-05-19 RA50 verified
Changed: Aligned source policy and Source Exchange capability truth. Source
Exchange requests now choose `OP_REQUESTSOURCES2` only for SX2 peers, choose
hash-only `OP_REQUESTSOURCES` only for SX1 peers with version greater than 1,
and skip unsupported peers. Require-crypt source endpoints remain preserved as
metadata but no longer enter normal peer connect scheduling while encrypted
transport is unimplemented.
Verified: `cmake --build --preset default --target aria2_tests` passed.
`build/default/aria2_tests` passed with `OK (1095)`. `git diff --check`
passed.
Remaining: Start RA51 Kad alignment.
Blocked: none.

2026-05-19 RA51 verified
Changed: Aligned Kad routing and publish behavior with the authoritative
aMule/eMule/Wireshark references. Kad now answers bootstrap requests with
filtered closest contacts, excludes the requester when possible, uses the
configured ED2K TCP port in Kad hello, bootstrap, firewalled-check, and publish
paths, completes bootstrap transactions, answers targeted `KAD_REQ` lookups,
responds to `KAD_PING`, rejects unusable Kad routing contacts, and publishes
large-file HighID sources with source type 4. Kad1 and UI-only Kad behavior
remain pruned.
Verified: The large-file publish assertion failed before the source-type fix
with expected 4 and actual 1. After the fix, `git diff --check` passed,
`cmake --build --preset default --target aria2_tests` passed, and
`build/default/aria2_tests` passed with `OK (1096)`.
Remaining: Start RA60 sharing upload and credits alignment.
Blocked: none.

2026-05-19 RA60 verified
Changed: Closed sharing, upload queue, and credit alignment. The shared-file
store and responder already matched the non-pruned reference surface for
completed/imported file metadata, disk validation, filename/status/hashset,
Source Exchange, AICH, and part serving. UploadQueue now also rejects a second
queue entry with the same 16-byte user hash, matching the aMule/eMule duplicate
identity guard without importing secure-ident or GUI queue policy.
Verified: The new `Ed2kSharedStoreTest::testUploadQueueRejectsDuplicateUserHash`
failed before the UploadQueue guard because two endpoints with the same user
hash could occupy the queue. After the fix, `cmake --build --preset default
--target aria2_tests` passed and `build/default/aria2_tests` passed with
`OK (1097)`. `git diff --check` passed.
Remaining: Start RA70 search RPC and Motrix alignment.
Blocked: none.

2026-05-19 RA70 verified
Changed: Closed search, RPC, and Motrix field alignment. Existing server/Kad
search transport, result deduplication, local filters, merged source counts,
complete source counts, generated `ed2kLink`, boolean `moreResults`, and nested
ED2K `tellStatus` fields already matched the non-pruned tracker surface.
Search result metadata parsing now also maps the legacy named media codec tag
`codec` into `mediaCodec`, matching the aMule/eMule metadata conversion path.
The tracker path for the manual was corrected to `docs/manual/en/aria2-next.rst`.
Verified: The new `Ed2kHelperTest::testSearchResultPayload` assertion failed
before the parser fix because `mediaCodec` was empty for a named `codec` tag.
After the fix, `cmake --build --preset default --target aria2_tests` passed and
`build/default/aria2_tests` passed with `OK (1097)`.
Remaining: Start RA71 documentation and final local audit.
Blocked: none.
