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

GitHub Release assets are standalone executable binaries named `aria2-next-<version>-<platform>-<architecture>`, plus a SHA-256 checksum file. Source code and license material are provided by the GitHub release tag source archives.

Release jobs must verify runtime dependency closure before packaging. Use `packaging/scripts/check-runtime-deps` on the final stripped binary so compiler runtimes and third-party shared libraries cannot leak into portable artifacts.

The release dependency boundary is platform-specific. Linux release binaries must be fully static ELF executables with no interpreter and no `NEEDED` shared libraries. macOS release binaries may link only Apple system libraries and frameworks at runtime; third-party dependencies must be linked into the executable. Windows release binaries may link only Windows system DLLs at runtime; third-party DLLs and private CRT DLLs are not allowed. Android release binaries may link only Android system runtime libraries and must not require `libc++_shared.so`.

Release jobs also run HTTPS dry-run smoke tests against the final binary. These checks cover startup, TLS backend selection, and default certificate verification before assets are uploaded.
