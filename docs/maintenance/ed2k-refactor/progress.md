# Native ED2K/eMule Refactor Progress

This file is the compact evidence trail for the ED2K/eMule refactor. Keep it
checkpoint-sized. Do not record every investigation step, every routine local
build, or every small test run.

Verification is batched by default. Run intermediate checks only when they
prove a root cause, protect a risky protocol boundary, or close a checkpoint.
Ordinary parser edits, log wording changes, small cleanup, and mechanical
refactors should be verified together at the end of the active batch. Do not
add a progress entry for a test run unless it changes checkpoint state,
confirms a regression boundary, or records a durable blocker.

Tests must stay focused. Add or run tests only for parser contracts, packet
framing, protocol state transitions, persistence formats, disk and integrity
safety, RPC field contracts, or confirmed regressions. Do not add broad
scaffolding, placeholder tests, tests for incidental logging, or tests that
mirror implementation details. Read command output directly from the terminal.
Keep raw public-network logs and packet captures outside the repository under
`/Users/sekiro/Desktop/aria2-next-ed2k-debug` only when network evidence is
needed.

Use this format:

```text
YYYY-MM-DD RFx status
Changed: concise file and behavior summary.
Verified: final command and result for the completed batch, or
documentation-only reason.
Remaining: next concrete gap.
Blocked: none, or exact blocker.
```

## Log

2026-05-19 RF0 verified
Changed: Updated the maintenance index so `ed2k-refactor` is the active ED2K
tracker, marked the previous `ed2k` tracker as historical, and created the
refactor overview, checkpoint matrix, reference audit, and progress log.
Verified: Documentation-only tracker activation. `git diff --check
docs/maintenance` passed. CSV width checks for `checkpoints.csv` and
`reference-audit.csv` passed after removing trailing blank records.
Remaining: none for RF0.
Blocked: none.

2026-05-19 RF1 verified
Changed: Closed the live failure baseline checkpoint using the existing
controlled public runs under `/Users/sekiro/Desktop/aria2-next-ed2k-debug`.
The baseline records the XP fixture and Windows 11 fixture behavior without
claiming public transfer success. XP runs consistently parsed the link,
created the task, connected to ED2K servers, received LowID warnings, received
one or two server sources, sent local peer `OP_HELLO`, then saw the peer reset
before any peer packet. A Windows 11 x64 run reached `OP_HELLOANSWER`, sent
`OP_EMULEINFO` and `OP_REQUESTFILENAME`, received `OP_EMULEINFOANSWER`, and
closed before a file answer; later runs against the same public source reset
after local `OP_HELLO`.
Verified: Documentation-only checkpoint closure based on existing controlled
logs:
`/Users/sekiro/Desktop/aria2-next-ed2k-debug/rf1-hello-version-20260519-113122/aria2-ed2k-test.log`,
`/Users/sekiro/Desktop/aria2-next-ed2k-debug/rf1-win11-x64-20260519-113528/aria2-ed2k-test.log`,
and
`/Users/sekiro/Desktop/aria2-next-ed2k-debug/rf1-win11-x64-extreq-20260519-114314/aria2-ed2k-extreq.log`.
The packet boundary is server source discovery succeeded, public peer progress
is nondeterministic under LowID, and RF3 must classify source and callback
paths before direct peer failures can be treated as pure peer-handshake bugs.
Remaining: Start RF3 by auditing and correcting server source classification,
HighID/LowID state, OBFU metadata, and callback routing.
Blocked: none.

2026-05-19 RF2 verified
Changed: Completed the authoritative reference audit using only
`amule-official`, `emule-official-0.50a`, `mldonkey-official`,
`wireshark-official`, and `protocol-docs`. The audit covers links, metadata
files, server TCP/UDP, OBFU, HighID/LowID, callback, peer handshake,
capabilities, request flow, Source Exchange, AICH, compressed transfer, Kad,
search, scheduling, resume, sharing, upload, credits, CLI/RPC/Motrix fields,
persistence, and prune-only legacy surfaces.
Verified: Documentation-only checkpoint closure. Coverage check, CSV width
checks, and `git diff --check docs/maintenance/ed2k-refactor` passed.
Remaining: none for RF2.
Blocked: none.

2026-05-19 RF3/RF5 partial
Changed: Advanced server-source compatibility and kept adjacent request-flow
fixes found during baseline work. RF3 now preserves server obfuscation and
extended UDP metadata, parses TCP and UDP source crypt metadata, gates
large-file source requests by server capability, sends `OP_GETSOURCES_OBFU`
only when advertised, accepts extended callback-requested payloads, skips
plaintext scheduling for sources that require encrypted transport, handles
server `OP_REJECT`, keeps the server session alive after `OP_CALLBACK_FAIL`,
and handles packed UDP server source replies using the reference shape where
only the original payload is compressed. RF5 adjacent work parses eMule misc
option tags, sends extended filename requests when advertised, writes
file-status bitfields in reference wire order, and skips the extra
request-file-id step for single-part files.
Verified: One focused local build covered the affected source path:
`cmake --build --preset default --target aria2_tests -j 1` passed with the
existing local `/opt/homebrew/opt/tcl-tk/lib` linker warning, and
`git diff --check` passed. Socket-heavy ED2K command simulations were removed
from `DownloadHelperTest` because they duplicated integration behavior and
made the full CppUnit binary depend on local bind/connect permission. Keep
future RF3/RF5 checks batched, with live ED2K runs reserved for checkpoint
closure or final interoperability evidence.
Remaining: RF3 still needs final callback/source-state review and one
server-source evidence run before checkpoint closure. RF5 remains pending for
multipart status/hashset sequencing, multipacket variants, file identifiers,
queue/transfer state verification, and later live evidence.
Blocked: none.

2026-05-19 RF3 partial
Changed: Split ED2K server source-request throttling from active socket state.
After a source request is queued, the server session now stays connected until
the request is rejected, skipped, or answered. The server is marked idle only
when that source request path is finished, which prevents the scheduler from
opening duplicate commands to the same server while an `OP_FOUNDSOURCES` reply
is still pending.
Verified: `git diff --check` passed. `cmake --build --preset default --target
aria2_tests -j 1` passed with the existing local
`/opt/homebrew/opt/tcl-tk/lib` linker warning.
Remaining: RF3 still needs the final UDP source/callback audit, tracker status
updates for the verified RF3 rows, and one short server-source evidence run
before checkpoint closure.
Blocked: none.
