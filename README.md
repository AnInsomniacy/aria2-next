# aria2-next

[![CI](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml/badge.svg)](https://github.com/AnInsomniacy/aria2-next/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/AnInsomniacy/aria2-next.svg)](https://github.com/AnInsomniacy/aria2-next/releases)
[![License: GPLv2](https://img.shields.io/badge/license-GPLv2-blue.svg)](COPYING)

aria2-next is a maintained fork of [aria2](https://github.com/aria2/aria2), developed specifically as the `aria2c` sidecar engine for [Motrix Next](https://github.com/AnInsomniacy/motrix-next) while keeping the original aria2 format intact.

The protocols and integration surfaces have not been reinvented. The binary is still `aria2c`; existing CLI options, configuration files, session files, JSON-RPC methods, package metadata, and public libaria2 headers remain aria2-compatible, so other applications can use these builds directly.

This fork keeps that compatibility while carrying a large maintenance pass over upstream aria2. The review started from the upstream issue backlog of 1,000+ issues, was cleaned and triaged into a bug-focused set, and is preserved in [`maintenance/issue-review-matrix.csv`](maintenance/issue-review-matrix.csv). The maintained build path was also migrated from Autotools to CMake with reproducible release automation for macOS, Windows, Linux, and Android.

## What aria2-next Is

aria2-next is not a new downloader protocol or a Motrix-only binary format. It is aria2 with an actively maintained build, release, and reliability layer around it.

| Area | What this repository provides |
| --- | --- |
| Engine | aria2-compatible `aria2c` binaries |
| Primary consumer | Motrix Next native Tauri sidecar |
| External consumers | Existing aria2 scripts, frontends, RPC clients, and automation |
| Build system | CMake 3.25+ with Ninja presets |
| Release targets | macOS, Windows, and Linux on x64 and ARM64 |
| Additional packaging | Android ARM64 |
| Maintenance record | Preserved upstream issue review matrix |

## Compatibility

The project keeps aria2 compatibility as the default behavior.

| Surface | Compatibility |
| --- | --- |
| Executable | `aria2c` |
| CLI | aria2 option names and behavior |
| Configuration | aria2 config file format |
| Sessions | aria2 session and input file conventions |
| RPC | aria2 JSON-RPC methods and response shapes |
| Library | public libaria2 headers under `src/includes/aria2/` |

Motrix Next uses this project as its embedded engine, but the release artifacts are ordinary aria2-compatible builds. They can be used outside Motrix Next wherever aria2-compatible behavior is expected.

## Maintenance Audit

The upstream issue review is documented under [`maintenance/`](maintenance/). The durable audit artifact is [`maintenance/issue-review-matrix.csv`](maintenance/issue-review-matrix.csv).

Current preserved review data:

| Metric | Count |
| --- | ---: |
| Reviewed upstream open bug issues preserved in the matrix | 137 |
| Priority P1 rows | 98 |
| Priority P2 rows | 13 |
| Priority P3 rows | 26 |
| Rows with final state `fixed-verified` | 44 |
| Rows whose required action is `fixed-verified` | 37 |

The review covers DNS and IPv6, TLS and certificates, BitTorrent and DHT, filesystem and session handling, HTTP range and retry behavior, RPC and WebSocket behavior, core behavior, and build/test infrastructure.

Not every reviewed issue became a code change. The matrix keeps fixes, already-fixed reports, documented behavior, environment-specific reports, site-specific reports, platform-specific reports, non-reproducible reports, and larger architecture limitations separate so the maintenance history remains auditable.

## Build System Modernization

The maintained build path has moved from Autotools to CMake.

CMake now owns local development, tests, package metadata generation, and release packaging for this repository. Ninja is the default generator. The migration preserved the existing aria2 feature surface while making cross-platform release jobs easier to reproduce and validate.

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
build/default/aria2c --version
```

A plain Ninja build without presets is also supported:

```bash
cmake -S . -B build/default -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/default
ctest --test-dir build/default --output-on-failure
```

Useful CMake options include `ARIA2_ENABLE_BITTORRENT`, `ARIA2_ENABLE_METALINK`, `ARIA2_ENABLE_WEBSOCKET`, `ARIA2_ENABLE_LIBARIA2`, `ARIA2_ENABLE_STATIC`, `ARIA2_WITH_APPLETLS`, `ARIA2_WITH_WINTLS`, `ARIA2_WITH_OPENSSL`, `ARIA2_WITH_GNUTLS`, `ARIA2_WITH_LIBXML2`, `ARIA2_WITH_EXPAT`, `ARIA2_WITH_CARES`, `ARIA2_WITH_ZLIB`, `ARIA2_WITH_SQLITE3`, and `ARIA2_WITH_LIBSSH2`.

## Downloads

Prebuilt artifacts are published on the [GitHub Releases](https://github.com/AnInsomniacy/aria2-next/releases) page.

| Platform | Architecture | Artifact |
| --- | --- | --- |
| Linux | x86_64 | `aria2-<version>-linux-x86_64.tar.xz` |
| Linux | ARM64 | `aria2-<version>-linux-aarch64.tar.xz` |
| macOS | Apple Silicon | `aria2-<version>-macos-arm64.tar.bz2` |
| macOS | Intel | `aria2-<version>-macos-x86_64.tar.bz2` |
| Windows | x86_64 | `aria2-<version>-windows-x86_64.zip` |
| Windows | ARM64 | `aria2-<version>-windows-arm64.zip` |
| Checksums | all release assets | `aria2-<version>-checksums.sha256` |

Use the downloaded binary the same way as aria2:

```bash
aria2c https://example.com/file.iso
aria2c --enable-rpc --rpc-listen-all=false --rpc-listen-port=6800
```

## Release and Versioning

`CMakeLists.txt` is the project version source of truth. Release tags use `v{PROJECT_VERSION}`.

The release workflow runs when a matching GitHub Release is published. It validates the tag against `CMakeLists.txt`, builds all maintained platform artifacts, generates SHA-256 checksums, and uploads the assets to the published release.

Tag pushes alone do not publish release builds. `workflow_dispatch` remains available for manual release-path validation and uploads artifacts only to the workflow run.

## Dependency Baseline

Release dependency versions are tracked in [`packaging/dependencies.env`](packaging/dependencies.env).

| Dependency | Version | Release usage |
| --- | --- | --- |
| zlib | 1.3.2 | All release targets |
| Expat | 2.8.1 | Release targets using Expat |
| c-ares | 1.34.6 | Async DNS release targets |
| SQLite | 3.53.1 | Cookie storage release targets |
| libssh2 | 1.11.1 | SFTP release targets |
| OpenSSL | 3.5.6 LTS | Linux, Windows, Android, optional macOS |
| GMP | 6.3.0 | macOS and Windows dependency builds |
| libgpg-error | 1.61 | macOS dependency builds |
| libgcrypt | 1.12.2 | macOS dependency builds |
| Android NDK | r29 | Android release build |

## Repository Layout

| Path | Purpose |
| --- | --- |
| `CMakeLists.txt` | Root CMake build definition and project version source |
| `CMakePresets.json` | Standard configure, build, and test presets |
| `cmake/` | CMake templates, source inventories, and generated config inputs |
| `src/` | aria2 command-line client and core implementation |
| `src/includes/aria2/` | public libaria2 headers |
| `test/` | CppUnit test suite registered through CTest |
| `doc/` | manual, manpage, bash completion, and documentation tooling |
| `packaging/` | release dependencies, Dockerfiles, cross-build scripts, package assets |
| `third_party/` | vendored source with explicit ownership rules |
| `tools/` | repository helper scripts outside platform packaging |
| `maintenance/` | issue review records and CMake migration notes |

Directory-specific notes live in [`packaging/README.md`](packaging/README.md), [`tools/README.md`](tools/README.md), [`third_party/README.md`](third_party/README.md), and [`maintenance/README.md`](maintenance/README.md).

## License

Same as [aria2](https://github.com/aria2/aria2): [GPLv2](COPYING).
