# Packaging

This directory owns release packaging, cross-compilation helpers, Docker build contexts, platform package resources, and release dependency metadata.

`dependencies.env` is the authoritative dependency version source for maintained release automation. Update it before changing dependency versions in scripts, Dockerfiles, workflow files, or release documentation.

## Layout

| Path | Purpose |
|------|---------|
| `docs/` | Platform-specific notes copied into binary packages |
| `docker/` | Maintained Dockerfiles for reproducible cross-platform build images |
| `legacy/` | Unsupported historical packaging retained only for reference |
| `macos/` | macOS package resources |
| `scripts/` | Maintained release packaging helpers |

Supported packaging paths must build this repository checkout through CMake. They should not clone upstream aria2 during a release build.

Third-party dependencies may use their own upstream build systems while they are being built as release dependencies. That does not make those build systems supported for aria2-next itself.

Unsupported platform experiments belong in `legacy/` with an explicit note describing why they are not part of the maintained release surface.

Binary packages should include `README.md`, license files, and the relevant platform note from `docs/`. They should not include generated changelog snapshots; release history lives in git and GitHub Releases.
