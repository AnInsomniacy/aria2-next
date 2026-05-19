# Native ED2K/eMule Refactor Progress

This file is the compact evidence trail for the ED2K/eMule refactor. Keep it
checkpoint-sized. Do not record every investigation step, every routine local
build, or every small test run.

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

2026-05-19 RF2 verified
Changed: Completed the authoritative reference audit using only
`amule-official`, `emule-official-0.50a`, `mldonkey-official`,
`wireshark-official`, and `protocol-docs`. The audit split broad areas into
durable subsystem rows for links, metadata files, server TCP/UDP, OBFU,
HighID/LowID, callback, peer handshake, capability bits, multipacket, file
identifiers, secure identification, crypt/obfuscation, file requests, hashsets,
Source Exchange, AICH, compressed transfer, Kad, search, scheduling, resume,
sharing, upload queue, credits, CLI/RPC/Motrix fields, persistence, and
prune-only legacy surfaces.
Verified: Documentation-only checkpoint closure. Coverage keyword check
against the goal scope found no missing top-level subsystem. Tracker CSV width
checks and `git diff --check docs/maintenance/ed2k-refactor` passed.
Remaining: none for RF2.
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

2026-05-19 RF3/RF4/RF5 partial
Changed: Advanced server-source compatibility and adjacent peer-capability
truth. Server IDChange preserves TCP obfuscation ports. Server status preserves
extended UDP and obfuscation fields. Server and UDP FoundSources parsing keeps
LowID classification, client ID, user hash, and crypt metadata. Large-file TCP
source requests are gated by server capability. Peer hello parsing reads eMule
misc option tags from Hello and HelloAnswer. ED2K command-level tests were
narrowed to bounded loopback protocol checks so `aria2_tests` no longer does
public ED2K network work.
Verified: `cmake --preset default`, `cmake --build --preset default --target
aria2_tests -j 1`, `cmake --build --preset default --target aria2-next -j 1`,
`build/default/aria2_tests`, the loopback-only ED2K connection check, and
`git diff --check` passed.
Remaining: RF3 still needs callback-fail state handling, UDP source/status
closure, and final server-source live evidence before the checkpoint can be
marked verified.
Blocked: none.

2026-05-19 RF3 partial
Changed: TCP source requests now use `OP_GETSOURCES_OBFU` when the connected
server advertises TCP source obfuscation support. Callback-requested parsing
accepts extended payloads with trailing data while preserving endpoint, crypt
options, and user hash fields. Callback-returned peers that require encrypted
transport are not scheduled into the current plaintext peer connection path.
Verified: `cmake --build --preset default --target aria2_tests -j 1` passed
with the existing local linker warning about `/opt/homebrew/opt/tcl-tk/lib`.
`build/default/aria2_tests` passed with `OK (1114)`.
Remaining: RF3 still needs callback-fail state handling, UDP source/status
closure, and final server-source live evidence before the checkpoint can be
marked verified.
Blocked: none.

2026-05-19 RF5 partial
Changed: Preserved two peer request-flow fixes discovered during live baseline
work. Extended `OP_REQUESTFILENAME` now includes part status and complete
source count when the peer advertises extended requests. File-status bitfields
use the aMule/eMule least-significant-bit-first wire order. Single-part files
skip `OP_SETREQFILEID` after `OP_REQFILENAMEANSWER`, matching reference client
behavior.
Verified: Each change was verified with the local `aria2-next` and
`aria2_tests` targets at the time of implementation. Future RF5 work should
batch related request-flow fixes and run one final focused verification for
the batch.
Remaining: RF5 still needs multi-part status/hashset sequencing, multipacket
variants, file identifiers, controlled queue/transfer state verification, and
later live evidence when the goal reaches peer interoperability testing.
Blocked: none.
