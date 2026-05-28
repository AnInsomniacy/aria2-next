# Packaging

This directory owns release packaging, cross-compilation helpers, Docker build contexts, platform package resources, and release dependency metadata.

`dependencies.env` is the authoritative dependency source for maintained release automation. It records versions, archive names, URLs, and SHA-256 hashes for downloaded release inputs, including libtorrent-rasterbar and Boost for the maintained BitTorrent backend.

## Layout

| Path | Purpose |
| --- | --- |
| `notes/` | Platform notes copied into binary packages |
| `docker/` | Dockerfiles for reproducible cross-platform build images |
| `macos/` | macOS package resources |
| `scripts/` | Release packaging helpers |
| `dependencies.env` | Maintained dependency baseline and source archive hashes |

Supported packaging paths build this repository checkout through CMake. Third-party dependencies may use their own upstream build systems while they are being built as release dependencies. BitTorrent support is built through libtorrent-rasterbar only; release packaging does not build or ship the removed native BitTorrent engine.

Official release builds use `packaging/scripts/release-size-profile` to apply size-oriented compiler flags, per-function and per-data sections, and platform linker dead-code elimination. The profile is used by GitHub release jobs and Docker cross-build images so portable artifacts keep the maintained dependency baseline without retaining avoidable unused code. `packaging/scripts/common.sh` also owns the shared Boost header installation, libcurl c-ares resolver build, and static libtorrent-rasterbar build helper used by release jobs and Docker cross-build images. Release binaries default to `--dns-resolver=system`; `--dns-resolver=async` selects the libcurl c-ares resolver for ordinary URL transfers.

GitHub Release assets are standalone executable binaries named `aria2-next-<version>-<platform>-<architecture>`, plus a SHA-256 checksum file. Source code and license material are provided by the GitHub release tag source archives.

Release jobs must verify runtime dependency closure before packaging. Use `packaging/scripts/check-runtime-deps` on the final stripped binary so compiler runtimes and third-party shared libraries cannot leak into portable artifacts.

Release jobs also run `packaging/scripts/size-audit` on final binaries. This audit records size and dependency metadata for inspection, while runtime dependency closure remains the gating packaging check.

The release dependency boundary is platform-specific. Linux release binaries must be fully static ELF executables with no interpreter and no `NEEDED` shared libraries. macOS release binaries may link only Apple system libraries and frameworks at runtime; third-party dependencies must be linked into the executable. Windows release binaries may link only Windows system DLLs at runtime; third-party DLLs and private CRT DLLs are not allowed. Android release binaries may link only Android system runtime libraries and must not require `libc++_shared.so`.

Release jobs run final-binary loopback and hostname smoke tests, runtime dependency checks, and size audits before assets are uploaded. External network downloads are limited to verified dependency acquisition, not release smoke testing.

Manual release workflow runs accept `version=latest` and `build_profile=release|debug`. `latest` resolves to the current version declared in `CMakeLists.txt`. Official GitHub Releases always force the release profile. Manual debug runs use `RelWithDebInfo`, skip stripping, add a `-debug` artifact suffix, and upload Windows linker map files as workflow artifacts only.

Release checksum files are generated from the exact uploaded artifact names. Official releases upload `aria2-next-<version>-checksums.sha256`; manual debug validation uploads `aria2-next-<version>-debug-checksums.sha256` only to the workflow run.

See [`../docs/RELEASE.md`](../docs/RELEASE.md) for maintainer release steps, debug workflow behavior, checksum verification, and release recovery rules.
