# Packaging

This directory owns release packaging, cross-compilation helpers, Docker build contexts, platform package resources, and release dependency metadata.

`dependencies.env` is the authoritative dependency source for maintained release automation. It records versions, archive names, URLs, and SHA-256 hashes for downloaded release inputs.

## Layout

| Path | Purpose |
| --- | --- |
| `notes/` | Platform notes copied into binary packages |
| `docker/` | Dockerfiles for reproducible cross-platform build images |
| `macos/` | macOS package resources |
| `scripts/` | Release packaging helpers |
| `dependencies.env` | Maintained dependency baseline and source archive hashes |

Supported packaging paths build this repository checkout through CMake. Third-party dependencies may use their own upstream build systems while they are being built as release dependencies.

Official release builds use `packaging/scripts/release-size-profile` to apply size-oriented compiler flags, per-function and per-data sections, and platform linker dead-code elimination. The profile is used by GitHub release jobs and Docker cross-build images so portable artifacts keep the maintained dependency baseline without retaining avoidable unused code.

GitHub Release assets are executable binaries named `aria2-next-<version>-<platform>-<architecture>`, plus a SHA-256 checksum file. Source code and license material are provided by the GitHub release tag source archives.

Release jobs must verify runtime dependency closure before packaging. Use `packaging/scripts/check-runtime-deps` on the final stripped binary so unintended compiler runtimes and third-party shared libraries cannot leak into release artifacts.

Release jobs also run `packaging/scripts/size-audit` on final binaries. This audit records size and dependency metadata for inspection, while runtime dependency closure remains the gating packaging check.

The release dependency boundary is platform-specific. Linux release binaries may use the system ELF loader, C/C++ runtime, and OpenSSL 3 runtime, while zlib, Expat, SQLite, c-ares, and libssh2 must be linked into the executable. macOS release binaries may link only Apple system libraries and frameworks at runtime; third-party dependencies must be linked into the executable. Windows release binaries use WinTLS and libssh2 WinCNG, may link only Windows system DLLs at runtime, and must not carry OpenSSL, third-party DLLs, or private CRT DLLs. Android release binaries may link only Android system runtime libraries and must not require `libc++_shared.so`.

Release jobs also run HTTPS dry-run smoke tests against the final binary. These checks cover startup, TLS backend selection, and default certificate verification before assets are uploaded.
