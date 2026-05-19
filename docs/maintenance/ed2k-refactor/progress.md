# Native ED2K/eMule Refactor Progress

This file is the compact chronological evidence trail for the ED2K/eMule
refactor. Append one entry after every checkpoint, root-cause finding, or
material cleanup.

Use this format:

```text
YYYY-MM-DD RFx status
Changed: concise file and behavior summary.
Verified: exact command and result, or documentation-only reason.
Remaining: next concrete gap.
Blocked: none, or exact blocker.
```

## Log

2026-05-19 RF0 partial
Changed: Created the active ED2K refactor tracker. The tracker records the
authoritative reference set, the known live interoperability failure, the
required root-cause-first workflow, the checkpoint matrix, and the reference
audit ledger.
Verified: Documentation-only tracker creation in progress.
Remaining: Update the maintenance index and mark the previous ED2K tracker as
historical.
Blocked: none.

2026-05-19 RF0 verified
Changed: Updated the maintenance index so `ed2k-refactor` is the active ED2K
tracker, and marked the previous `ed2k` tracker as historical.
Verified: Documentation-only tracker activation. `git diff --check
docs/maintenance` passed. CSV width checks for `checkpoints.csv` and
`reference-audit.csv` passed after removing trailing blank records.
Remaining: Start RF1 with a short live-failure baseline before changing ED2K
source code.
Blocked: none.

2026-05-19 RF1 partial
Changed: Reproduced the XP ED2K fixture in controlled 30 second runs under
`/Users/sekiro/Desktop/aria2-next-ed2k-debug`. Added packet-boundary debug
logging for ED2K command send and receive paths. Fixed three authoritative
reference mismatches found during root-cause investigation: standalone
`OP_EMULEINFO` now uses old MuleInfo tags instead of Hello tags and includes
the UDP port, comments, and compatible-client fields used by aMule/eMule;
outgoing Hello now uses the eMule hash marker, an aMule-compatible structured
version tag, conservative capability bits, and reference tag ordering; local
peer capabilities no longer advertise multipacket before the multipacket
request flow is implemented; and server `OP_FOUNDSOURCES_OBFU` source metadata
can be parsed and routed.
Verified: `PATH=/opt/homebrew/bin:$PATH cmake --build --preset default
--target aria2-next aria2_tests` passed. `git diff --check` passed for the
changed ED2K files. Short XP-link live runs after the changes still reached
only server discovery. Server `45.82.80.155:5687` returned ordinary
`OP_FOUNDSOURCES` with two sources. Both peers `152.67.253.33:8337` and
`175.0.74.131:2822` reset after local `OP_HELLO`; no peer packet was received
and the file remained 0 bytes. The last run directory was
`/Users/sekiro/Desktop/aria2-next-ed2k-debug/rf1-hello-version-20260519-113122`.
Remaining: Determine whether the current public XP sources are stale or
whether local outgoing peer hello still differs in a rejection-relevant way.
Next evidence should compare the exact sent hello bytes against aMule/eMule
and exercise a known local peer fixture that can answer hello.
Blocked: Public XP fixture currently provides only two reachable-looking
server sources, and both close before any peer response.

