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

Maintenance records should be durable and compact. Temporary API payloads,
scratch research files, generated reports, local caches, and conversation logs
should not be committed here.

Release history is tracked by git tags and GitHub Releases. Do not add
standalone changelog snapshots unless they are generated release artifacts.
