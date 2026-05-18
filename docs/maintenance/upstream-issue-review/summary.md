# Upstream Open Issue Review

This document records the final review result for the open bug issues imported from
the upstream `aria2/aria2` GitHub issue tracker during the aria2-next docs/maintenance
work.

The repository keeps only the durable review artifacts:

- `docs/maintenance/upstream-issue-review/summary.md` is the human-readable
  summary.
- `docs/maintenance/upstream-issue-review/matrix.csv` is the complete
  issue-by-issue review matrix.

The temporary GitHub API cache, raw issue payloads, and raw comment payloads were
intentionally removed after the matrix was preserved. They were useful as local
working data, but they are not appropriate source artifacts for this repository.

## Scope

The review covered 137 upstream issues that were still open at the time of the
import and survived the bug-focused cleanup pass. Feature requests, broad support
questions, duplicates without actionable evidence, and stale reports without a
maintainable code path were excluded before this final matrix was produced.

Each retained issue has one row in
`docs/maintenance/upstream-issue-review/matrix.csv`. The matrix records the
issue number, priority, affected area, title, final state, root-cause group,
required action, and review evidence.

## Result

The final matrix contains 137 reviewed issues.

By priority:

- P1: 98
- P2: 13
- P3: 26

By module:

- DNS and IPv6: 44
- TLS and certificates: 31
- BitTorrent and DHT: 24
- Filesystem and session handling: 14
- HTTP range and retry behavior: 11
- RPC and WebSocket: 5
- Other core behavior: 5
- Build and tests infrastructure: 3

By required action:

- Fixed and verified: 37
- No code change required: 51
- Configuration or documented behavior: 10
- No further action: 8
- Environment or permission issue: 8
- Site or usage issue: 7
- Platform or configuration issue: 6
- Environment or network issue: 4
- Environment or build issue: 2
- Environment or configuration issue: 2
- Configuration or input validation: 1
- Windows verification still required: 1

## Interpretation

`fixed-verified` rows correspond to issues addressed by code or tests changes in
this maintenance pass and verified with the local test suite or targeted builds.

`no-code`, environment, site, usage, configuration, documented-behavior, and
not-reproducible rows are intentionally retained in the matrix. They show that
the issue was reviewed and that the final decision did not justify a source
change from the available evidence.

`needs-architecture/no-code` rows identify real limitations that require larger
design work rather than small isolated patches. They are kept as explicit
follow-up candidates instead of being hidden as completed code fixes.

## Verification

The final review matrix was checked before preservation:

- It contains 137 data rows.
- Every retained issue has a final state.
- Every retained issue has a required action.
- Every retained issue has review evidence.
- The local full tests run completed successfully after the docs/maintenance fixes:
  `cd tests && ./aria2c` reported `OK (1009)`.

The matrix is the authoritative audit artifact for this issue review pass.