2026-05-19 RF1/RF5 partial
Changed: Tested the newer Windows 11 x64 ED2K fixture in controlled 30 second
runs under `/Users/sekiro/Desktop/aria2-next-ed2k-debug`. The fixture is
better than the XP link for request-flow diagnosis because public servers
returned a source for hash `f3c8ae69aa86f1fc8204cc9bc4d9f61b`. A pre-fix run
reached peer `58.163.147.2:4377`, received `OP_HELLOANSWER`, sent
`OP_EMULEINFO` plus `OP_REQUESTFILENAME`, received `OP_EMULEINFOANSWER`, and
then saw the peer close before any file answer. Authoritative aMule/eMule
references show that when `extendedRequestsVersion > 0`, `OP_REQUESTFILENAME`
must include part-status data after the file hash, and when the version is
greater than 1 it must also include complete-source count. The draft
advertised extended request version 2 but sent only the 16 byte file hash.
Implemented an aria2-next native request payload builder that writes hash,
ED2K part count, local completed-part bitfield, and complete-source count.
Incoming shared-peer file requests now accept modern extended payloads and
pass the hash portion to the shared responder. A compact regression assertion
was added to the existing peer hello test to verify that the first outgoing
`OP_REQUESTFILENAME` is no longer the 16 byte short packet that aMule treats
as invalid after advertising extended requests.
Verified: `PATH=/opt/homebrew/bin:$PATH cmake --build --preset default
--target aria2-next aria2_tests` passed. A post-fix 30 second public run with
the same Windows 11 x64 fixture still ended at 0 bytes, but that run did not
reach the fixed request-flow boundary: server `45.82.80.155:5687` returned
one source, then peer `58.163.147.2:4377` reset immediately after local
`OP_HELLO` with no peer packet received. All observed servers assigned LowID
and emitted the public warning that the local network configuration is not
externally reachable. The post-fix run directory was
`/Users/sekiro/Desktop/aria2-next-ed2k-debug/rf1-win11-x64-extreq-20260519-114314`.
Remaining: Continue RF1/RF5 with controlled local peer fixtures and later
public runs that reach `OP_REQFILENAMEANSWER`, `OP_FILESTATUS`, queue, or part
request state. Public single-source runs cannot prove the request-flow fix
when the peer closes before answering hello.
Blocked: Public live verification is nondeterministic while the client is
LowID and current fixtures provide only one or two reachable-looking sources.

2026-05-19 RF5 partial
Changed: Corrected ED2K file-status bitfield packing and parsing to use the
least-significant-bit-first order used by aMule and eMule `WritePartStatus`
and `ProcessExtendedInfo`. The previous local helper round-tripped its own
high-bit-first encoding, but that encoding would invert peer part availability
within each byte when talking to real clients. Added one byte-level assertion
to the existing protocol payload test so the wire order is pinned to the
reference behavior without adding new scaffolding.
Verified: `PATH=/opt/homebrew/bin:$PATH cmake --build --preset default
--target aria2-next aria2_tests` passed.
Remaining: Continue RF5 request-flow work with file status, hashset, queue,
start-upload, accept-upload, and part request sequencing under controlled peer
fixtures before relying on public network runs.
Blocked: none.

2026-05-19 RF5 partial
Changed: Aligned the plain non-multipacket post-filename path with aMule and
eMule behavior for single-part files. After `OP_REQFILENAMEANSWER`, aria2-next
now skips `OP_SETREQFILEID` when the file is no larger than one ED2K part and
continues to source exchange plus `OP_STARTUPLOADREQ`. This avoids sending the
file-status request that the reference clients deliberately removed for
single-part files for better eDonkeyHybrid compatibility. The existing
outgoing peer test now asserts that a single-part request does not emit
`OP_SETREQFILEID` after a valid filename answer.
Verified: `PATH=/opt/homebrew/bin:$PATH cmake --build --preset default
--target aria2-next aria2_tests` passed.
Remaining: Continue RF5 for multi-part status/hashset sequencing,
multi-packet variants, file identifiers, and controlled queue/transfer state
verification.
Blocked: none.

2026-05-19 RF2 partial
Changed: Split the RF2 metadata audit row into focused durable rows for file
links, server and nodes links, server.met parsing, and nodes.dat parsing. The
new rows are based on the current authoritative local references: aMule
`ED2KLink`, eMule `ED2KLink`, the local aMule ED2K link protocol document,
aMule/eMule server list and server metadata loaders, MLDonkey server.met
import paths, and aMule/eMule/MLDonkey Kad contact loading paths. The audit now
separates behavior to adapt from legacy GUI update prompts and local UI
history that should be pruned or replaced by aria2-next task/session plumbing.
Verified: Documentation-only audit refinement. CSV width check for
`checkpoints.csv` and `reference-audit.csv` passed.
Remaining: Continue RF2 by auditing search-link/UI-only surfaces, social
packet surfaces, known-file metadata, and persistence formats before marking
RF2 verified.
Blocked: none.
