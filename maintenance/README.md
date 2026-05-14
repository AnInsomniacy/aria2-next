# Maintenance

This directory contains project maintenance records that are useful for ongoing
stewardship but are not part of the build, test, or release execution path.

## Layout

| Path | Purpose |
|------|---------|
| `open-issue-review.md` | Human-readable summary of the upstream open issue review |
| `issue-review-matrix.csv` | Complete issue-by-issue review matrix |
| `cmake-migration-progress.md` | Checkpoint log for the CMake migration |

Maintenance records should be durable and compact. Temporary API payloads,
scratch research files, generated reports, and local caches should not be
committed here.

Release history is tracked by git tags and GitHub Releases. Do not add
standalone changelog snapshots unless they are generated release artifacts.
