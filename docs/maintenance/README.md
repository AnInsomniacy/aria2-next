# Maintenance

This directory contains durable project maintenance records. These files are
useful for long-running stewardship, but they are not part of the build, test,
or release execution path.

The root of this directory intentionally contains only this index. Each
maintenance task owns its own subdirectory.

| Path | Purpose |
| --- | --- |
| `cmake-migration/progress.md` | Completed checkpoint log for the CMake migration |
| `upstream-issue-review/summary.md` | Completed human-readable summary of the upstream issue review |
| `upstream-issue-review/matrix.csv` | Completed issue-by-issue review matrix |
| `ed2k/overview.md` | Historical tracker for the first native ED2K/eMule draft |
| `ed2k/checkpoints.csv` | Historical checkpoint matrix for the first ED2K/eMule draft |
| `ed2k/reference-parity.csv` | Historical parity ledger for the first ED2K/eMule draft |
| `ed2k/progress.md` | Historical chronological ED2K/eMule draft log |
| `ed2k-refactor/overview.md` | Active tracker entry point for the ED2K/eMule reference-alignment refactor |
| `ed2k-refactor/checkpoint-index.csv` | Active checkpoint index for the ED2K/eMule refactor |
| `ed2k-refactor/reference-ledger.csv` | Active authoritative reference alignment and pruning ledger |
| `ed2k-refactor/checkpoints/` | Active domain-specific ED2K/eMule checkpoint matrices |
| `ed2k-refactor/progress.md` | Active chronological ED2K/eMule refactor progress log |
| `ed2k-download-hardening/overview.md` | Active tracker entry point for ED2K/eMule download reliability hardening |
| `ed2k-download-hardening/roadmap.csv` | Active checkpoint index for ED2K download hardening |
| `ed2k-download-hardening/capability-ledger.csv` | Active aMule-runtime capability and pruning ledger |
| `ed2k-download-hardening/checkpoints/` | Active narrow ED2K download hardening checkpoint matrices |
| `ed2k-download-hardening/progress.md` | Active chronological ED2K download hardening progress log |
| `libtorrent-bt-migration/overview.md` | Active tracker entry point for the libtorrent BitTorrent replacement |
| `libtorrent-bt-migration/roadmap.csv` | Active checkpoint index for the libtorrent BitTorrent replacement |
| `libtorrent-bt-migration/capability-ledger.csv` | Active BitTorrent capability migration and removal ledger |
| `libtorrent-bt-migration/checkpoints/` | Active checkpoint matrices for the libtorrent BitTorrent replacement |
| `libtorrent-bt-migration/progress.md` | Active chronological libtorrent BitTorrent replacement progress log |
| `core-modernization/overview.md` | Active tracker entry point for the core modernization migration |
| `core-modernization/roadmap.csv` | Active checkpoint index for core modernization |
| `core-modernization/capability-ledger.csv` | Active capability migration, retention, replacement, and removal ledger |
| `core-modernization/dependency-ledger.csv` | Active dependency ownership, packaging, and removal ledger |
| `core-modernization/checkpoints/` | Active checkpoint matrices for core modernization |
| `core-modernization/progress.md` | Active chronological core modernization progress log |

Maintenance records should be durable and compact. Temporary API payloads,
scratch research files, generated reports, local caches, and conversation logs
should not be committed here.

Release history is tracked by git tags and GitHub Releases. Do not add
standalone changelog snapshots unless they are generated release artifacts.
